/*++
/* NAME
/*	postconf_master 3
/* SUMMARY
/*	support for master.cf
/* SYNOPSIS
/*	#include <postconf.h>
/*
/*	const char daemon_options_expecting_value[];
/*
/*	void	read_master(fail_on_open)
/*	int	fail_on_open;
/*
/*	void	show_master_entries(fp, mode, service_filters)
/*	VSTREAM	*fp;
/*	int	mode;
/*	char	**service_filters;
/*
/*	void	show_master_fields(fp, mode, n_filters, field_filters)
/*	VSTREAM	*fp;
/*	int	mode;
/*	int	n_filters;
/*	char	**field_filters;
/*
/*	void	edit_master_field(masterp, field, new_value)
/*	PC_MASTER_ENT *masterp;
/*	int	field;
/*	const char *new_value;
/*
/*	void	show_master_params(fp, mode, argc, **param_filters)
/*	VSTREAM	*fp;
/*	int	mode;
/*	int	argc;
/*	char	**param_filters;
/*
/*	void	edit_master_param(masterp, mode, param_name, param_value)
/*	PC_MASTER_ENT *masterp;
/*	int	mode;
/*	const char *param_name;
/*	const char *param_value;
/* AUXILIARY FUNCTIONS
/*	const char *parse_master_entry(masterp, buf)
/*	PC_MASTER_ENT *masterp;
/*	const char *buf;
/*
/*	void	print_master_entry(fp, mode, masterp)
/*	VSTREAM *fp;
/*	int mode;
/*	PC_MASTER_ENT *masterp;
/*
/*	void	free_master_entry(masterp)
/*	PC_MASTER_ENT *masterp;
/* DESCRIPTION
/*	read_master() reads entries from master.cf into memory.
/*
/*	show_master_entries() writes the entries in the master.cf
/*	file to the specified stream.
/*
/*	show_master_fields() writes name/type/field=value records to
/*	the specified stream.
/*
/*	edit_master_field() updates the value of a single-column
/*	or multi-column attribute.
/*
/*	show_master_params() writes name/type/parameter=value records
/*	to the specified stream.
/*
/*	edit_master_param() updates, removes or adds the named
/*	parameter in a master.cf entry (the remove request ignores
/*	the parameter value).
/*
/*	daemon_options_expecting_value[] is an array of master.cf
/*	daemon command-line options that expect an option value.
/*
/*	parse_master_entry() parses a (perhaps multi-line) string
/*	that contains a complete master.cf entry, and normalizes
/*	daemon command-line options to simplify further handling.
/*
/*	print_master_entry() prints a parsed master.cf entry.
/*
/*	free_master_entry() returns storage to the heap that was
/*	allocated by parse_master_entry().
/*
/*	Arguments
/* .IP fail_on_open
/*	Specify FAIL_ON_OPEN if open failure is a fatal error,
/*	WARN_ON_OPEN if a warning should be logged instead.
/* .IP fp
/*	Output stream.
/* .IP mode
/*	Bit-wise OR of flags. Flags other than the following are ignored.
/* .RS
/* .IP FOLD_LINE
/*	Wrap long output lines.
/* .IP SHOW_EVAL
/*	Expand $name in parameter values.
/* .IP EDIT_EXCL
/*	Request that edit_master_param() removes the parameter.
/* .RE
/* .IP n_filters
/*	The number of command-line filters.
/* .IP field_filters
/*	A list of zero or more service field patterns (name/type/field).
/*	The output is formatted as "name/type/field = value".  If
/*	no filters are specified, show_master_fields() outputs the
/*	fields of all master.cf entries in the specified order.
/* .IP param_filters
/*	A list of zero or more service parameter patterns
/*	(name/type/parameter).  The output is formatted as
/*	"name/type/parameter = value".  If no filters are specified,
/*	show_master_params() outputs the parameters of all master.cf
/*	entries in sorted order.
/* .IP service_filters
/*	A list of zero or more service patterns (name or name/type).
/*	If no filters are specified, show_master_entries() outputs
/*	all master.cf entries in the specified order.
/* .IP field
/*	Index into parsed master.cf entry.
/* .IP new_value
/*	Replacement value for the specified field. It is split
/*	in whitespace in case of a multi-field attribute.
/* DIAGNOSTICS
/*	Problems are reported to the standard error stream.
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

/* System library. */

#include <sys_defs.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <vstring.h>
#include <argv.h>
#include <vstream.h>
#include <readlline.h>
#include <stringops.h>
#include <split_at.h>

/* Global library. */

#include <mail_params.h>

/* Master library. */

#include <master_proto.h>

/* Application-specific. */

#include <postconf.h>

const char daemon_options_expecting_value[] = "o";

 /*
  * Data structure to capture a command-line service field filter.
  */
typedef struct {
    int     match_count;		/* hit count */
    const char *raw_text;		/* full pattern text */
    ARGV   *service_pattern;		/* parsed service name, type, ... */
    int     field_pattern;		/* parsed field pattern */
    const char *param_pattern;		/* parameter pattern */
} PC_MASTER_FIELD_REQ;

 /*
  * Valid inputs.
  */
static const char *valid_master_types[] = {
    MASTER_XPORT_NAME_UNIX,
    MASTER_XPORT_NAME_FIFO,
    MASTER_XPORT_NAME_INET,
    MASTER_XPORT_NAME_PASS,
    0,
};

static const char valid_bool_types[] = "yn-";

#define STR(x) vstring_str(x)

/* normalize_options - bring options into canonical form */

static void normalize_options(ARGV *argv)
{
    int     field;
    char   *arg;
    char   *cp;
    char   *junk;

    /*
     * Normalize options to simplify later processing.
     */
    for (field = PC_MASTER_MIN_FIELDS; argv->argv[field] != 0; field++) {
	arg = argv->argv[field];
	if (arg[0] != '-' || strcmp(arg, "--") == 0)
	    break;
	for (cp = arg + 1; *cp; cp++) {
	    if (strchr(daemon_options_expecting_value, *cp) != 0
		&& cp > arg + 1) {
		/* Split "-stuffozz" into "-stuff" and "-ozz". */
		junk = concatenate("-", cp, (char *) 0);
		argv_insert_one(argv, field + 1, junk);
		myfree(junk);
		*cp = 0;			/* XXX argv_replace_one() */
		break;
	    }
	}
	if (strchr(daemon_options_expecting_value, arg[1]) == 0)
	    /* Option requires no value. */
	    continue;
	if (arg[2] != 0) {
	    /* Split "-oname=value" into "-o" "name=value". */
	    argv_insert_one(argv, field + 1, arg + 2);
	    arg[2] = 0;				/* XXX argv_replace_one() */
	    field += 1;
	} else if (argv->argv[field + 1] != 0) {
	    /* Already in "-o" "name=value" form. */
	    field += 1;
	}
    }
}

/* fix_fatal - fix multiline text before release */

static NORETURN PRINTFLIKE(1, 2) fix_fatal(const char *fmt,...)
{
    VSTRING *buf = vstring_alloc(100);
    va_list ap;

    /*
     * Replace newline with whitespace.
     */
    va_start(ap, fmt);
    vstring_vsprintf(buf, fmt, ap);
    va_end(ap);
    translit(STR(buf), "\n", " ");
    msg_fatal("%s", STR(buf));
    /* NOTREACHED */
}

/* check_master_entry - sanity check master.cf entry */

static void check_master_entry(ARGV *argv, const char *raw_text)
{
    const char **cpp;
    char   *cp;
    int     len;
    int     field;

    cp = argv->argv[PC_MASTER_FIELD_TYPE];
    for (cpp = valid_master_types; /* see below */ ; cpp++) {
	if (*cpp == 0)
	    fix_fatal("invalid " PC_MASTER_NAME_TYPE " field \"%s\" in \"%s\"",
		      cp, raw_text);
	if (strcmp(*cpp, cp) == 0)
	    break;
    }

    for (field = PC_MASTER_FIELD_PRIVATE; field <= PC_MASTER_FIELD_CHROOT; field++) {
	cp = argv->argv[field];
	if (cp[1] != 0 || strchr(valid_bool_types, *cp) == 0)
	    fix_fatal("invalid %s field \%s\" in \"%s\"",
		      str_field_pattern(field), cp, raw_text);
    }

    cp = argv->argv[PC_MASTER_FIELD_WAKEUP];
    len = strlen(cp);
    if (len > 0 && cp[len - 1] == '?')
	len--;
    if (!(cp[0] == '-' && len == 1) && strspn(cp, "0123456789") != len)
	fix_fatal("invalid " PC_MASTER_NAME_WAKEUP " field \%s\" in \"%s\"",
		  cp, raw_text);

    cp = argv->argv[PC_MASTER_FIELD_MAXPROC];
    if (strcmp("-", cp) != 0 && cp[strspn(cp, "0123456789")] != 0)
	fix_fatal("invalid " PC_MASTER_NAME_MAXPROC " field \%s\" in \"%s\"",
		  cp, raw_text);
}

/* free_master_entry - destroy parsed entry */

void    free_master_entry(PC_MASTER_ENT *masterp)
{
    /* XX Fixme: allocation/deallocation asymmetry. */
    myfree(masterp->name_space);
    argv_free(masterp->argv);
    if (masterp->valid_names)
	htable_free(masterp->valid_names, myfree);
    if (masterp->all_params)
	dict_free(masterp->all_params);
    myfree((char *) masterp);
}

/* parse_master_entry - parse one master line */

const char *parse_master_entry(PC_MASTER_ENT *masterp, const char *buf)
{
    ARGV   *argv;

    /*
     * We can't use the master daemon's master_ent routines in their current
     * form. They convert everything to internal form, and they skip disabled
     * services.
     * 
     * The postconf command needs to show default fields as "-", and needs to
     * know about all service names so that it can generate service-dependent
     * parameter names (transport-dependent etc.).
     * 
     * XXX Do per-field sanity checks.
     */
    argv = argv_split(buf, PC_MASTER_BLANKS);
    if (argv->argc < PC_MASTER_MIN_FIELDS) {
	argv_free(argv);			/* Coverity 201311 */
	return ("bad field count");
    }
    check_master_entry(argv, buf);
    normalize_options(argv);
    masterp->name_space =
	concatenate(argv->argv[0], PC_NAMESP_SEP_STR, argv->argv[1], (char *) 0);
    masterp->argv = argv;
    masterp->valid_names = 0;
    masterp->all_params = 0;
    return (0);
}

/* read_master - read and digest the master.cf file */

void    read_master(int fail_on_open_error)
{
    const char *myname = "read_master";
    char   *path;
    VSTRING *buf;
    VSTREAM *fp;
    const char *err;
    int     entry_count = 0;
    int     line_count = 0;

    /*
     * Sanity check.
     */
    if (master_table != 0)
	msg_panic("%s: master table is already initialized", myname);

    /*
     * Get the location of master.cf.
     */
    if (var_config_dir == 0)
	set_config_dir();
    path = concatenate(var_config_dir, "/", MASTER_CONF_FILE, (char *) 0);

    /*
     * Initialize the in-memory master table.
     */
    master_table = (PC_MASTER_ENT *) mymalloc(sizeof(*master_table));

    /*
     * Skip blank lines and comment lines. Degrade gracefully if master.cf is
     * not available, and master.cf is not the primary target.
     */
    if ((fp = vstream_fopen(path, O_RDONLY, 0)) == 0) {
	if (fail_on_open_error)
	    msg_fatal("open %s: %m", path);
	msg_warn("open %s: %m", path);
    } else {
	buf = vstring_alloc(100);
	while (readlline(buf, fp, &line_count) != 0) {
	    master_table = (PC_MASTER_ENT *) myrealloc((char *) master_table,
				 (entry_count + 2) * sizeof(*master_table));
	    if ((err = parse_master_entry(master_table + entry_count,
					  STR(buf))) != 0)
		msg_fatal("file %s: line %d: %s", path, line_count, err);
	    entry_count += 1;
	}
	vstream_fclose(fp);
	vstring_free(buf);
    }

    /*
     * Null-terminate the master table and clean up.
     */
    master_table[entry_count].argv = 0;
    myfree(path);
}

/* print_master_entry - print one master line */

void    print_master_entry(VSTREAM *fp, int mode, PC_MASTER_ENT *masterp)
{
    char  **argv = masterp->argv->argv;
    const char *arg;
    const char *aval;
    int     arg_len;
    int     line_len;
    int     field;
    int     in_daemon_options;
    static int column_goal[] = {
	0,				/* service */
	11,				/* type */
	17,				/* private */
	25,				/* unpriv */
	33,				/* chroot */
	41,				/* wakeup */
	49,				/* maxproc */
	57,				/* command */
    };

#define ADD_TEXT(text, len) do { \
        vstream_fputs(text, fp); line_len += len; } \
    while (0)
#define ADD_SPACE ADD_TEXT(" ", 1)

    /*
     * Show the standard fields at their preferred column position. Use at
     * least one-space column separation.
     */
    for (line_len = 0, field = 0; field < PC_MASTER_MIN_FIELDS; field++) {
	arg = argv[field];
	if (line_len > 0) {
	    do {
		ADD_SPACE;
	    } while (line_len < column_goal[field]);
	}
	ADD_TEXT(arg, strlen(arg));
    }

    /*
     * Format the daemon command-line options and non-option arguments. Here,
     * we have no data-dependent preference for column positions, but we do
     * have argument grouping preferences.
     */
    in_daemon_options = 1;
    for ( /* void */ ; (arg = argv[field]) != 0; field++) {
	arg_len = strlen(arg);
	aval = 0;
	if (in_daemon_options) {

	    /*
	     * Try to show the generic options (-v -D) on the first line, and
	     * non-options on a later line.
	     */
	    if (arg[0] != '-' || strcmp(arg, "--") == 0) {
		in_daemon_options = 0;
#if 0
		if (mode & FOLD_LINE)
		    /* Force line wrap. */
		    line_len = LINE_LIMIT;
#endif
	    }

	    /*
	     * Special processing for options that require a value.
	     */
	    else if (strchr(daemon_options_expecting_value, arg[1]) != 0
		     && (aval = argv[field + 1]) != 0) {

		/* Force line wrap before option with value. */
		line_len = LINE_LIMIT;

		/*
		 * Optionally, expand $name in parameter value.
		 */
		if (strcmp(arg, "-o") == 0
		    && (mode & SHOW_EVAL) != 0)
		    aval = expand_parameter_value((VSTRING *) 0, mode,
						  aval, masterp);

		/*
		 * Keep option and value on the same line.
		 */
		arg_len += strlen(aval) + 1;
	    }
	}

	/*
	 * Insert a line break when the next item won't fit.
	 */
	if (line_len > INDENT_LEN) {
	    if ((mode & FOLD_LINE) == 0
		|| line_len + 1 + arg_len < LINE_LIMIT) {
		ADD_SPACE;
	    } else {
		vstream_fputs("\n" INDENT_TEXT, fp);
		line_len = INDENT_LEN;
	    }
	}
	ADD_TEXT(arg, strlen(arg));
	if (aval) {
	    ADD_SPACE;
	    ADD_TEXT(aval, strlen(aval));
	    field += 1;

	    /* Force line wrap after option with value. */
	    line_len = LINE_LIMIT;

	}
    }
    vstream_fputs("\n", fp);

    if (msg_verbose)
	vstream_fflush(fp);
}

/* show_master_entries - show master.cf entries */

void    show_master_entries(VSTREAM *fp, int mode, int argc, char **argv)
{
    PC_MASTER_ENT *masterp;
    PC_MASTER_FIELD_REQ *field_reqs;
    PC_MASTER_FIELD_REQ *req;

    /*
     * Parse the filter expressions.
     */
    if (argc > 0) {
	field_reqs = (PC_MASTER_FIELD_REQ *)
	    mymalloc(sizeof(*field_reqs) * argc);
	for (req = field_reqs; req < field_reqs + argc; req++) {
	    req->match_count = 0;
	    req->raw_text = *argv++;
	    req->service_pattern =
		parse_service_pattern(req->raw_text, 1, 2);
	    if (req->service_pattern == 0)
		msg_fatal("-M option requires service_name[/type]");
	}
    }

    /*
     * Iterate over the master table.
     */
    for (masterp = master_table; masterp->argv != 0; masterp++) {
	if (argc > 0) {
	    for (req = field_reqs; req < field_reqs + argc; req++) {
		if (MATCH_SERVICE_PATTERN(req->service_pattern,
					  masterp->argv->argv[0],
					  masterp->argv->argv[1])) {
		    req->match_count++;
		    print_master_entry(fp, mode, masterp);
		}
	    }
	} else {
	    print_master_entry(fp, mode, masterp);
	}
    }

    /*
     * Cleanup.
     */
    if (argc > 0) {
	for (req = field_reqs; req < field_reqs + argc; req++) {
	    if (req->match_count == 0)
		msg_warn("unmatched request: \"%s\"", req->raw_text);
	    argv_free(req->service_pattern);
	}
	myfree((char *) field_reqs);
    }
}

/* print_master_field - scaffolding */

static void print_master_field(VSTREAM *fp, int mode,
			               PC_MASTER_ENT *masterp,
			               int field)
{
    char  **argv = masterp->argv->argv;
    const char *arg;
    const char *aval;
    int     arg_len;
    int     line_len;
    int     in_daemon_options;

    /*
     * Show the field value, or the first value in the case of a multi-column
     * field.
     */
#define ADD_CHAR(ch) ADD_TEXT((ch), 1)

    line_len = 0;
    if ((mode & HIDE_NAME) == 0) {
	ADD_TEXT(argv[0], strlen(argv[0]));
	ADD_CHAR(PC_NAMESP_SEP_STR);
	ADD_TEXT(argv[1], strlen(argv[1]));
	ADD_CHAR(PC_NAMESP_SEP_STR);
	ADD_TEXT(str_field_pattern(field), strlen(str_field_pattern(field)));
	ADD_TEXT(" = ", 3);
	if (line_len + strlen(argv[field]) > LINE_LIMIT) {
	    vstream_fputs("\n" INDENT_TEXT, fp);
	    line_len = INDENT_LEN;
	}
    }
    ADD_TEXT(argv[field], strlen(argv[field]));

    /*
     * Format the daemon command-line options and non-option arguments. Here,
     * we have no data-dependent preference for column positions, but we do
     * have argument grouping preferences.
     */
    if (field == PC_MASTER_FIELD_CMD) {
	in_daemon_options = 1;
	for (field += 1; (arg = argv[field]) != 0; field++) {
	    arg_len = strlen(arg);
	    aval = 0;
	    if (in_daemon_options) {

		/*
		 * We make no special case for generic options (-v -D)
		 * options.
		 */
		if (arg[0] != '-' || strcmp(arg, "--") == 0) {
		    in_daemon_options = 0;
		} else if (strchr(daemon_options_expecting_value, arg[1]) != 0
			   && (aval = argv[field + 1]) != 0) {

		    /* Force line break before option with value. */
		    line_len = LINE_LIMIT;

		    /*
		     * Optionally, expand $name in parameter value.
		     */
		    if (strcmp(arg, "-o") == 0
			&& (mode & SHOW_EVAL) != 0)
			aval = expand_parameter_value((VSTRING *) 0, mode,
						      aval, masterp);

		    /*
		     * Keep option and value on the same line.
		     */
		    arg_len += strlen(aval) + 1;
		}
	    }

	    /*
	     * Insert a line break when the next item won't fit.
	     */
	    if (line_len > INDENT_LEN) {
		if ((mode & FOLD_LINE) == 0
		    || line_len + 1 + arg_len < LINE_LIMIT) {
		    ADD_SPACE;
		} else {
		    vstream_fputs("\n" INDENT_TEXT, fp);
		    line_len = INDENT_LEN;
		}
	    }
	    ADD_TEXT(arg, strlen(arg));
	    if (aval) {
		ADD_SPACE;
		ADD_TEXT(aval, strlen(aval));
		field += 1;

		/* Force line break after option with value. */
		line_len = LINE_LIMIT;
	    }
	}
    }
    vstream_fputs("\n", fp);

    if (msg_verbose)
	vstream_fflush(fp);
}

/* show_master_fields - show master.cf fields */

void    show_master_fields(VSTREAM *fp, int mode, int argc, char **argv)
{
    const char *myname = "show_master_fields";
    PC_MASTER_ENT *masterp;
    PC_MASTER_FIELD_REQ *field_reqs;
    PC_MASTER_FIELD_REQ *req;
    int     field;

    /*
     * Parse the filter expressions.
     */
    if (argc > 0) {
	field_reqs = (PC_MASTER_FIELD_REQ *)
	    mymalloc(sizeof(*field_reqs) * argc);
	for (req = field_reqs; req < field_reqs + argc; req++) {
	    req->match_count = 0;
	    req->raw_text = *argv++;
	    req->service_pattern =
		parse_service_pattern(req->raw_text, 1, 3);
	    if (req->service_pattern == 0)
		msg_fatal("-F option requires service_name[/type[/field]]");
	    field = req->field_pattern =
		parse_field_pattern(req->service_pattern->argv[2]);
	    if (is_magic_field_pattern(field) == 0
		&& (field < 0 || field > PC_MASTER_FIELD_CMD))
		msg_panic("%s: bad attribute field index: %d",
			  myname, field);
	}
    }

    /*
     * Iterate over the master table.
     */
    for (masterp = master_table; masterp->argv != 0; masterp++) {
	if (argc > 0) {
	    for (req = field_reqs; req < field_reqs + argc; req++) {
		if (MATCH_SERVICE_PATTERN(req->service_pattern,
					  masterp->argv->argv[0],
					  masterp->argv->argv[1])) {
		    req->match_count++;
		    field = req->field_pattern;
		    if (is_magic_field_pattern(field)) {
			for (field = 0; field <= PC_MASTER_FIELD_CMD; field++)
			    print_master_field(fp, mode, masterp, field);
		    } else {
			print_master_field(fp, mode, masterp, field);
		    }
		}
	    }
	} else {
	    for (field = 0; field <= PC_MASTER_FIELD_CMD; field++)
		print_master_field(fp, mode, masterp, field);
	}
    }

    /*
     * Cleanup.
     */
    if (argc > 0) {
	for (req = field_reqs; req < field_reqs + argc; req++) {
	    if (req->match_count == 0)
		msg_warn("unmatched request: \"%s\"", req->raw_text);
	    argv_free(req->service_pattern);
	}
	myfree((char *) field_reqs);
    }
}

/* edit_master_field - replace master.cf field value. */

void    edit_master_field(PC_MASTER_ENT *masterp, int field,
			          const char *new_value)
{

    /*
     * Replace multi-column attribute.
     */
    if (field == PC_MASTER_FIELD_CMD) {
	argv_truncate(masterp->argv, PC_MASTER_FIELD_CMD);
	argv_split_append(masterp->argv, new_value, PC_MASTER_BLANKS);
    }

    /*
     * Replace single-column attribute.
     */
    else {
	argv_replace_one(masterp->argv, field, new_value);
    }

    /*
     * Do per-field sanity checks.
     */
    check_master_entry(masterp->argv, new_value);
}

/* print_master_param - scaffolding */

static void print_master_param(VSTREAM *fp, int mode, PC_MASTER_ENT *masterp,
		            const char *param_name, const char *param_value)
{
    if ((mode & SHOW_EVAL) != 0)
	param_value = expand_parameter_value((VSTRING *) 0, mode,
					     param_value, masterp);
    if ((mode & HIDE_NAME) == 0) {
	print_line(fp, mode, "%s%c%s = %s\n",
		   masterp->name_space, PC_NAMESP_SEP_CH,
		   param_name, param_value);
    } else {
	print_line(fp, mode, "%s\n", param_value);
    }
    if (msg_verbose)
	vstream_fflush(fp);
}

/* sort_argv_cb - sort argv call-back */

static int sort_argv_cb(const void *a, const void *b)
{
    return (strcmp(*(char **) a, *(char **) b));
}

/* show_master_any_param - show any parameter in master.cf service entry */

static void show_master_any_param(VSTREAM *fp, int mode, PC_MASTER_ENT *masterp)
{
    const char *myname = "show_master_any_param";
    ARGV   *argv = argv_alloc(10);
    DICT   *dict = masterp->all_params;
    const char *param_name;
    const char *param_value;
    int     param_count = 0;
    int     how;
    char  **cpp;

    /*
     * Print parameters in sorted order. The number of parameters per
     * master.cf entry is small, so we optmiize for code simplicity and don't
     * worry about the cost of double lookup.
     */

    /* Look up the parameter names and ignore the values. */

    for (how = DICT_SEQ_FUN_FIRST;
	 dict->sequence(dict, how, &param_name, &param_value) == 0;
	 how = DICT_SEQ_FUN_NEXT) {
	argv_add(argv, param_name, ARGV_END);
	param_count++;
    }

    /* Print the parameters in sorted order. */

    qsort(argv->argv, param_count, sizeof(argv->argv[0]), sort_argv_cb);
    for (cpp = argv->argv; (param_name = *cpp) != 0; cpp++) {
	if ((param_value = dict_get(dict, param_name)) == 0)
	    msg_panic("%s: parameter name not found: %s", myname, param_name);
	print_master_param(fp, mode, masterp, param_name, param_value);
    }

    /*
     * Clean up.
     */
    argv_free(argv);
}

/* show_master_params - show master.cf params */

void    show_master_params(VSTREAM *fp, int mode, int argc, char **argv)
{
    PC_MASTER_ENT *masterp;
    PC_MASTER_FIELD_REQ *field_reqs;
    PC_MASTER_FIELD_REQ *req;
    DICT   *dict;
    const char *param_value;

    /*
     * Parse the filter expressions.
     */
    if (argc > 0) {
	field_reqs = (PC_MASTER_FIELD_REQ *)
	    mymalloc(sizeof(*field_reqs) * argc);
	for (req = field_reqs; req < field_reqs + argc; req++) {
	    req->match_count = 0;
	    req->raw_text = *argv++;
	    req->service_pattern =
		parse_service_pattern(req->raw_text, 1, 3);
	    if (req->service_pattern == 0)
		msg_fatal("-P option requires service_name[/type[/parameter]]");
	    req->param_pattern = req->service_pattern->argv[2];
	}
    }

    /*
     * Iterate over the master table.
     */
    for (masterp = master_table; masterp->argv != 0; masterp++) {
	if ((dict = masterp->all_params) != 0) {
	    if (argc > 0) {
		for (req = field_reqs; req < field_reqs + argc; req++) {
		    if (MATCH_SERVICE_PATTERN(req->service_pattern,
					      masterp->argv->argv[0],
					      masterp->argv->argv[1])) {
			if (IS_MAGIC_PARAM_PATTERN(req->param_pattern)) {
			    show_master_any_param(fp, mode, masterp);
			    req->match_count += 1;
			} else if ((param_value = dict_get(dict,
						req->param_pattern)) != 0) {
			    print_master_param(fp, mode, masterp,
					       req->param_pattern,
					       param_value);
			    req->match_count += 1;
			}
		    }
		}
	    } else {
		show_master_any_param(fp, mode, masterp);
	    }
	}
    }

    /*
     * Cleanup.
     */
    if (argc > 0) {
	for (req = field_reqs; req < field_reqs + argc; req++) {
	    if (req->match_count == 0)
		msg_warn("unmatched request: \"%s\"", req->raw_text);
	    argv_free(req->service_pattern);
	}
	myfree((char *) field_reqs);
    }
}

/* edit_master_param - update, add or remove -o parameter=value */

void    edit_master_param(PC_MASTER_ENT *masterp, int mode,
			          const char *param_name,
			          const char *param_value)
{
    const char *myname = "edit_master_param";
    ARGV   *argv = masterp->argv;
    const char *arg;
    const char *aval;
    int     param_match = 0;
    int     name_len = strlen(param_name);
    int     field;

    for (field = PC_MASTER_MIN_FIELDS; argv->argv[field] != 0; field++) {
	arg = argv->argv[field];

	/*
	 * Stop at the first non-option argument or end-of-list.
	 */
	if (arg[0] != '-' || strcmp(arg, "--") == 0) {
	    break;
	}

	/*
	 * Zoom in on command-line options with a value.
	 */
	else if (strchr(daemon_options_expecting_value, arg[1]) != 0
		 && (aval = argv->argv[field + 1]) != 0) {

	    /*
	     * Zoom in on "-o parameter=value".
	     */
	    if (strcmp(arg, "-o") == 0) {
		if (strncmp(aval, param_name, name_len) == 0
		    && aval[name_len] == '=') {
		    param_match = 1;
		    switch (mode & (EDIT_CONF | EDIT_EXCL)) {

			/*
			 * Update parameter=value.
			 */
		    case EDIT_CONF:
			aval = concatenate(param_name, "=",
					   param_value, (char *) 0);
			argv_replace_one(argv, field + 1, aval);
			myfree((char *) aval);
			if (masterp->all_params)
			    dict_put(masterp->all_params, param_name, param_value);
			/* XXX Update parameter "used/defined" status. */
			break;

			/*
			 * Delete parameter=value.
			 */
		    case EDIT_EXCL:
			argv_delete(argv, field, 2);
			if (masterp->all_params)
			    dict_del(masterp->all_params, param_name);
			/* XXX Update parameter "used/defined" status. */
			field -= 2;
			break;
		    default:
			msg_panic("%s: unexpected mode: %d", myname, mode);
		    }
		}
	    }

	    /*
	     * Skip over the command-line option value.
	     */
	    field += 1;
	}
    }

    /*
     * Add unmatched parameter.
     */
    if ((mode & EDIT_CONF) && param_match == 0) {
	/* XXX Generalize: argv_insert(argv, where, list...) */
	argv_insert_one(argv, field, "-o");
	aval = concatenate(param_name, "=",
			   param_value, (char *) 0);
	argv_insert_one(argv, field + 1, aval);
	if (masterp->all_params)
	    dict_put(masterp->all_params, param_name, param_value);
	/* XXX May affect parameter "used/defined" status. */
	myfree((char *) aval);
	param_match = 1;
    }
}
