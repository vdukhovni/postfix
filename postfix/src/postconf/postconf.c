/*++
/* NAME
/*	postconf 1
/* SUMMARY
/*	Postfix configuration utility
/* SYNOPSIS
/* .fi
/*	\fBManaging main.cf:\fR
/*
/*	\fBpostconf\fR [\fB-dfhnv\fR] [\fB-c \fIconfig_dir\fR]
/*	[\fIparameter ...\fR]
/*
/*	\fBpostconf\fR [\fB-ev\fR] [\fB-c \fIconfig_dir\fR]
/*	[\fIparameter=value ...\fR]
/*
/*	\fBpostconf\fR [\fB-#v\fR] [\fB-c \fIconfig_dir\fR]
/*	[\fIparameter ...\fR]
/*
/*	\fBManaging master.cf:\fR
/*
/*	\fBpostconf\fR [\fB-fMv\fR] [\fB-c \fIconfig_dir\fR]
/*	[\fIservice ...\fR]
/*
/*	\fBManaging other configuration:\fR
/*
/*	\fBpostconf\fR [\fB-aAlmv\fR] [\fB-c \fIconfig_dir\fR]
/*
/*	\fBpostconf\fR [\fB-btv\fR] [\fB-c \fIconfig_dir\fR] [\fItemplate_file\fR]
/* DESCRIPTION
/*	By default, the \fBpostconf\fR(1) command displays the
/*	values of \fBmain.cf\fR configuration parameters, and warns
/*	about possible mis-typed parameter names (Postfix 2.9 and later).
/*	It can also change \fBmain.cf\fR configuration
/*	parameter values, or display other configuration information
/*	about the Postfix mail system.
/*
/*	Options:
/* .IP \fB-A\fR
/*	List the available SASL client plug-in types.  The SASL
/*	plug-in type is selected with the \fBsmtp_sasl_type\fR or
/*	\fBlmtp_sasl_type\fR configuration parameters by specifying
/*	one of the names listed below.
/* .IP \fB-a\fR
/*	List the available SASL server plug-in types.  The SASL
/*	plug-in type is selected with the \fBsmtpd_sasl_type\fR
/*	configuration parameter by specifying one of the names
/*	listed below.
/* .RS
/* .IP \fBcyrus\fR
/*	This server plug-in is available when Postfix is built with
/*	Cyrus SASL support.
/* .IP \fBdovecot\fR
/*	This server plug-in uses the Dovecot authentication server,
/*	and is available when Postfix is built with any form of SASL
/*	support.
/* .RE
/* .IP
/*	This feature is available with Postfix 2.3 and later.
/* .RS
/* .IP \fBcyrus\fR
/*	This client plug-in is available when Postfix is built with
/*	Cyrus SASL support.
/* .RE
/* .IP
/*	This feature is available with Postfix 2.3 and later.
/* .IP "\fB-b\fR [\fItemplate_file\fR]"
/*	Display the message text that appears at the beginning of
/*	delivery status notification (DSN) messages, with $\fBname\fR
/*	expressions replaced by actual values.  To override the
/*	built-in message text, specify a template file at the end
/*	of the command line, or specify a template file in \fBmain.cf\fR
/*	with the \fBbounce_template_file\fR parameter.
/*	To force selection of the built-in message text templates,
/*	specify an empty template file name (in shell language: "").
/*
/*	This feature is available with Postfix 2.3 and later.
/* .IP "\fB-c \fIconfig_dir\fR"
/*	The \fBmain.cf\fR configuration file is in the named directory
/*	instead of the default configuration directory.
/* .IP \fB-d\fR
/*	Print \fBmain.cf\fR default parameter settings instead of
/*	actual settings.
/* .IP \fB-e\fR
/*	Edit the \fBmain.cf\fR configuration file. The file is copied
/*	to a temporary file then renamed into place. Parameters and
/*	values are specified on the command line. Use quotes in order
/*	to protect shell metacharacters and whitespace.
/*
/*	With Postfix version 2.8 and later, the \fB-e\fR is no
/*	longer needed.
/* .IP \fB-f\fR
/*	When printing \fBmain.cf\fR or \fBmaster.cf\fR configuration file
/*	entries, fold long lines for human readability.
/*
/*	This feature is available with Postfix 2.9 and later.
/* .IP \fB-h\fR
/*	Show \fBmain.cf\fR parameter values only; do not prepend
/*	the "\fIname = \fR" label that normally precedes the value.
/* .IP \fB-l\fR
/*	List the names of all supported mailbox locking methods.
/*	Postfix supports the following methods:
/* .RS
/* .IP \fBflock\fR
/*	A kernel-based advisory locking method for local files only.
/*	This locking method is available on systems with a BSD
/*	compatible library.
/* .IP \fBfcntl\fR
/*	A kernel-based advisory locking method for local and remote files.
/* .IP \fBdotlock\fR
/*	An application-level locking method. An application locks a file
/*	named \fIfilename\fR by creating a file named \fIfilename\fB.lock\fR.
/*	The application is expected to remove its own lock file, as well as
/*	stale lock files that were left behind after abnormal termination.
/* .RE
/* .IP \fB-M\fR
/*	Show \fBmaster.cf\fR file contents instead of \fBmain.cf\fR
/*	file contents.  Use \fB-Mf\fR to fold long lines for human
/*	readability.
/*
/*	If \fIservice ...\fR is specified, only the matching services
/*	will be output. For example, a service of \fBinet\fR will
/*	match all services that listen on the network.
/*
/*	Specify zero or more argument, each with a \fIservice-type\fR
/*	name (\fBinet\fR, \fBunix\fR, \fBfifo\fR, or \fBpass\fR)
/*	or with a \fIservice-name.service-type\fR pair, where
/*	\fIservice-name\fR is the first field of a master.cf entry.
/*
/*	This feature is available with Postfix 2.9 and later.
/* .IP \fB-m\fR
/*	List the names of all supported lookup table types. In Postfix
/*	configuration files,
/*	lookup tables are specified as \fItype\fB:\fIname\fR, where
/*	\fItype\fR is one of the types listed below. The table \fIname\fR
/*	syntax depends on the lookup table type as described in the
/*	DATABASE_README document.
/* .RS
/* .IP \fBbtree\fR
/*	A sorted, balanced tree structure.
/*	This is available on systems with support for Berkeley DB
/*	databases.
/* .IP \fBcdb\fR
/*	A read-optimized structure with no support for incremental updates.
/*	This is available on systems with support for CDB databases.
/* .IP \fBcidr\fR
/*	A table that associates values with Classless Inter-Domain Routing
/*	(CIDR) patterns. This is described in \fBcidr_table\fR(5).
/* .IP \fBdbm\fR
/*	An indexed file type based on hashing.
/*	This is available on systems with support for DBM databases.
/* .IP \fBenviron\fR
/*	The UNIX process environment array. The lookup key is the variable
/*	name. Originally implemented for testing, someone may find this
/*	useful someday.
/* .IP \fBhash\fR
/*	An indexed file type based on hashing.
/*	This is available on systems with support for Berkeley DB
/*	databases.
/* .IP \fBinternal\fR
/*	A non-shared, in-memory hash table. Its content are lost
/*	when a process terminates.
/* .IP "\fBldap\fR (read-only)"
/*	Perform lookups using the LDAP protocol. This is described
/*	in \fBldap_table\fR(5).
/* .IP "\fBmysql\fR (read-only)"
/*	Perform lookups using the MYSQL protocol. This is described
/*	in \fBmysql_table\fR(5).
/* .IP "\fBpcre\fR (read-only)"
/*	A lookup table based on Perl Compatible Regular Expressions. The
/*	file format is described in \fBpcre_table\fR(5).
/* .IP "\fBpgsql\fR (read-only)"
/*	Perform lookups using the PostgreSQL protocol. This is described
/*	in \fBpgsql_table\fR(5).
/* .IP "\fBproxy\fR (read-only)"
/*	A lookup table that is implemented via the Postfix
/*	\fBproxymap\fR(8) service. The table name syntax is
/*	\fItype\fB:\fIname\fR.
/* .IP "\fBregexp\fR (read-only)"
/*	A lookup table based on regular expressions. The file format is
/*	described in \fBregexp_table\fR(5).
/* .IP \fBsdbm\fR
/*	An indexed file type based on hashing.
/*	This is available on systems with support for SDBM databases.
/* .IP "\fBsqlite\fR (read-only)"
/*	Perform lookups from SQLite database files. This is described
/*	in \fBsqlite_table\fR(5).
/* .IP "\fBstatic\fR (read-only)"
/*	A table that always returns its name as lookup result. For example,
/*	\fBstatic:foobar\fR always returns the string \fBfoobar\fR as lookup
/*	result.
/* .IP "\fBtcp\fR (read-only)"
/*	Perform lookups using a simple request-reply protocol that is
/*	described in \fBtcp_table\fR(5).
/* .IP "\fBtexthash\fR (read-only)"
/*	Produces similar results as hash: files, except that you don't
/*	need to run the postmap(1) command before you can use the file,
/*	and that it does not detect changes after the file is read.
/* .IP "\fBunix\fR (read-only)"
/*	A limited way to query the UNIX authentication database. The
/*	following tables are implemented:
/* .RS
/*. IP \fBunix:passwd.byname\fR
/*	The table is the UNIX password database. The key is a login name.
/*	The result is a password file entry in \fBpasswd\fR(5) format.
/* .IP \fBunix:group.byname\fR
/*	The table is the UNIX group database. The key is a group name.
/*	The result is a group file entry in \fBgroup\fR(5) format.
/* .RE
/* .RE
/* .IP
/*	Other table types may exist depending on how Postfix was built.
/* .IP \fB-n\fR
/*	Print \fBmain.cf\fR parameter settings that are explicitly
/*	specified in \fBmain.cf\fR.
/* .IP "\fB-t\fR [\fItemplate_file\fR]"
/*	Display the templates for delivery status notification (DSN)
/*	messages. To override the built-in templates, specify a
/*	template file at the end of the command line, or specify a
/*	template file in \fBmain.cf\fR with the \fBbounce_template_file\fR
/*	parameter.  To force selection of the built-in templates,
/*	specify an empty template file name (in shell language:
/*	"").
/*
/*	This feature is available with Postfix 2.3 and later.
/* .IP \fB-v\fR
/*	Enable verbose logging for debugging purposes. Multiple \fB-v\fR
/*	options make the software increasingly verbose.
/* .IP \fB-#\fR
/*	Edit the \fBmain.cf\fR configuration file. The file is copied
/*	to a temporary file then renamed into place. The parameters
/*	specified on the command line are commented-out, so that they
/*	revert to their default values. Specify a list of parameter
/*	names, not name=value pairs.  There is no \fBpostconf\fR command
/*	to perform the reverse operation.
/*
/*	This feature is available with Postfix 2.6 and later.
/* DIAGNOSTICS
/*	Problems are reported to the standard error stream.
/* BUGS
/*	\fBpostconf\fR(1) may log "unused parameter" warnings for
/*	\fBmaster.cf\fR entries with "-o user-defined-name=value".
/*	Addressing this limitation requires support for per-service
/*	parameter name spaces.
/* ENVIRONMENT
/* .ad
/* .fi
/* .IP \fBMAIL_CONFIG\fR
/*	Directory with Postfix configuration files.
/* CONFIGURATION PARAMETERS
/* .ad
/* .fi
/*	The following \fBmain.cf\fR parameters are especially relevant to
/*	this program.
/*
/*	The text below provides only a parameter summary. See
/*	\fBpostconf\fR(5) for more details including examples.
/* .IP "\fBconfig_directory (see 'postconf -d' output)\fR"
/*	The default location of the Postfix main.cf and master.cf
/*	configuration files.
/* .IP "\fBbounce_template_file (empty)\fR"
/*	Pathname of a configuration file with bounce message templates.
/* FILES
/*	/etc/postfix/main.cf, Postfix configuration parameters
/* SEE ALSO
/*	bounce(5), bounce template file format
/*	master(5), master.cf configuration file syntax
/*	postconf(5), main.cf configuration file syntax
/* README FILES
/* .ad
/* .fi
/*	Use "\fBpostconf readme_directory\fR" or
/*	"\fBpostconf html_directory\fR" to locate this information.
/* .na
/* .nf
/*	DATABASE_README, Postfix lookup table overview
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
#include <sys/stat.h>
#include <stdio.h>			/* rename() */
#include <pwd.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

#ifdef USE_PATHS_H
#include <paths.h>
#endif

/* Utility library. */

#include <msg.h>
#include <vstream.h>
#include <msg_vstream.h>
#include <get_hostname.h>
#include <stringops.h>
#include <htable.h>
#include <dict.h>
#include <safe.h>
#include <mymalloc.h>
#include <argv.h>
#include <split_at.h>
#include <vstring_vstream.h>
#include <myflock.h>
#include <inet_proto.h>
#include <argv.h>
#include <edit_file.h>
#include <readlline.h>
#include <mac_expand.h>

/* Global library. */

#include <mynetworks.h>
#include <mail_conf.h>
#include <mail_dict.h>
#include <mail_proto.h>
#include <mail_version.h>
#include <mail_params.h>
#include <mail_addr.h>
#include <mbox_conf.h>
#include <mail_run.h>
#include <match_service.h>

/* XSASL library. */

#include <xsasl.h>

/* master library. */

#include <master_proto.h>

 /*
  * What we're supposed to be doing.
  */
#define SHOW_NONDEF	(1<<0)		/* show main.cf non-default settings */
#define SHOW_DEFS	(1<<1)		/* show main.cf default setting */
#define SHOW_NAME	(1<<2)		/* show main.cf parameter name */
#define SHOW_MAPS	(1<<3)		/* show map types */
#define EDIT_MAIN	(1<<4)		/* edit main.cf */
#define SHOW_LOCKS	(1<<5)		/* show mailbox lock methods */
#define SHOW_EVAL	(1<<6)		/* expand main.cf right-hand sides */
#define SHOW_SASL_SERV	(1<<7)		/* show server auth plugin types */
#define SHOW_SASL_CLNT	(1<<8)		/* show client auth plugin types */
#define COMMENT_OUT	(1<<9)		/* #-out selected main.cf entries */
#define SHOW_MASTER	(1<<10)		/* show master.cf entries */
#define FOLD_LINE	(1<<11)		/* fold long *.cf entries */

 /*
  * Lookup table for in-core parameter info.
  * 
  * XXX Change the value pointers from table indices into pointers to parameter
  * objects with print methods.
  */
HTABLE *param_table;

 /*
  * Lookup table for master.cf info.
  * 
  * XXX Replace by data structures with per-entry hashes for "-o name=value", so
  * that we can properly handle name=value definitions in per-service name
  * spaces.
  */
static ARGV **master_table;

 /*
  * Support for built-in parameters: declarations generated by scanning
  * actual C source files.
  */
#include "time_vars.h"
#include "bool_vars.h"
#include "int_vars.h"
#include "str_vars.h"
#include "raw_vars.h"
#include "nint_vars.h"
#include "nbool_vars.h"
#include "long_vars.h"

 /*
  * Support for built-in parameters: manually extracted.
  */
#include "install_vars.h"

 /*
  * Support for built-in parameters: lookup tables generated by scanning
  * actual C source files.
  */
static const CONFIG_TIME_TABLE time_table[] = {
#include "time_table.h"
    0,
};

static const CONFIG_BOOL_TABLE bool_table[] = {
#include "bool_table.h"
    0,
};

static const CONFIG_INT_TABLE int_table[] = {
#include "int_table.h"
    0,
};

static const CONFIG_STR_TABLE str_table[] = {
#include "str_table.h"
#include "install_table.h"
    0,
};

static const CONFIG_RAW_TABLE raw_table[] = {
#include "raw_table.h"
    0,
};

static const CONFIG_NINT_TABLE nint_table[] = {
#include "nint_table.h"
    0,
};

static const CONFIG_NBOOL_TABLE nbool_table[] = {
#include "nbool_table.h"
    0,
};

static const CONFIG_LONG_TABLE long_table[] = {
#include "long_table.h"
    0,
};

 /*
  * Ad-hoc name-value string pair.
  */
typedef struct {
    const char *name;
    const char *value;
} PC_STRING_NV;

 /*
  * Service-defined parameter names are created by appending postfix-defined
  * suffixes to master.cf service names. These parameters have default values
  * that are defined by built-in parameters.
  */
static PC_STRING_NV *serv_param_table;
static ssize_t serv_param_tablen;

 /*
  * Support for user-defined parameters.
  * 
  * There are three categories of known parameters: built-in, service-defined
  * (see previous comment), and valid user-defined. In addition there are
  * multiple name spaces: the global main.cf name space, and the per-service
  * name space of each master.cf entry.
  * 
  * There are two categories of valid user-defined parameters:
  * 
  * - Parameters whose name appears in the value of smtpd_restriction_classes in
  * main.cf, and whose name has a "name=value" entry in main.cf (todo:
  * support for creating names with "-o smtpd_restriction_classes=name"
  * within a master.cf per-service name space).
  * 
  * - Parameters whose $name (macro expansion) appears in the value of a
  * "name=value" entry in main.cf or master.cf of a "known" parameter, and
  * whose name has a "name=value" entry in main.cf (todo: master.cf).
  * 
  * All other user-defined parameters are invalid. We currently log a warning
  * for "name=value" entries in main.cf or master.cf whose $name does not
  * appear in the value of a main.cf or master.cf "name=value" entry of a
  * "known" parameter.
  */
static char **user_param_table;
static ssize_t user_param_tablen;

 /*
  * Parameters with default values obtained via function calls.
  */
char   *var_myhostname;
char   *var_mydomain;
char   *var_mynetworks;

static const char *check_myhostname(void);
static const char *check_mydomainname(void);
static const char *check_mynetworks(void);

static const CONFIG_STR_FN_TABLE str_fn_table[] = {
    VAR_MYHOSTNAME, check_myhostname, &var_myhostname, 1, 0,
    VAR_MYDOMAIN, check_mydomainname, &var_mydomain, 1, 0,
    0,
};
static const CONFIG_STR_FN_TABLE str_fn_table_2[] = {
    VAR_MYNETWORKS, check_mynetworks, &var_mynetworks, 1, 0,
    0,
};

 /*
  * Line-wrapping support.
  */
#define LINE_LIMIT	80		/* try to fold longer lines */
#define SEPARATORS	" \t\r\n"
#define INDENT_LEN	4		/* indent long text by 4 */
#define INDENT_TEXT	"    "

 /*
  * XXX Global so that call-backs can see it.
  */
#define DEF_MODE	SHOW_NAME
static int cmd_mode = DEF_MODE;

/* set_config_dir - forcibly override var_config_dir */

static void set_config_dir(void)
{
    char   *config_dir;

    if (var_config_dir)
	myfree(var_config_dir);
    var_config_dir = mystrdup((config_dir = safe_getenv(CONF_ENV_PATH)) != 0 ?
			      config_dir : DEF_CONFIG_DIR);	/* XXX */
    set_mail_conf_str(VAR_CONFIG_DIR, var_config_dir);
}

/* check_myhostname - lookup hostname and validate */

static const char *check_myhostname(void)
{
    static const char *name;
    const char *dot;
    const char *domain;

    /*
     * Use cached result.
     */
    if (name)
	return (name);

    /*
     * If the local machine name is not in FQDN form, try to append the
     * contents of $mydomain.
     */
    name = get_hostname();
    if ((dot = strchr(name, '.')) == 0) {
	if ((domain = mail_conf_lookup_eval(VAR_MYDOMAIN)) == 0)
	    domain = DEF_MYDOMAIN;
	name = concatenate(name, ".", domain, (char *) 0);
    }
    return (name);
}

/* get_myhostname - look up and store my hostname */

static void get_myhostname(void)
{
    const char *name;

    if ((name = mail_conf_lookup_eval(VAR_MYHOSTNAME)) == 0)
	name = check_myhostname();
    var_myhostname = mystrdup(name);
}

/* check_mydomainname - lookup domain name and validate */

static const char *check_mydomainname(void)
{
    char   *dot;

    /*
     * Use the hostname when it is not a FQDN ("foo"), or when the hostname
     * actually is a domain name ("foo.com").
     */
    if (var_myhostname == 0)
	get_myhostname();
    if ((dot = strchr(var_myhostname, '.')) == 0 || strchr(dot + 1, '.') == 0)
	return (DEF_MYDOMAIN);
    return (dot + 1);
}

/* check_mynetworks - lookup network address list */

static const char *check_mynetworks(void)
{
    INET_PROTO_INFO *proto_info;
    const char *junk;

    if (var_inet_interfaces == 0) {
	if ((cmd_mode & SHOW_DEFS)
	    || (junk = mail_conf_lookup_eval(VAR_INET_INTERFACES)) == 0)
	    junk = DEF_INET_INTERFACES;
	var_inet_interfaces = mystrdup(junk);
    }
    if (var_mynetworks_style == 0) {
	if ((cmd_mode & SHOW_DEFS)
	    || (junk = mail_conf_lookup_eval(VAR_MYNETWORKS_STYLE)) == 0)
	    junk = DEF_MYNETWORKS_STYLE;
	var_mynetworks_style = mystrdup(junk);
    }
    if (var_inet_protocols == 0) {
	if ((cmd_mode & SHOW_DEFS)
	    || (junk = mail_conf_lookup_eval(VAR_INET_PROTOCOLS)) == 0)
	    junk = DEF_INET_PROTOCOLS;
	var_inet_protocols = mystrdup(junk);
	proto_info = inet_proto_init(VAR_INET_PROTOCOLS, var_inet_protocols);
    }
    return (mynetworks());
}

/* edit_parameters - edit parameter file */

static void edit_parameters(int cmd_mode, int argc, char **argv)
{
    char   *path;
    EDIT_FILE *ep;
    VSTREAM *src;
    VSTREAM *dst;
    VSTRING *buf = vstring_alloc(100);
    VSTRING *key = vstring_alloc(10);
    char   *cp;
    char   *edit_key;
    char   *edit_val;
    HTABLE *table;
    struct cvalue {
	char   *value;
	int     found;
    };
    struct cvalue *cvalue;
    HTABLE_INFO **ht_info;
    HTABLE_INFO **ht;
    int     interesting;
    const char *err;

    /*
     * Store command-line parameters for quick lookup.
     */
    table = htable_create(argc);
    while ((cp = *argv++) != 0) {
	if (strchr(cp, '\n') != 0)
	    msg_fatal("-e or -# accepts no multi-line input");
	while (ISSPACE(*cp))
	    cp++;
	if (*cp == '#')
	    msg_fatal("-e or -# accepts no comment input");
	if (cmd_mode & EDIT_MAIN) {
	    if ((err = split_nameval(cp, &edit_key, &edit_val)) != 0)
		msg_fatal("%s: \"%s\"", err, cp);
	} else if (cmd_mode & COMMENT_OUT) {
	    if (*cp == 0)
		msg_fatal("-# requires non-blank parameter names");
	    if (strchr(cp, '=') != 0)
		msg_fatal("-# requires parameter names only");
	    edit_key = mystrdup(cp);
	    trimblanks(edit_key, 0);
	    edit_val = 0;
	} else {
	    msg_panic("edit_parameters: unknown mode %d", cmd_mode);
	}
	cvalue = (struct cvalue *) mymalloc(sizeof(*cvalue));
	cvalue->value = edit_val;
	cvalue->found = 0;
	htable_enter(table, edit_key, (char *) cvalue);
    }

    /*
     * Open a temp file for the result. This uses a deterministic name so we
     * don't leave behind thrash with random names.
     */
    set_config_dir();
    path = concatenate(var_config_dir, "/", MAIN_CONF_FILE, (char *) 0);
    if ((ep = edit_file_open(path, O_CREAT | O_WRONLY, 0644)) == 0)
	msg_fatal("open %s%s: %m", path, EDIT_FILE_SUFFIX);
    dst = ep->tmp_fp;

    /*
     * Open the original file for input.
     */
    if ((src = vstream_fopen(path, O_RDONLY, 0)) == 0) {
	/* OK to delete, since we control the temp file name exclusively. */
	(void) unlink(ep->tmp_path);
	msg_fatal("open %s for reading: %m", path);
    }

    /*
     * Copy original file to temp file, while replacing parameters on the
     * fly. Issue warnings for names found multiple times.
     */
#define STR(x) vstring_str(x)

    interesting = 0;
    while (vstring_get(buf, src) != VSTREAM_EOF) {
	for (cp = STR(buf); ISSPACE(*cp) /* including newline */ ; cp++)
	     /* void */ ;
	/* Copy comment, all-whitespace, or empty line. */
	if (*cp == '#' || *cp == 0) {
	    vstream_fputs(STR(buf), dst);
	}
	/* Copy, skip or replace continued text. */
	else if (cp > STR(buf)) {
	    if (interesting == 0)
		vstream_fputs(STR(buf), dst);
	    else if (cmd_mode & COMMENT_OUT)
		vstream_fprintf(dst, "#%s", STR(buf));
	}
	/* Copy or replace start of logical line. */
	else {
	    vstring_strncpy(key, cp, strcspn(cp, " \t\r\n="));
	    cvalue = (struct cvalue *) htable_find(table, STR(key));
	    if ((interesting = !!cvalue) != 0) {
		if (cvalue->found++ == 1)
		    msg_warn("%s: multiple entries for \"%s\"", path, STR(key));
		if (cmd_mode & EDIT_MAIN)
		    vstream_fprintf(dst, "%s = %s\n", STR(key), cvalue->value);
		else if (cmd_mode & COMMENT_OUT)
		    vstream_fprintf(dst, "#%s", cp);
		else
		    msg_panic("edit_parameters: unknown mode %d", cmd_mode);
	    } else {
		vstream_fputs(STR(buf), dst);
	    }
	}
    }

    /*
     * Generate new entries for parameters that were not found.
     */
    if (cmd_mode & EDIT_MAIN) {
	for (ht_info = ht = htable_list(table); *ht; ht++) {
	    cvalue = (struct cvalue *) ht[0]->value;
	    if (cvalue->found == 0)
		vstream_fprintf(dst, "%s = %s\n", ht[0]->key, cvalue->value);
	}
	myfree((char *) ht_info);
    }

    /*
     * When all is well, rename the temp file to the original one.
     */
    if (vstream_fclose(src))
	msg_fatal("read %s: %m", path);
    if (edit_file_close(ep) != 0)
	msg_fatal("close %s%s: %m", path, EDIT_FILE_SUFFIX);

    /*
     * Cleanup.
     */
    myfree(path);
    vstring_free(buf);
    vstring_free(key);
    htable_free(table, myfree);
}

/* read_parameters - read parameter info from file */

static void read_parameters(void)
{
    char   *path;

    /*
     * A direct rip-off of mail_conf_read(). XXX Avoid code duplication by
     * better code decomposition.
     */
    dict_unknown_allowed = 1;
    set_config_dir();
    path = concatenate(var_config_dir, "/", MAIN_CONF_FILE, (char *) 0);
    dict_load_file(CONFIG_DICT, path);
    myfree(path);
}

/* set_parameters - set parameter values from default or explicit setting */

static void set_parameters(void)
{

    /*
     * The proposal below describes some of the steps needed to expand
     * parameter values. It has a problem: it updates the configuration
     * parameter dictionary, and in doing so breaks the "postconf -d"
     * implementation.
     * 
     * Populate the configuration parameter dictionary with default settings or
     * with actual settings.
     * 
     * Iterate over each entry in str_fn_table, str_fn_table_2, time_table,
     * bool_table, int_table, str_table, and raw_table. Look up each
     * parameter name in the configuration parameter dictionary. If the
     * parameter is not set, take the default value, or take the value from
     * main.cf, without doing $name expansions. This includes converting
     * default values from numeric/boolean internal forms to external string
     * form.
     * 
     * Once the configuration parameter dictionary is populated, printing a
     * parameter setting is a matter of querying the configuration parameter
     * dictionary, optionally expanding of $name values, and printing the
     * result.
     */
}

/* read_master - read and digest the master.cf file */

static void read_master(void)
{
    char   *path;
    VSTRING *buf = vstring_alloc(100);
    ARGV   *argv;
    VSTREAM *fp;
    int     entry_count = 0;
    int     line_count = 0;

    /*
     * Get the location of master.cf.
     */
    if (var_config_dir == 0)
	set_config_dir();
    path = concatenate(var_config_dir, "/", MASTER_CONF_FILE, (char *) 0);

    /*
     * We can't use the master_ent routines in their current form. They
     * convert everything to internal form, and they skip disabled services.
     * We need to be able to show default fields as "-", and we need to know
     * about all service names so that we can generate service-dependent
     * parameter names (transport-dependent etc.).
     */
#define MASTER_BLANKS	" \t\r\n"		/* XXX */
#define MASTER_FIELD_COUNT 8			/* XXX */

    /*
     * Initialize the in-memory master table.
     */
    master_table = (ARGV **) mymalloc(sizeof(*master_table));

    /*
     * Skip blank lines and comment lines.
     */
    if ((fp = vstream_fopen(path, O_RDONLY, 0)) == 0)
	msg_fatal("open %s: %m", path);
    while (readlline(buf, fp, &line_count) != 0) {
	master_table = (ARGV **) myrealloc((char *) master_table,
				 (entry_count + 2) * sizeof(*master_table));
	argv = argv_split(STR(buf), MASTER_BLANKS);
	if (argv->argc < MASTER_FIELD_COUNT)
	    msg_fatal("file %s: line %d: bad field count", path, line_count);
	master_table[entry_count++] = argv;
    }
    master_table[entry_count] = 0;
    vstream_fclose(fp);
    myfree(path);
    vstring_free(buf);
}

 /*
  * Basename of programs in $daemon_directory. XXX These belong in a header
  * file, or they should be made configurable.
  */
#ifndef MAIL_PROGRAM_LOCAL
#define MAIL_PROGRAM_LOCAL	"local"
#define MAIL_PROGRAM_ERROR	"error"
#define MAIL_PROGRAM_VIRTUAL	"virtual"
#define MAIL_PROGRAM_SMTP	"smtp"
#define MAIL_PROGRAM_LMTP	"lmtp"
#define MAIL_PROGRAM_PIPE	"pipe"
#define MAIL_PROGRAM_SPAWN	"spawn"
#endif

/* add_service_parameter - add one service parameter name and default */

static void add_service_parameter(const char *service, const char *suffix,
				          const char *defparam)
{
    PC_STRING_NV *sp;
    char   *name = concatenate(service, suffix, (char *) 0);

    /*
     * Skip service parameter names that have built-in definitions. This
     * happens with message delivery transports that have a non-default
     * per-destination concurrency or recipient limit, such as local(8).
     */
    if (htable_locate(param_table, name) != 0) {
	myfree(name);
    } else {
	serv_param_table = (PC_STRING_NV *)
	    myrealloc((char *) serv_param_table,
		      (serv_param_tablen + 1) * sizeof(*serv_param_table));
	sp = serv_param_table + serv_param_tablen;
	sp->name = name;
	sp->value = defparam;
	serv_param_tablen += 1;
    }
}

/* add_service_parameters - add all service parameters with defaults */

static void add_service_parameters(void)
{
    static const PC_STRING_NV service_params[] = {
	/* suffix, default parameter name */
	_XPORT_RCPT_LIMIT, VAR_XPORT_RCPT_LIMIT,
	_STACK_RCPT_LIMIT, VAR_STACK_RCPT_LIMIT,
	_XPORT_REFILL_LIMIT, VAR_XPORT_REFILL_LIMIT,
	_XPORT_REFILL_DELAY, VAR_XPORT_REFILL_DELAY,
	_DELIVERY_SLOT_COST, VAR_DELIVERY_SLOT_COST,
	_DELIVERY_SLOT_LOAN, VAR_DELIVERY_SLOT_LOAN,
	_DELIVERY_SLOT_DISCOUNT, VAR_DELIVERY_SLOT_DISCOUNT,
	_MIN_DELIVERY_SLOTS, VAR_MIN_DELIVERY_SLOTS,
	_INIT_DEST_CON, VAR_INIT_DEST_CON,
	_DEST_CON_LIMIT, VAR_DEST_CON_LIMIT,
	_DEST_RCPT_LIMIT, VAR_DEST_RCPT_LIMIT,
	_CONC_POS_FDBACK, VAR_CONC_POS_FDBACK,
	_CONC_NEG_FDBACK, VAR_CONC_NEG_FDBACK,
	_CONC_COHORT_LIM, VAR_CONC_COHORT_LIM,
	_DEST_RATE_DELAY, VAR_DEST_RATE_DELAY,
	0,
    };
    static const PC_STRING_NV spawn_params[] = {
	/* suffix, default parameter name */
	_MAXTIME, VAR_COMMAND_MAXTIME,
	0,
    };
    typedef struct {
	const char *progname;
	const PC_STRING_NV *params;
    } PC_SERVICE_DEF;
    static const PC_SERVICE_DEF service_defs[] = {
	MAIL_PROGRAM_LOCAL, service_params,
	MAIL_PROGRAM_ERROR, service_params,
	MAIL_PROGRAM_VIRTUAL, service_params,
	MAIL_PROGRAM_SMTP, service_params,
	MAIL_PROGRAM_LMTP, service_params,
	MAIL_PROGRAM_PIPE, service_params,
	MAIL_PROGRAM_SPAWN, spawn_params,
	0,
    };
    const PC_STRING_NV *sp;
    const char *progname;
    const char *service;
    ARGV  **argvp;
    ARGV   *argv;
    const PC_SERVICE_DEF *sd;

    /*
     * Initialize the table with service parameter names and defaults.
     */
    serv_param_table = (PC_STRING_NV *) mymalloc(1);
    serv_param_tablen = 0;

    /*
     * Extract service names from master.cf and generate service parameter
     * information.
     */
    for (argvp = master_table; (argv = *argvp) != 0; argvp++) {

	/*
	 * Add service parameters for message delivery transports or spawn
	 * programs.
	 */
	progname = argv->argv[7];
	for (sd = service_defs; sd->progname; sd++) {
	    if (strcmp(sd->progname, progname) == 0) {
		service = argv->argv[0];
		for (sp = sd->params; sp->name; sp++)
		    add_service_parameter(service, sp->name, sp->value);
		break;
	    }
	}
    }

    /*
     * Now that the service parameter table size is frozen, enter the
     * parameters into the hash so that we can find and print them.
     */
    for (sp = serv_param_table; sp < serv_param_table + serv_param_tablen; sp++)
	if (htable_locate(param_table, sp->name) == 0)
	    htable_enter(param_table, sp->name, (char *) sp);
}

/* add_user_parameter - add one user-defined parameter name */

static void add_user_parameter(const char *name)
{
    /* XXX Merge this with check_parameter_value() */
    user_param_table = (char **)
    myrealloc((char *) user_param_table,
	      (user_param_tablen + 1) * sizeof(*user_param_table));
    user_param_table[user_param_tablen] = mystrdup(name);
    user_param_tablen += 1;
}

/* scan_user_parameter_value - extract macro names from parameter value */

#define NO_SCAN_RESULT	((VSTRING *) 0)
#define NO_SCAN_FILTER	((char *) 0)
#define NO_SCAN_MODE	(0)
#define NO_SCAN_CONTEXT	((char *) 0)

#define scan_user_parameter_value(value) do { \
    (void) mac_expand(NO_SCAN_RESULT, (value), MAC_EXP_FLAG_SCAN, \
		    NO_SCAN_FILTER, check_user_parameter, NO_SCAN_CONTEXT); \
} while (0)

/* check_user_parameter - try to promote user-defined parameter */

static const char *check_user_parameter(const char *mac_name,
					        int unused_mode,
					        char *unused_context)
{
    const char *mac_value;

    /*
     * Promote only user-defined parameters with an explicit "name=value"
     * definition in main.cf (todo: master.cf). Do not promote parameters
     * whose name appears only as a macro expansion; this is how Postfix
     * implements backwards compatibility after a feature name change.
     * 
     * Skip parameters that are already in the param_table hash.
     */
    if (htable_locate(param_table, mac_name) == 0) {
	mac_value = mail_conf_lookup(mac_name);
	if (mac_value != 0) {
	    add_user_parameter(mac_name);
	    /* Promote parameter names recursively. */
	    scan_user_parameter_value(mac_value);
	}
    }
    return (0);
}

/* add_user_parameters - add parameters with user-defined names */

static void add_user_parameters(void)
{
    const char *class_list;
    char   *saved_class_list;
    char   *cp;
    const char *cparam_value;
    HTABLE_INFO **ht_info;
    HTABLE_INFO **ht;
    ARGV  **argvp;
    ARGV   *argv;
    char   *arg;
    int     field;
    char   *saved_arg;
    char   *param_name;
    char   *param_value;
    char  **up;

    /*
     * Initialize the table with user-defined parameter names and values.
     */
    user_param_table = (char **) mymalloc(1);
    user_param_tablen = 0;

    /*
     * Add parameters whose names are defined with smtpd_restriction_classes,
     * but only if they have a "name=value" entry in main.cf.
     * 
     * XXX It is possible that a user-defined parameter is defined in master.cf
     * with "-o smtpd_restriction_classes=name -o name=value". This requires
     * name space support for master.cf entries. Without this, we always log
     * "unused parameter" warnings for "-o user-defined-name=value" entries.
     */
    if ((class_list = mail_conf_lookup_eval(VAR_REST_CLASSES)) != 0) {
	cp = saved_class_list = mystrdup(class_list);
	while ((param_name = mystrtok(&cp, ", \t\r\n")) != 0)
	    check_user_parameter(param_name, NO_SCAN_MODE, NO_SCAN_CONTEXT);
	myfree(saved_class_list);
    }

    /*
     * Parse the "name=value" instances in main.cf of built-in and service
     * parameters only, look for macro expansions of unknown parameter names,
     * and flag those parameter names as "known" if they have a "name=value"
     * entry in main.cf. Recursively apply the procedure to the values of
     * newly-flagged parameters.
     */
    for (ht_info = ht = htable_list(param_table); *ht; ht++)
	if ((cparam_value = mail_conf_lookup(ht[0]->key)) != 0)
	    scan_user_parameter_value(cparam_value);
    myfree((char *) ht_info);

    /*
     * Parse all "-o parameter=value" instances in master.cf, look for macro
     * expansions of unknown parameter names, and flag those parameter names
     * as "known" if they have a "name=value" entry in main.cf (XXX todo: in
     * master.cf; without master.cf name space support we always log "unused
     * parameter" warnings for "-o user-defined-name=value" entries).
     */
    for (argvp = master_table; (argv = *argvp) != 0; argvp++) {
	for (field = MASTER_FIELD_COUNT; argv->argv[field] != 0; field++) {
	    arg = argv->argv[field];
	    if (arg[0] != '-')
		break;
	    if (strcmp(arg, "-o") == 0 && (arg = argv->argv[field + 1]) != 0) {
		saved_arg = mystrdup(arg);
		if (split_nameval(saved_arg, &param_name, &param_value) == 0)
		    scan_user_parameter_value(param_value);
		myfree(saved_arg);
		field += 1;
	    }
	}
    }

    /*
     * Now that the user-defined parameter table size is frozen, enter the
     * parameters into the hash so that we can find and print them.
     */
    for (up = user_param_table; up < user_param_table + user_param_tablen; up++)
	if (htable_locate(param_table, *up) == 0)
	    htable_enter(param_table, *up, (char *) up);
}

/* hash_parameters - hash all parameter names so we can find and sort them */

static void hash_parameters(void)
{
    const CONFIG_TIME_TABLE *ctt;
    const CONFIG_BOOL_TABLE *cbt;
    const CONFIG_INT_TABLE *cit;
    const CONFIG_STR_TABLE *cst;
    const CONFIG_STR_FN_TABLE *csft;
    const CONFIG_RAW_TABLE *rst;
    const CONFIG_NINT_TABLE *nst;
    const CONFIG_NBOOL_TABLE *bst;
    const CONFIG_LONG_TABLE *lst;

    param_table = htable_create(100);

    for (ctt = time_table; ctt->name; ctt++)
	htable_enter(param_table, ctt->name, (char *) ctt);
    for (cbt = bool_table; cbt->name; cbt++)
	htable_enter(param_table, cbt->name, (char *) cbt);
    for (cit = int_table; cit->name; cit++)
	htable_enter(param_table, cit->name, (char *) cit);
    for (cst = str_table; cst->name; cst++)
	htable_enter(param_table, cst->name, (char *) cst);
    for (csft = str_fn_table; csft->name; csft++)
	htable_enter(param_table, csft->name, (char *) csft);
    for (csft = str_fn_table_2; csft->name; csft++)
	htable_enter(param_table, csft->name, (char *) csft);
    for (rst = raw_table; rst->name; rst++)
	htable_enter(param_table, rst->name, (char *) rst);
    for (nst = nint_table; nst->name; nst++)
	htable_enter(param_table, nst->name, (char *) nst);
    for (bst = nbool_table; bst->name; bst++)
	htable_enter(param_table, bst->name, (char *) bst);
    for (lst = long_table; lst->name; lst++)
	htable_enter(param_table, lst->name, (char *) lst);
}

/* print_line - show line possibly folded, and with normalized whitespace */

static void print_line(int mode, const char *fmt,...)
{
    va_list ap;
    static VSTRING *buf = 0;
    char   *start;
    char   *next;
    int     line_len = 0;
    int     word_len;

    /*
     * One-off initialization.
     */
    if (buf == 0)
	buf = vstring_alloc(100);

    /*
     * Format the text.
     */
    va_start(ap, fmt);
    vstring_vsprintf(buf, fmt, ap);
    va_end(ap);

    /*
     * Normalize the whitespace. We don't use the line_wrap() routine because
     * 1) that function does not normalize whitespace between words and 2) we
     * want to normalize whitespace even when not wrapping lines.
     * 
     * XXX Some parameters preserve whitespace: for example, smtpd_banner and
     * smtpd_reject_footer. If we have to preserve whitespace between words,
     * then perhaps readlline() can be changed to canonicalize whitespace
     * that follows a newline.
     */
    for (start = STR(buf); *(start += strspn(start, SEPARATORS)) != 0; start = next) {
	word_len = strcspn(start, SEPARATORS);
	if (*(next = start + word_len) != 0)
	    *next++ = 0;
	if (word_len > 0 && line_len > 0) {
	    if ((mode & FOLD_LINE) == 0 || line_len + word_len < LINE_LIMIT) {
		vstream_fputs(" ", VSTREAM_OUT);
		line_len += 1;
	    } else {
		vstream_fputs("\n" INDENT_TEXT, VSTREAM_OUT);
		line_len = INDENT_LEN;
	    }
	}
	vstream_fputs(start, VSTREAM_OUT);
	line_len += word_len;
    }
    vstream_fputs("\n", VSTREAM_OUT);
}

/* show_strval - show string-valued parameter */

static void show_strval(int mode, const char *name, const char *value)
{
    if (mode & SHOW_EVAL)
	value = mail_conf_eval(value);

    if (mode & SHOW_NAME) {
	print_line(mode, "%s = %s\n", name, value);
    } else {
	print_line(mode, "%s\n", value);
    }
}

/* show_intval - show integer-valued parameter */

static void show_intval(int mode, const char *name, int value)
{
    if (mode & SHOW_NAME) {
	print_line(mode, "%s = %d\n", name, value);
    } else {
	print_line(mode, "%d\n", value);
    }
}

/* show_longval - show long-valued parameter */

static void show_longval(int mode, const char *name, long value)
{
    if (mode & SHOW_NAME) {
	print_line(mode, "%s = %ld\n", name, value);
    } else {
	print_line(mode, "%ld\n", value);
    }
}

/* print_bool - print boolean parameter */

static void print_bool(int mode, const char *name, CONFIG_BOOL_TABLE *cbt)
{
    const char *value;

    if (mode & SHOW_DEFS) {
	show_strval(mode, name, cbt->defval ? "yes" : "no");
    } else {
	value = dict_lookup(CONFIG_DICT, name);
	if ((mode & SHOW_NONDEF) == 0) {
	    if (value == 0) {
		show_strval(mode, name, cbt->defval ? "yes" : "no");
	    } else {
		show_strval(mode, name, value);
	    }
	} else {
	    if (value != 0)
		show_strval(mode, name, value);
	}
    }
}

/* print_time - print relative time parameter */

static void print_time(int mode, const char *name, CONFIG_TIME_TABLE *ctt)
{
    const char *value;

    if (mode & SHOW_DEFS) {
	show_strval(mode, name, ctt->defval);
    } else {
	value = dict_lookup(CONFIG_DICT, name);
	if ((mode & SHOW_NONDEF) == 0) {
	    if (value == 0) {
		show_strval(mode, name, ctt->defval);
	    } else {
		show_strval(mode, name, value);
	    }
	} else {
	    if (value != 0)
		show_strval(mode, name, value);
	}
    }
}

/* print_int - print integer parameter */

static void print_int(int mode, const char *name, CONFIG_INT_TABLE *cit)
{
    const char *value;

    if (mode & SHOW_DEFS) {
	show_intval(mode, name, cit->defval);
    } else {
	value = dict_lookup(CONFIG_DICT, name);
	if ((mode & SHOW_NONDEF) == 0) {
	    if (value == 0) {
		show_intval(mode, name, cit->defval);
	    } else {
		show_strval(mode, name, value);
	    }
	} else {
	    if (value != 0)
		show_strval(mode, name, value);
	}
    }
}

/* print_str - print string parameter */

static void print_str(int mode, const char *name, CONFIG_STR_TABLE *cst)
{
    const char *value;

    if (mode & SHOW_DEFS) {
	show_strval(mode, name, cst->defval);
    } else {
	value = dict_lookup(CONFIG_DICT, name);
	if ((mode & SHOW_NONDEF) == 0) {
	    if (value == 0) {
		show_strval(mode, name, cst->defval);
	    } else {
		show_strval(mode, name, value);
	    }
	} else {
	    if (value != 0)
		show_strval(mode, name, value);
	}
    }
}

/* print_str_fn - print string-function parameter */

static void print_str_fn(int mode, const char *name, CONFIG_STR_FN_TABLE *csft)
{
    const char *value;

    if (mode & SHOW_DEFS) {
	show_strval(mode, name, csft->defval());
    } else {
	value = dict_lookup(CONFIG_DICT, name);
	if ((mode & SHOW_NONDEF) == 0) {
	    if (value == 0) {
		show_strval(mode, name, csft->defval());
	    } else {
		show_strval(mode, name, value);
	    }
	} else {
	    if (value != 0)
		show_strval(mode, name, value);
	}
    }
}

/* print_str_fn_2 - print string-function parameter */

static void print_str_fn_2(int mode, const char *name, CONFIG_STR_FN_TABLE *csft)
{
    const char *value;

    if (mode & SHOW_DEFS) {
	show_strval(mode, name, csft->defval());
    } else {
	value = dict_lookup(CONFIG_DICT, name);
	if ((mode & SHOW_NONDEF) == 0) {
	    if (value == 0) {
		show_strval(mode, name, csft->defval());
	    } else {
		show_strval(mode, name, value);
	    }
	} else {
	    if (value != 0)
		show_strval(mode, name, value);
	}
    }
}

/* print_raw - print raw string parameter */

static void print_raw(int mode, const char *name, CONFIG_RAW_TABLE *rst)
{
    const char *value;

    if (mode & SHOW_EVAL)
	msg_warn("parameter %s expands at run-time", rst->name);
    mode &= ~SHOW_EVAL;

    if (mode & SHOW_DEFS) {
	show_strval(mode, name, rst->defval);
    } else {
	value = dict_lookup(CONFIG_DICT, name);
	if ((mode & SHOW_NONDEF) == 0) {
	    if (value == 0) {
		show_strval(mode, name, rst->defval);
	    } else {
		show_strval(mode, name, value);
	    }
	} else {
	    if (value != 0)
		show_strval(mode, name, value);
	}
    }
}

/* print_nint - print new integer parameter */

static void print_nint(int mode, const char *name, CONFIG_NINT_TABLE *rst)
{
    const char *value;

    if (mode & SHOW_EVAL)
	msg_warn("parameter %s expands at run-time", name);
    mode &= ~SHOW_EVAL;

    if (mode & SHOW_DEFS) {
	show_strval(mode, name, rst->defval);
    } else {
	value = dict_lookup(CONFIG_DICT, name);
	if ((mode & SHOW_NONDEF) == 0) {
	    if (value == 0) {
		show_strval(mode, name, rst->defval);
	    } else {
		show_strval(mode, name, value);
	    }
	} else {
	    if (value != 0)
		show_strval(mode, name, value);
	}
    }
}

/* print_nbool - print new boolean parameter */

static void print_nbool(int mode, const char *name, CONFIG_NBOOL_TABLE *bst)
{
    const char *value;

    if (mode & SHOW_EVAL)
	msg_warn("parameter %s expands at run-time", name);
    mode &= ~SHOW_EVAL;

    if (mode & SHOW_DEFS) {
	show_strval(mode, name, bst->defval);
    } else {
	value = dict_lookup(CONFIG_DICT, name);
	if ((mode & SHOW_NONDEF) == 0) {
	    if (value == 0) {
		show_strval(mode, name, bst->defval);
	    } else {
		show_strval(mode, name, value);
	    }
	} else {
	    if (value != 0)
		show_strval(mode, name, value);
	}
    }
}

/* print_long - print long parameter */

static void print_long(int mode, const char *name, CONFIG_LONG_TABLE *clt)
{
    const char *value;

    if (mode & SHOW_DEFS) {
	show_longval(mode, name, clt->defval);
    } else {
	value = dict_lookup(CONFIG_DICT, name);
	if ((mode & SHOW_NONDEF) == 0) {
	    if (value == 0) {
		show_longval(mode, name, clt->defval);
	    } else {
		show_strval(mode, name, value);
	    }
	} else {
	    if (value != 0)
		show_strval(mode, name, value);
	}
    }
}

static void print_parameter(int, const char *, char *);

/* print_service_param_default show service parameter default value */

static void print_service_param_default(int mode, const char *name,
					        const char *defparam)
{
    const char *myname = "print_service_param_default";
    char   *ptr;

    if ((ptr = htable_find(param_table, defparam)) == 0)
	msg_panic("%s: service parameter %s has unknown default value $%s",
		  myname, name, defparam);
    if (mode & SHOW_EVAL)
	print_parameter(mode, name, ptr);
    else
	print_line(mode, "%s = $%s\n", name, defparam);
}

/* print_service_param - show service parameter */

static void print_service_param(int mode, const char *name,
				        const PC_STRING_NV *dst)
{
    const char *value;

    if (mode & SHOW_DEFS) {
	print_service_param_default(mode, name, dst->value);
    } else {
	value = dict_lookup(CONFIG_DICT, name);
	if ((mode & SHOW_NONDEF) == 0) {
	    if (value == 0) {
		print_service_param_default(mode, name, dst->value);
	    } else {
		show_strval(mode, name, value);
	    }
	} else {
	    if (value != 0)
		show_strval(mode, name, value);
	}
    }
}

/* print_user_param - show user-defined parameter */

static void print_user_param(int mode, const char *name)
{
    const char *value;

    if (mode & SHOW_DEFS) {			/* can't happen */
	show_strval(mode, name, "");
    } else {
	value = dict_lookup(CONFIG_DICT, name);
	if ((mode & SHOW_NONDEF) == 0) {
	    if (value == 0) {			/* can't happen */
		show_strval(mode, name, "");
	    } else {
		show_strval(mode, name, value);
	    }
	} else {
	    if (value != 0)
		show_strval(mode, name, value);
	}
    }
}

/* print_parameter - show specific parameter */

static void print_parameter(int mode, const char *name, char *ptr)
{

#define INSIDE(p,t) (ptr >= (char *) t && ptr < ((char *) t) + sizeof(t))
#define INSIDE3(p,t,l) (ptr >= (char *) t && ptr < (char *) ((t) + (l)))

    /*
     * This is gross, but the best we can do on short notice.
     */
    if (INSIDE(ptr, time_table))
	print_time(mode, name, (CONFIG_TIME_TABLE *) ptr);
    if (INSIDE(ptr, bool_table))
	print_bool(mode, name, (CONFIG_BOOL_TABLE *) ptr);
    if (INSIDE(ptr, int_table))
	print_int(mode, name, (CONFIG_INT_TABLE *) ptr);
    if (INSIDE(ptr, str_table))
	print_str(mode, name, (CONFIG_STR_TABLE *) ptr);
    if (INSIDE(ptr, str_fn_table))
	print_str_fn(mode, name, (CONFIG_STR_FN_TABLE *) ptr);
    if (INSIDE(ptr, str_fn_table_2))
	print_str_fn_2(mode, name, (CONFIG_STR_FN_TABLE *) ptr);
    if (INSIDE(ptr, raw_table))
	print_raw(mode, name, (CONFIG_RAW_TABLE *) ptr);
    if (INSIDE(ptr, nint_table))
	print_nint(mode, name, (CONFIG_NINT_TABLE *) ptr);
    if (INSIDE(ptr, nbool_table))
	print_nbool(mode, name, (CONFIG_NBOOL_TABLE *) ptr);
    if (INSIDE(ptr, long_table))
	print_long(mode, name, (CONFIG_LONG_TABLE *) ptr);
    if (INSIDE3(ptr, serv_param_table, serv_param_tablen))
	print_service_param(mode, name, (PC_STRING_NV *) ptr);
    if (INSIDE3(ptr, user_param_table, user_param_tablen))
	print_user_param(mode, name);
    if (msg_verbose)
	vstream_fflush(VSTREAM_OUT);
}

/* comp_names - qsort helper */

static int comp_names(const void *a, const void *b)
{
    HTABLE_INFO **ap = (HTABLE_INFO **) a;
    HTABLE_INFO **bp = (HTABLE_INFO **) b;

    return (strcmp(ap[0]->key, bp[0]->key));
}

/* show_parameters - show parameter info */

static void show_parameters(int mode, char **names)
{
    HTABLE_INFO **list;
    HTABLE_INFO **ht;
    char  **namep;
    char   *value;

    /*
     * Show all parameters.
     */
    if (*names == 0) {
	list = htable_list(param_table);
	qsort((char *) list, param_table->used, sizeof(*list), comp_names);
	for (ht = list; *ht; ht++)
	    print_parameter(mode, ht[0]->key, ht[0]->value);
	myfree((char *) list);
	return;
    }

    /*
     * Show named parameters.
     */
    for (namep = names; *namep; namep++) {
	if ((value = htable_find(param_table, *namep)) == 0) {
	    msg_warn("%s: unknown parameter", *namep);
	} else {
	    print_parameter(mode, *namep, value);
	}
    }
}

/* show_maps - show available maps */

static void show_maps(void)
{
    ARGV   *maps_argv;
    int     i;

    maps_argv = dict_mapnames();
    for (i = 0; i < maps_argv->argc; i++)
	vstream_printf("%s\n", maps_argv->argv[i]);
    argv_free(maps_argv);
}

/* show_locks - show available mailbox locking methods */

static void show_locks(void)
{
    ARGV   *locks_argv;
    int     i;

    locks_argv = mbox_lock_names();
    for (i = 0; i < locks_argv->argc; i++)
	vstream_printf("%s\n", locks_argv->argv[i]);
    argv_free(locks_argv);
}

/* print_master_line - print one master line */

static void print_master_line(int mode, ARGV *argv)
{
    char   *arg;
    char   *aval;
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
	vstream_fputs(text, VSTREAM_OUT); line_len += len; } \
    while (0)
#define ADD_SPACE ADD_TEXT(" ", 1)

    /*
     * Show the standard fields at their preferred column position. Use
     * single-space separation when some field does not fit.
     */
    for (line_len = 0, field = 0; field < MASTER_FIELD_COUNT; field++) {
	arg = argv->argv[field];
	if (line_len > 0) {
	    while (line_len < column_goal[field] - 1)
		ADD_SPACE;
	    ADD_SPACE;
	}
	ADD_TEXT(arg, strlen(arg));
    }

    /*
     * Format the daemon command-line options and non-option arguments. Here,
     * we have no data-dependent preference for column positions, but we do
     * have argument grouping preferences.
     */
    in_daemon_options = 1;
    for ( /* void */ ; argv->argv[field] != 0; field++) {
	arg = argv->argv[field];
	if (in_daemon_options) {

	    /*
	     * Try to show the generic options (-v -D) on the first line, and
	     * non-options on a later line.
	     */
	    if (arg[0] != '-') {
		in_daemon_options = 0;
		if ((mode & FOLD_LINE)
		    && line_len > column_goal[MASTER_FIELD_COUNT - 1]) {
		    vstream_fputs("\n" INDENT_TEXT, VSTREAM_OUT);
		    line_len = INDENT_LEN;
		}
	    }

	    /*
	     * Try to avoid breaking "-o name=value" over multiple lines if
	     * it would fit on one line.
	     */
	    else if ((mode & FOLD_LINE)
		     && line_len > INDENT_LEN && strcmp(arg, "-o") == 0
		     && (aval = argv->argv[field + 1]) != 0
		     && INDENT_LEN + 3 + strlen(aval) < LINE_LIMIT) {
		vstream_fputs("\n" INDENT_TEXT, VSTREAM_OUT);
		line_len = INDENT_LEN;
		ADD_TEXT(arg, strlen(arg));
		arg = aval;
		field += 1;
	    }
	}

	/*
	 * Insert a line break when the next argument won't fit (unless, of
	 * course, we just inserted a line break).
	 */
	if (line_len > INDENT_LEN) {
	    if ((mode & FOLD_LINE) == 0
		|| line_len + 1 + strlen(arg) < LINE_LIMIT) {
		ADD_SPACE;
	    } else {
		vstream_fputs("\n" INDENT_TEXT, VSTREAM_OUT);
		line_len = INDENT_LEN;
	    }
	}
	ADD_TEXT(arg, strlen(arg));
    }
    vstream_fputs("\n", VSTREAM_OUT);
}

/* show_master - show master.cf entries */

static void show_master(int mode, char **filters)
{
    ARGV  **argvp;
    ARGV   *argv;
    VSTRING *service_name = 0;
    ARGV   *service_filter = 0;

    /*
     * Initialize the service filter.
     */
    if (filters[0]) {
	service_name = vstring_alloc(10);
	service_filter = match_service_init_argv(filters);
    }

    /*
     * Iterate over the master table.
     */
    for (argvp = master_table; (argv = *argvp) != 0; argvp++) {
	if (service_filter) {
	    vstring_sprintf(service_name, "%s.%s",
			    argv->argv[0], argv->argv[1]);
	    if (match_service_match(service_filter, STR(service_name)) == 0)
		continue;
	}
	print_master_line(mode, argv);
    }
    if (service_filter) {
	argv_free(service_filter);
	vstring_free(service_name);
    }
}

/* show_sasl - show SASL plug-in types */

static void show_sasl(int what)
{
    ARGV   *sasl_argv;
    int     i;

    sasl_argv = (what & SHOW_SASL_SERV) ? xsasl_server_types() :
	xsasl_client_types();
    for (i = 0; i < sasl_argv->argc; i++)
	vstream_printf("%s\n", sasl_argv->argv[i]);
    argv_free(sasl_argv);
}

/* flag_unused_main_parameters - warn about unused parameters */

static void flag_unused_main_parameters(void)
{
    const char *myname = "flag_unused_main_parameters";
    DICT   *dict;
    const char *param_name;
    const char *param_value;
    int     how;

    /*
     * Iterate over all main.cf entries, and flag parameter names that aren't
     * used anywhere. Show the warning message at the end of the output.
     */
    if ((dict = dict_handle(CONFIG_DICT)) == 0)
	msg_panic("%s: parameter dictionary %s not found",
		  myname, CONFIG_DICT);
    if (dict->sequence == 0)
	msg_panic("%s: parameter dictionary %s has no iterator",
		  myname, CONFIG_DICT);
    for (how = DICT_SEQ_FUN_FIRST;
	 dict->sequence(dict, how, &param_name, &param_value) == 0;
	 how = DICT_SEQ_FUN_NEXT) {
	if (htable_locate(param_table, param_name) == 0) {
	    vstream_fflush(VSTREAM_OUT);
	    msg_warn("%s/" MAIN_CONF_FILE ": unused parameter: %s=%s",
		     var_config_dir, param_name, param_value);
	}
    }
}

/* flag_unused_master_parameters - warn about unused parameters */

static void flag_unused_master_parameters(void)
{
    ARGV  **argvp;
    ARGV   *argv;
    int     field;
    char   *arg;
    char   *saved_arg;
    char   *param_name;
    char   *param_value;

    /*
     * Iterate over all master.cf entries, and flag parameter names that
     * aren't used anywhere. Show the warning message at the end of the
     * output.
     * 
     * XXX It is possible that a user-defined parameter is defined in master.cf
     * with "-o smtpd_restriction_classes=name", or with "-o name1=value1"
     * and then used in a "-o name2=$name1" macro expansion in that same
     * master.cf entry. To handle this we need to give each master.cf entry
     * its own name space. Until then, we always log "unused parameter"
     * warnings for "-o user-defined-name=value" entries.
     */
    for (argvp = master_table; (argv = *argvp) != 0; argvp++) {
	for (field = MASTER_FIELD_COUNT; argv->argv[field] != 0; field++) {
	    arg = argv->argv[field];
	    if (arg[0] != '-')
		break;
	    if (strcmp(arg, "-o") == 0 && (arg = argv->argv[field + 1]) != 0) {
		saved_arg = mystrdup(arg);
		if (split_nameval(saved_arg, &param_name, &param_value) == 0
		    && htable_locate(param_table, param_name) == 0) {
		    vstream_fflush(VSTREAM_OUT);
		    msg_warn("%s/" MASTER_CONF_FILE ": unused parameter: %s=%s",
			     var_config_dir, param_name, param_value);
		}
		myfree(saved_arg);
		field += 1;
	    }
	}
    }
}

MAIL_VERSION_STAMP_DECLARE;

/* main */

int     main(int argc, char **argv)
{
    int     ch;
    int     fd;
    struct stat st;
    int     junk;
    ARGV   *ext_argv = 0;

    /*
     * Fingerprint executables and core dumps.
     */
    MAIL_VERSION_STAMP_ALLOCATE;

    /*
     * Be consistent with file permissions.
     */
    umask(022);

    /*
     * To minimize confusion, make sure that the standard file descriptors
     * are open before opening anything else. XXX Work around for 44BSD where
     * fstat can return EBADF on an open file descriptor.
     */
    for (fd = 0; fd < 3; fd++)
	if (fstat(fd, &st) == -1
	    && (close(fd), open("/dev/null", O_RDWR, 0)) != fd)
	    msg_fatal("open /dev/null: %m");

    /*
     * Set up logging.
     */
    msg_vstream_init(argv[0], VSTREAM_ERR);

    /*
     * Parse JCL.
     */
    while ((ch = GETOPT(argc, argv, "aAbc:deEf#hlmMntv")) > 0) {
	switch (ch) {
	case 'a':
	    cmd_mode |= SHOW_SASL_SERV;
	    break;
	case 'A':
	    cmd_mode |= SHOW_SASL_CLNT;
	    break;
	case 'b':
	    if (ext_argv)
		msg_fatal("specify one of -b and -t");
	    ext_argv = argv_alloc(2);
	    argv_add(ext_argv, "bounce", "-SVnexpand_templates", (char *) 0);
	    break;
	case 'c':
	    if (setenv(CONF_ENV_PATH, optarg, 1) < 0)
		msg_fatal("out of memory");
	    break;
	case 'd':
	    cmd_mode |= SHOW_DEFS;
	    break;
	case 'e':
	    cmd_mode |= EDIT_MAIN;
	    break;
	case 'f':
	    cmd_mode |= FOLD_LINE;
	    break;

	    /*
	     * People, this does not work unless you properly handle default
	     * settings. For example, fast_flush_domains = $relay_domains
	     * must not evaluate to the empty string when relay_domains is
	     * left at its default setting of $mydestination.
	     */
#if 0
	case 'E':
	    cmd_mode |= SHOW_EVAL;
	    break;
#endif
	case '#':
	    cmd_mode = COMMENT_OUT;
	    break;

	case 'h':
	    cmd_mode &= ~SHOW_NAME;
	    break;
	case 'l':
	    cmd_mode |= SHOW_LOCKS;
	    break;
	case 'm':
	    cmd_mode |= SHOW_MAPS;
	    break;
	case 'M':
	    cmd_mode |= SHOW_MASTER;
	    break;
	case 'n':
	    cmd_mode |= SHOW_NONDEF;
	    break;
	case 't':
	    if (ext_argv)
		msg_fatal("specify one of -b and -t");
	    ext_argv = argv_alloc(2);
	    argv_add(ext_argv, "bounce", "-SVndump_templates", (char *) 0);
	    break;
	case 'v':
	    msg_verbose++;
	    break;
	default:
	    msg_fatal("usage: %s [-a (server SASL types)] [-A (client SASL types)] [-b (bounce templates)] [-c config_dir] [-d (defaults)] [-e (edit)] [-f (fold lines)] [-# (comment-out)] [-h (no names)] [-l (lock types)] [-m (map types)] [-M (master.cf)] [-n (non-defaults)] [-v] [name...]", argv[0]);
	}
    }

    /*
     * Sanity check.
     */
    junk = (cmd_mode & (SHOW_DEFS | SHOW_NONDEF | SHOW_MAPS | SHOW_LOCKS | EDIT_MAIN | SHOW_SASL_SERV | SHOW_SASL_CLNT | COMMENT_OUT | SHOW_MASTER));
    if (junk != 0 && ((junk != SHOW_DEFS && junk != SHOW_NONDEF
	     && junk != SHOW_MAPS && junk != SHOW_LOCKS && junk != EDIT_MAIN
		       && junk != SHOW_SASL_SERV && junk != SHOW_SASL_CLNT
		       && junk != COMMENT_OUT && junk != SHOW_MASTER)
		      || ext_argv != 0))
	msg_fatal("specify one of -a, -A, -b, -d, -e, -#, -l, -m, -M and -n");

    /*
     * Display bounce template information and exit.
     */
    if (ext_argv) {
	if (argv[optind]) {
	    if (argv[optind + 1])
		msg_fatal("options -b and -t require at most one template file");
	    argv_add(ext_argv, "-o",
		     concatenate(VAR_BOUNCE_TMPL, "=",
				 argv[optind], (char *) 0),
		     (char *) 0);
	}
	/* Grr... */
	argv_add(ext_argv, "-o",
		 concatenate(VAR_QUEUE_DIR, "=", ".", (char *) 0),
		 (char *) 0);
	mail_conf_read();
	mail_run_replace(var_daemon_dir, ext_argv->argv);
	/* NOTREACHED */
    }

    /*
     * If showing map types, show them and exit
     */
    if (cmd_mode & SHOW_MAPS) {
	mail_dict_init();
	show_maps();
    }

    /*
     * If showing locking methods, show them and exit
     */
    else if (cmd_mode & SHOW_LOCKS) {
	show_locks();
    }

    /*
     * If showing master.cf entries, show them and exit
     */
    else if (cmd_mode & SHOW_MASTER) {
	read_master();
	show_master(cmd_mode, argv + optind);
    }

    /*
     * If showing SASL plug-in types, show them and exit
     */
    else if (cmd_mode & SHOW_SASL_SERV) {
	show_sasl(SHOW_SASL_SERV);
    } else if (cmd_mode & SHOW_SASL_CLNT) {
	show_sasl(SHOW_SASL_CLNT);
    }

    /*
     * Edit main.cf.
     */
    else if (cmd_mode & (EDIT_MAIN | COMMENT_OUT)) {
	edit_parameters(cmd_mode, argc - optind, argv + optind);
    } else if (cmd_mode == DEF_MODE
	       && argv[optind] && strchr(argv[optind], '=')) {
	edit_parameters(cmd_mode | EDIT_MAIN, argc - optind, argv + optind);
    }

    /*
     * If showing non-default values, read main.cf.
     */
    else {
	if ((cmd_mode & SHOW_DEFS) == 0) {
	    read_parameters();
	    set_parameters();
	}
	hash_parameters();

	/*
	 * Add service-dependent parameters (service names from master.cf)
	 * and user-defined parameters ($name macros in parameter values in
	 * main.cf and master.cf).
	 */
	read_master();
	add_service_parameters();
	if ((cmd_mode & SHOW_DEFS) == 0)
	    add_user_parameters();

	/*
	 * Show the requested values.
	 */
	show_parameters(cmd_mode, argv + optind);

	/*
	 * Flag unused parameters. This makes no sense with "postconf -d",
	 * because that ignores all the user-specified parameters and
	 * user-specified macro expansions in main.cf.
	 */
	if ((cmd_mode & SHOW_DEFS) == 0) {
	    flag_unused_main_parameters();
	    flag_unused_master_parameters();
	}
    }
    vstream_fflush(VSTREAM_OUT);
    exit(0);
}
