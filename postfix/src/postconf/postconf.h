/*++
/* NAME
/*	postconf 3h
/* SUMMARY
/*	module interfaces
/* SYNOPSIS
/*	#include <postconf.h>
/* DESCRIPTION
/* .nf

 /*
  * System library.
  */
#include <unistd.h>

 /*
  * Utility library.
  */
#include <htable.h>
#include <argv.h>
#include <dict.h>
#include <name_code.h>

 /*
  * What we're supposed to be doing.
  */
#define SHOW_NONDEF	(1<<0)		/* show main.cf non-default settings */
#define SHOW_DEFS	(1<<1)		/* show main.cf default setting */
#define HIDE_NAME	(1<<2)		/* hide main.cf parameter name */
#define SHOW_MAPS	(1<<3)		/* show map types */
#define EDIT_CONF	(1<<4)		/* edit main.cf or master.cf */
#define SHOW_LOCKS	(1<<5)		/* show mailbox lock methods */
#define SHOW_EVAL	(1<<6)		/* expand main.cf right-hand sides */
#define SHOW_SASL_SERV	(1<<7)		/* show server auth plugin types */
#define SHOW_SASL_CLNT	(1<<8)		/* show client auth plugin types */
#define COMMENT_OUT	(1<<9)		/* #-out selected main.cf entries */
#define MASTER_ENTRY	(1<<10)		/* manage master.cf entries */
#define FOLD_LINE	(1<<11)		/* fold long *.cf entries */
#define EDIT_EXCL	(1<<12)		/* exclude main.cf entries */
#define MASTER_FIELD	(1<<13)		/* hierarchical pathname */
#define MAIN_PARAM	(1<<14)		/* manage main.cf entries */
#define EXP_DSN_TEMPL	(1<<15)		/* expand bounce templates */
#define PARAM_CLASS	(1<<16)		/* select parameter class */
#define MAIN_OVER	(1<<17)		/* override parameter values */
#define DUMP_DSN_TEMPL	(1<<18)		/* show bounce templates */
#define MASTER_PARAM	(1<<19)		/* manage master.cf -o name=value */

#define DEF_MODE	0

 /*
  * Structure for one "valid parameter" (built-in, service-defined or valid
  * user-defined). See the postconf_builtin, postconf_service and
  * postconf_user modules for narrative text.
  */
typedef struct {
    int     flags;			/* see below */
    char   *param_data;			/* mostly, the default value */
    const char *(*convert_fn) (char *);	/* value to string */
} PC_PARAM_NODE;

 /* Values for flags. See the postconf_node module for narrative text. */
#define PC_PARAM_FLAG_RAW	(1<<0)	/* raw parameter value */
#define PC_PARAM_FLAG_BUILTIN	(1<<1)	/* built-in parameter name */
#define PC_PARAM_FLAG_SERVICE	(1<<2)	/* service-defined parameter name */
#define PC_PARAM_FLAG_USER	(1<<3)	/* user-defined parameter name */
#define PC_PARAM_FLAG_LEGACY	(1<<4)	/* legacy parameter name */
#define PC_PARAM_FLAG_READONLY	(1<<5)	/* legacy parameter name */
#define PC_PARAM_FLAG_DBMS	(1<<6)	/* dbms-defined parameter name */

#define PC_PARAM_MASK_CLASS \
	(PC_PARAM_FLAG_BUILTIN | PC_PARAM_FLAG_SERVICE | PC_PARAM_FLAG_USER)
#define PC_PARAM_CLASS_OVERRIDE(node, class) \
	((node)->flags = (((node)->flags & ~PC_PARAM_MASK_CLASS) | (class)))

#define PC_RAW_PARAMETER(node) ((node)->flags & PC_PARAM_FLAG_RAW)
#define PC_LEGACY_PARAMETER(node) ((node)->flags & PC_PARAM_FLAG_LEGACY)
#define PC_READONLY_PARAMETER(node) ((node)->flags & PC_PARAM_FLAG_READONLY)
#define PC_DBMS_PARAMETER(node) ((node)->flags & PC_PARAM_FLAG_DBMS)

 /* Values for param_data. See postconf_node module for narrative text. */
#define PC_PARAM_NO_DATA	((char *) 0)

 /*
  * Lookup table for global "valid parameter" information.
  */
#define PC_PARAM_TABLE			HTABLE
#define PC_PARAM_INFO			HTABLE_INFO

extern PC_PARAM_TABLE *param_table;

 /*
  * postconf_node.c.
  */
#define PC_PARAM_TABLE_CREATE(size)	htable_create(size);
#define PC_PARAM_NODE_CAST(ptr)		((PC_PARAM_NODE *) (ptr))

#define PC_PARAM_TABLE_LIST(table)	htable_list(table)
#define PC_PARAM_INFO_NAME(ht)		((const char *) (ht)->key)
#define PC_PARAM_INFO_NODE(ht)		PC_PARAM_NODE_CAST((ht)->value)

#define PC_PARAM_TABLE_FIND(table, name) \
	PC_PARAM_NODE_CAST(htable_find((table), (name)))
#define PC_PARAM_TABLE_LOCATE(table, name) htable_locate((table), (name))
#define PC_PARAM_TABLE_ENTER(table, name, flags, data, func) \
	htable_enter((table), (name), (char *) make_param_node((flags), \
	    (data), (func)))

PC_PARAM_NODE *make_param_node(int, char *, const char *(*) (char *));
const char *convert_param_node(int, const char *, PC_PARAM_NODE *);
extern VSTRING *param_string_buf;

 /*
  * Structure of one master.cf entry.
  */
typedef struct {
    char   *name_space;			/* service/type, parameter name space */
    ARGV   *argv;			/* null, or master.cf fields */
    DICT   *all_params;			/* null, or all name=value entries */
    HTABLE *valid_names;		/* null, or "valid" parameter names */
} PC_MASTER_ENT;

#define PC_MASTER_MIN_FIELDS	8	/* mandatory field count */

#define PC_MASTER_NAME_SERVICE	"service"
#define PC_MASTER_NAME_TYPE	"type"
#define PC_MASTER_NAME_PRIVATE	"private"
#define PC_MASTER_NAME_UNPRIV	"unprivileged"
#define PC_MASTER_NAME_CHROOT	"chroot"
#define PC_MASTER_NAME_WAKEUP	"wakeup"
#define PC_MASTER_NAME_MAXPROC	"process_limit"
#define PC_MASTER_NAME_CMD	"command"

#define PC_MASTER_FIELD_SERVICE	0	/* service name */
#define PC_MASTER_FIELD_TYPE	1	/* service type */
#define PC_MASTER_FIELD_PRIVATE	2	/* private service */
#define PC_MASTER_FIELD_UNPRIV	3	/* unprivileged service */
#define PC_MASTER_FIELD_CHROOT	4	/* chrooted service */
#define PC_MASTER_FIELD_WAKEUP	5	/* wakeup timer */
#define PC_MASTER_FIELD_MAXPROC	6	/* process limit */
#define PC_MASTER_FIELD_CMD	7	/* command */

#define PC_MASTER_FIELD_WILDC	-1	/* wild-card */
#define PC_MASTER_FIELD_NONE	-2	/* not available
					 * 
					/* Lookup table for master.cf
					 * entries. The table is terminated
					 * with an entry that has a null argv
					 * member. */
PC_MASTER_ENT *master_table;

 /*
  * Line-wrapping support.
  */
#define LINE_LIMIT	80		/* try to fold longer lines */
#define SEPARATORS	" \t\r\n"
#define INDENT_LEN	4		/* indent long text by 4 */
#define INDENT_TEXT	"    "

 /*
  * XXX Global so that postconf_builtin.c call-backs can see it.
  */
extern int cmd_mode;

 /*
  * postconf_misc.c.
  */
extern void set_config_dir(void);

 /*
  * postconf_main.c
  */
extern void read_parameters(void);
extern void set_parameters(char **);
extern void show_parameters(VSTREAM *, int, int, char **);

 /*
  * postconf_edit.c
  */
extern void edit_main(int, int, char **);
extern void edit_master(int, int, char **);

 /*
  * postconf_master.c.
  */
extern const char daemon_options_expecting_value[];
extern void read_master(int);
extern void show_master_entries(VSTREAM *, int, int, char **);
extern const char *parse_master_entry(PC_MASTER_ENT *, const char *);
extern void print_master_entry(VSTREAM *, int, PC_MASTER_ENT *);
extern void free_master_entry(PC_MASTER_ENT *);
extern void show_master_fields(VSTREAM *, int, int, char **);
extern void edit_master_field(PC_MASTER_ENT *, int, const char *);
extern void show_master_params(VSTREAM *, int, int, char **);
extern void edit_master_param(PC_MASTER_ENT *, int, const char *, const char *);

#define WARN_ON_OPEN_ERROR	0
#define FAIL_ON_OPEN_ERROR	1

#define PC_MASTER_BLANKS	" \t\r\n"	/* XXX */

 /*
  * Master.cf parameter namespace management. The idea is to manage master.cf
  * "-o name=value" settings with other tools than text editors.
  * 
  * The natural choice is to use "service-name.service-type.parameter-name", but
  * unfortunately the '.' may appear in service and parameter names.
  * 
  * For example, a spawn(8) listener can have a service name 127.0.0.1:10028.
  * This service name becomes part of a service-dependent parameter name
  * "127.0.0.1:10028_time_limit". All those '.' characters mean we can't use
  * '.' as the parameter namespace delimiter.
  * 
  * (We could require that such service names are specified as $foo:port with
  * the value of "foo" defined in main.cf or at the top of master.cf.)
  * 
  * But it is easier if we use '/' instead.
  */
#define PC_NAMESP_SEP_CH	'/'
#define PC_NAMESP_SEP_STR	"/"

#define PC_LEGACY_SEP_CH	'.'

 /*
  * postconf_match.c.
  */
#define PC_MATCH_WILDC_STR	"*"
#define PC_MATCH_ANY(p)		((p)[0] == PC_MATCH_WILDC_STR[0] && (p)[1] == 0)
#define PC_MATCH_STRING(p, s)	(PC_MATCH_ANY(p) || strcmp((p), (s)) == 0)

extern ARGV *parse_service_pattern(const char *, int, int);
extern int parse_field_pattern(const char *);

#define IS_MAGIC_SERVICE_PATTERN(pat) \
    (PC_MATCH_ANY((pat)->argv[0]) || PC_MATCH_ANY((pat)->argv[1]))
#define MATCH_SERVICE_PATTERN(pat, name, type) \
    (PC_MATCH_STRING((pat)->argv[0], (name)) \
	&& PC_MATCH_STRING((pat)->argv[1], (type)))

#define is_magic_field_pattern(pat) ((pat) == PC_MASTER_FIELD_WILDC)
#define str_field_pattern(pat) ((const char *) (field_name_offset[pat].name))

#define IS_MAGIC_PARAM_PATTERN(pat) PC_MATCH_ANY(pat)
#define MATCH_PARAM_PATTERN(pat, name) PC_MATCH_STRING((pat), (name))

/* The following is not part of the postconf_match API. */
extern NAME_CODE field_name_offset[];

 /*
  * postconf_builtin.c.
  */
extern void register_builtin_parameters(const char *, pid_t);

 /*
  * postconf_service.c.
  */
extern void register_service_parameters(void);

 /*
  * Parameter context structure.
  */
typedef struct {
    PC_MASTER_ENT *local_scope;
    int     param_class;
} PC_PARAM_CTX;

 /*
  * postconf_user.c.
  */
extern void register_user_parameters(void);

 /*
  * postconf_dbms.c
  */
extern void register_dbms_parameters(const char *,
	               const char *(*) (const char *, int, PC_MASTER_ENT *),
				             PC_MASTER_ENT *);

 /*
  * postconf_lookup.c.
  */
const char *lookup_parameter_value(int, const char *, PC_MASTER_ENT *,
				           PC_PARAM_NODE *);

char   *expand_parameter_value(VSTRING *, int, const char *, PC_MASTER_ENT *);

 /*
  * postconf_print.c.
  */
extern void print_line(VSTREAM *, int, const char *,...);

 /*
  * postconf_unused.c.
  */
extern void flag_unused_main_parameters(void);
extern void flag_unused_master_parameters(void);

 /*
  * postconf_other.c.
  */
extern void show_maps(void);
extern void show_locks(void);
extern void show_sasl(int);

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/
