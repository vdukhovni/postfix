/*++
/* NAME
/*	postconf 1
/* SUMMARY
/*	Postfix configuration utility
/* SYNOPSIS
/* .fi
/*	\fBManaging main.cf:\fR
/*
/*	\fBpostconf\fR [\fB-dfhnovx\fR] [\fB-c \fIconfig_dir\fR]
/*	[\fB-C \fIclass,...\fR] [\fIparameter ...\fR]
/*
/*	\fBpostconf\fR [\fB-ev\fR] [\fB-c \fIconfig_dir\fR]
/*	\fIparameter=value ...\fR
/*
/*	\fBpostconf\fR [\fB-#vX\fR] [\fB-c \fIconfig_dir\fR]
/*	\fIparameter ...\fR
/*
/*	\fBManaging master.cf service entries:\fR
/*
/*	\fBpostconf\fR [\fB-fMovx\fR] [\fB-c \fIconfig_dir\fR]
/*	[\fIservice\fR[\fB/\fItype\fR]\fI ...\fR]
/*
/*	\fBpostconf\fR [\fB-eMv\fR] [\fB-c \fIconfig_dir\fR]
/*	\fIservice\fB/\fItype=value ...\fR
/*
/*	\fBpostconf\fR [\fB-#MvX\fR] [\fB-c \fIconfig_dir\fR]
/*	\fIservice\fB/\fItype ...\fR
/*
/*	\fBManaging master.cf service fields:\fR
/*
/*	\fBpostconf\fR [\fB-fFovx\fR] [\fB-c \fIconfig_dir\fR]
/*	[\fIservice\fR[\fB/\fItype\fR[\fB/\fIfield\fR]]\fI ...\fR]
/*
/*	\fBpostconf\fR [\fB-eFv\fR] [\fB-c \fIconfig_dir\fR]
/*	\fIservice\fB/\fItype\fB/\fIfield=value ...\fR
/*
/*	\fBManaging master.cf service parameters:\fR
/*
/*	\fBpostconf\fR [\fB-fPovx\fR] [\fB-c \fIconfig_dir\fR]
/*	[\fIservice\fR[\fB/\fItype\fR[\fB/\fIparameter\fR]]\fI ...\fR]
/*
/*	\fBpostconf\fR [\fB-ePv\fR] [\fB-c \fIconfig_dir\fR]
/*	\fIservice\fB/\fItype\fB/\fIparameter=value ...\fR
/*
/*	\fBpostconf\fR [\fB-PXv\fR] [\fB-c \fIconfig_dir\fR]
/*	\fIservice\fB/\fItype\fB/\fIparameter ...\fR
/*
/*	\fBManaging bounce message templates:\fR
/*
/*	\fBpostconf\fR [\fB-btv\fR] [\fB-c \fIconfig_dir\fR] [\fItemplate_file\fR]
/*
/*	\fBManaging other configuration:\fR
/*
/*	\fBpostconf\fR [\fB-aAlmv\fR] [\fB-c \fIconfig_dir\fR]
/* DESCRIPTION
/*	By default, the \fBpostconf\fR(1) command displays the
/*	values of \fBmain.cf\fR configuration parameters, and warns
/*	about possible mis-typed parameter names (Postfix 2.9 and later).
/*	It can also change \fBmain.cf\fR configuration
/*	parameter values, or display other configuration information
/*	about the Postfix mail system.
/*
/*	Options:
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
/* .IP \fB-A\fR
/*	List the available SASL client plug-in types.  The SASL
/*	plug-in type is selected with the \fBsmtp_sasl_type\fR or
/*	\fBlmtp_sasl_type\fR configuration parameters by specifying
/*	one of the names listed below.
/* .RS
/* .IP \fBcyrus\fR
/*	This client plug-in is available when Postfix is built with
/*	Cyrus SASL support.
/* .RE
/* .IP
/*	This feature is available with Postfix 2.3 and later.
/* .IP "\fB-b\fR [\fItemplate_file\fR]"
/*	Display the message text that appears at the beginning of
/*	delivery status notification (DSN) messages, replacing
/*	$\fBname\fR expressions with actual values as described in
/*	\fBbounce\fR(5).
/*
/*	To override the built-in templates, specify a template file
/*	name at the end of the \fBpostconf\fR(1) command line, or
/*	specify a file name in \fBmain.cf\fR with the
/*	\fBbounce_template_file\fR parameter.
/*
/*	To force selection of the built-in templates, specify an
/*	empty template file name on the \fBpostconf\fR(1) command
/*	line (in shell language: "").
/*
/*	This feature is available with Postfix 2.3 and later.
/* .IP "\fB-c \fIconfig_dir\fR"
/*	The \fBmain.cf\fR configuration file is in the named directory
/*	instead of the default configuration directory.
/* .IP "\fB-C \fIclass,...\fR"
/*	When displaying \fBmain.cf\fR parameters, select only
/*	parameters from the specified class(es):
/* .RS
/* .IP \fBbuiltin\fR
/*	Parameters with built-in names.
/* .IP \fBservice\fR
/*	Parameters with service-defined names (the first field of
/*	a \fBmaster.cf\fR entry plus a Postfix-defined suffix).
/* .IP \fBuser\fR
/*	Parameters with user-defined names.
/* .IP \fBall\fR
/*	All the above classes.
/* .RE
/* .IP
/*	The default is as if "\fB-C all\fR" is
/*	specified.
/* .IP \fB-d\fR
/*	Print \fBmain.cf\fR default parameter settings instead of
/*	actual settings.
/*	Specify \fB-df\fR to fold long lines for human readability
/*	(Postfix 2.9 and later).
/* .IP \fB-e\fR
/*	Edit the \fBmain.cf\fR configuration file, and update
/*	parameter settings with the "\fIname=value\fR" pairs on the
/*	\fBpostconf\fR(1) command line.
/*
/*	With \fB-M\fR, edit the \fBmaster.cf\fR configuration file,
/*	and replace one or more service entries with new values as
/*	specified with "\fIservice/type=value\fR" on the \fBpostconf\fR(1)
/*	command line.
/*
/*	With \fB-F\fR, edit the \fBmaster.cf\fR configuration file,
/*	and replace one or more service fields with new values as
/*	specied with "\fIservice/type/field=value\fR" on the
/*	\fBpostconf\fR(1) command line. Currently, the "command"
/*	field contains the command name and command arguments.  this
/*	may change in the near future, so that the "command" field
/*	contains only the command name, and a new "arguments"
/*	pseudofield contains the command arguments.
/*
/*	With \fB-P\fR, edit the \fBmaster.cf\fR configuration file,
/*	and add or update one or more service parameter settings
/*	(-o parameter=value settings) with new values as specied
/*	with "\fIservice/type/parameter=value\fR" on the \fBpostconf\fR(1)
/*	command line.
/*
/*	In all cases the file is copied to a temporary file then
/*	renamed into place.  Specify quotes to protect special
/*	characters and whitespace on the \fBpostconf\fR(1) command
/*	line.
/*
/*	The \fB-e\fR option is no longer needed with Postfix version
/*	2.8 and later.
/* .IP \fB-f\fR
/*	Fold long lines when printing \fBmain.cf\fR or \fBmaster.cf\fR
/*	configuration file entries, for human readability.
/*
/*	This feature is available with Postfix 2.9 and later.
/* .IP \fB-F\fR
/*	Show \fBmaster.cf\fR per-entry field settings (by default
/*	all services and all fields), formatted as one
/*	"\fIservice/type/field=value\fR" per line. Specify \fB-Ff\fR
/*	to fold long lines.
/*
/*	Specify one or more "\fIservice/type/field\fR" instances
/*	on the \fBpostconf\fR(1) command line to limit the output
/*	to fields of interest.  Trailing parameter name or service
/*	type fields that are omitted will be handled as "*" wildcard
/*	fields.
/*
/*	This feature is available with Postfix 2.11 and later.
/* .IP \fB-h\fR
/*	Show parameter or attribute values without the "\fIname\fR
/*	= " label that normally precedes the value.
/* .IP \fB-l\fR
/*	List the names of all supported mailbox locking methods.
/*	Postfix supports the following methods:
/* .RS
/* .IP \fBflock\fR
/*	A kernel-based advisory locking method for local files only.
/*	This locking method is available on systems with a BSD
/*	compatible library.
/* .IP \fBfcntl\fR
/*	A kernel-based advisory locking method for local and remote
/*	files.
/* .IP \fBdotlock\fR
/*	An application-level locking method. An application locks
/*	a file named \fIfilename\fR by creating a file named
/*	\fIfilename\fB.lock\fR.  The application is expected to
/*	remove its own lock file, as well as stale lock files that
/*	were left behind after abnormal program termination.
/* .RE
/* .IP \fB-m\fR
/*	List the names of all supported lookup table types. In
/*	Postfix configuration files, lookup tables are specified
/*	as \fItype\fB:\fIname\fR, where \fItype\fR is one of the
/*	types listed below. The table \fIname\fR syntax depends on
/*	the lookup table type as described in the DATABASE_README
/*	document.
/* .RS
/* .IP \fBbtree\fR
/*	A sorted, balanced tree structure.  Available on systems
/*	with support for Berkeley DB databases.
/* .IP \fBcdb\fR
/*	A read-optimized structure with no support for incremental
/*	updates.  Available on systems with support for CDB databases.
/* .IP \fBcidr\fR
/*	A table that associates values with Classless Inter-Domain
/*	Routing (CIDR) patterns. This is described in \fBcidr_table\fR(5).
/* .IP \fBdbm\fR
/*	An indexed file type based on hashing.  Available on systems
/*	with support for DBM databases.
/* .IP \fBenviron\fR
/*	The UNIX process environment array. The lookup key is the
/*	variable name. Originally implemented for testing, someone
/*	may find this useful someday.
/* .IP \fBfail\fR
/*	A table that reliably fails all requests. The lookup table
/*	name is used for logging. This table exists to simplify
/*	Postfix error tests.
/* .IP \fBhash\fR
/*	An indexed file type based on hashing.  Available on systems
/*	with support for Berkeley DB databases.
/* .IP \fBinternal\fR
/*	A non-shared, in-memory hash table. Its content are lost
/*	when a process terminates.
/* .IP "\fBlmdb\fR"
/*	OpenLDAP LMDB database (a memory-mapped, persistent file).
/*	Available on systems with support for LMDB databases.  This
/*	is described in \fBlmdb_table\fR(5).
/* .IP "\fBldap\fR (read-only)"
/*	LDAP database client. This is described in \fBldap_table\fR(5).
/* .IP "\fBmemcache\fR"
/*	Memcache database client. This is described in
/*	\fBmemcache_table\fR(5).
/* .IP "\fBmysql\fR (read-only)"
/*	MySQL database client.  Available on systems with support
/*	for MySQL databases.  This is described in \fBmysql_table\fR(5).
/* .IP "\fBpcre\fR (read-only)"
/*	A lookup table based on Perl Compatible Regular Expressions.
/*	The file format is described in \fBpcre_table\fR(5).
/* .IP "\fBpgsql\fR (read-only)"
/*	PostgreSQL database client. This is described in
/*	\fBpgsql_table\fR(5).
/* .IP "\fBproxy\fR"
/*	Postfix \fBproxymap\fR(8) client for shared access to Postfix
/*	databases. The table name syntax is \fItype\fB:\fIname\fR.
/* .IP "\fBregexp\fR (read-only)"
/*	A lookup table based on regular expressions. The file format
/*	is described in \fBregexp_table\fR(5).
/* .IP \fBsdbm\fR
/*	An indexed file type based on hashing.  Available on systems
/*	with support for SDBM databases.
/* .IP "\fBsocketmap\fR (read-only)"
/*	Sendmail-style socketmap client. The table name is
/*	\fBinet\fR:\fIhost\fR:\fIport\fR:\fIname\fR for a TCP/IP
/*	server, or \fBunix\fR:\fIpathname\fR:\fIname\fR for a
/*	UNIX-domain server. This is described in \fBsocketmap_table\fR(5).
/* .IP "\fBsqlite\fR (read-only)"
/*	SQLite database. This is described in \fBsqlite_table\fR(5).
/* .IP "\fBstatic\fR (read-only)"
/*	A table that always returns its name as lookup result. For
/*	example, \fBstatic:foobar\fR always returns the string
/*	\fBfoobar\fR as lookup result.
/* .IP "\fBtcp\fR (read-only)"
/*	TCP/IP client. The protocol is described in \fBtcp_table\fR(5).
/* .IP "\fBtexthash\fR (read-only)"
/*	Produces similar results as hash: files, except that you
/*	don't need to run the \fBpostmap\fR(1) command before you
/*	can use the file, and that it does not detect changes after
/*	the file is read.
/* .IP "\fBunix\fR (read-only)"
/*	A limited view of the UNIX authentication database. The
/*	following tables are implemented:
/* .RS
/*. IP \fBunix:passwd.byname\fR
/*	The table is the UNIX password database. The key is a login
/*	name.  The result is a password file entry in \fBpasswd\fR(5)
/*	format.
/* .IP \fBunix:group.byname\fR
/*	The table is the UNIX group database. The key is a group
/*	name.  The result is a group file entry in \fBgroup\fR(5)
/*	format.
/* .RE
/* .RE
/* .IP
/*	Other table types may exist depending on how Postfix was
/*	built.
/* .IP \fB-M\fR
/*	Show \fBmaster.cf\fR file contents instead of \fBmain.cf\fR
/*	file contents.  Specify \fB-Mf\fR to fold long lines for
/*	human readability.
/*
/*	Specify zero or more arguments, each with a \fIservice-name\fR
/*	or \fIservice-name/service-type\fR pair, where \fIservice-name\fR
/*	is the first field of a master.cf entry and \fIservice-type\fR
/*	is one of (\fBinet\fR, \fBunix\fR, \fBfifo\fR, or \fBpass\fR).
/*
/*	If \fIservice-name\fR or \fIservice-name/service-type\fR
/*	is specified, only the matching master.cf entries will be
/*	output. For example, "\fBpostconf -Mf smtp\fR" will output
/*	all services named "smtp", and "\fBpostconf -Mf smtp/inet\fR"
/*	will output only the smtp service that listens on the
/*	network.  Trailing service type fields that are omitted
/*	will be handled as "*" wildcard fields.
/*
/*	This feature is available with Postfix 2.9 and later. The
/*	syntax was changed from "\fIname.type\fR" to "\fIname/type\fR",
/*	and "*" wildcard support was added with Postfix 2.11.
/* .IP \fB-n\fR
/*	Show only configuration parameters that have explicit
/*	\fIname=value\fR settings in \fBmain.cf\fR.  Specify \fB-nf\fR
/*	to fold long lines for human readability (Postfix 2.9 and
/*	later).
/* .IP "\fB-o \fIname=value\fR"
/*	Override \fBmain.cf\fR parameter settings.
/*
/*	This feature is available with Postfix 2.10 and later.
/* .IP \fB-p\fR
/*	Show \fBmain.cf\fR parameter settings. This is the default.
/* .IP \fB-P\fR
/*	Show \fBmaster.cf\fR service parameter settings (by default
/*	all services and all parameters).  formatted as one
/*	"\fIservice/type/parameter=value\fR" per line.  Specify
/*	\fB-Pf\fR to fold long lines.
/*
/*	Specify one or more "\fIservice/type/parameter\fR" instances
/*	on the \fBpostconf\fR(1) command line to limit the output
/*	to parameters of interest.  Trailing parameter name or
/*	service type fields that are omitted will be handled as "*"
/*	wildcard fields.
/*
/*	This feature is available with Postfix 2.11 and later.
/* .IP "\fB-t\fR [\fItemplate_file\fR]"
/*	Display the templates for text that appears at the beginning
/*	of delivery status notification (DSN) messages, without
/*	expanding $\fBname\fR expressions.
/*
/*	To override the built-in templates, specify a template file
/*	name at the end of the \fBpostconf\fR(1) command line, or
/*	specify a file name in \fBmain.cf\fR with the
/*	\fBbounce_template_file\fR parameter.
/*
/*	To force selection of the built-in templates, specify an
/*	empty template file name on the \fBpostconf\fR(1) command
/*	line (in shell language: "").
/*
/*	This feature is available with Postfix 2.3 and later.
/* .IP \fB-v\fR
/*	Enable verbose logging for debugging purposes. Multiple
/*	\fB-v\fR options make the software increasingly verbose.
/* .IP \fB-x\fR
/*	Expand \fI$name\fR in \fBmain.cf\fR or \fBmaster.cf\fR
/*	parameter values. The expansion is recursive.
/*
/*	This feature is available with Postfix 2.10 and later.
/* .IP \fB-X\fR
/*	Edit the \fBmain.cf\fR configuration file, and remove the
/*	parameters named on the \fBpostconf\fR(1) command line.
/*	Specify a list of parameter names, not "\fIname=value\fR"
/*	pairs.
/*
/*	With \fB-M\fR, edit the \fBmaster.cf\fR configuration file,
/*	and remove one or more service entries as specified with
/*	"\fIservice/type\fR" on the \fBpostconf\fR(1) command line.
/*
/*	With \fB-P\fR, edit the \fBmaster.cf\fR configuration file,
/*	and remove one or more service parameter settings (-o
/*	parameter=value settings) as specied with
/*	"\fIservice/type/parameter\fR" on the \fBpostconf\fR(1)
/*	command line.
/*
/*	In all cases the file is copied to a temporary file then
/*	renamed into place.  Specify quotes to protect special
/*	characters on the \fBpostconf\fR(1) command line.
/*
/*	There is no \fBpostconf\fR(1) command to perform the reverse
/*	operation.
/*
/*	This feature is available with Postfix 2.10 and later.
/*	Support for -M and -P was added with Postfix 2.11.
/* .IP \fB-#\fR
/*	Edit the \fBmain.cf\fR configuration file, and comment out
/*	the parameters named on the \fBpostconf\fR(1) command line,
/*	so that those parameters revert to their default values.
/*	Specify a list of parameter names, not "\fIname=value\fR"
/*	pairs.
/*
/*	With \fB-M\fR, edit the \fBmaster.cf\fR configuration file,
/*	and comment out one or more service entries as specified
/*	with "\fIservice/type\fR" on the \fBpostconf\fR(1) command
/*	line.
/*
/*	In all cases the file is copied to a temporary file then
/*	renamed into place.  Specify quotes to protect special
/*	characters on the \fBpostconf\fR(1) command line.
/*
/*	There is no \fBpostconf\fR(1) command to perform the reverse
/*	operation.
/*
/*	This feature is available with Postfix 2.6 and later. Support
/*	for -M was added with Postfix 2.11.
/* DIAGNOSTICS
/*	Problems are reported to the standard error stream.
/* ENVIRONMENT
/* .ad
/* .fi
/* .IP \fBMAIL_CONFIG\fR
/*	Directory with Postfix configuration files.
/* CONFIGURATION PARAMETERS
/* .ad
/* .fi
/*	The following \fBmain.cf\fR parameters are especially
/*	relevant to this program.
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
/*	/etc/postfix/master.cf, Postfix master daemon configuration
/* SEE ALSO
/*	bounce(5), bounce template file format master(5), master.cf
/*	configuration file syntax postconf(5), main.cf configuration
/*	file syntax
/* README FILES
/* .ad
/* .fi
/*	Use "\fBpostconf readme_directory\fR" or "\fBpostconf
/*	html_directory\fR" to locate this information.
/* .na
/* .nf
/*	DATABASE_README, Postfix lookup table overview
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this
/*	software.
/* AUTHOR(S)
/*	Wietse Venema IBM T.J. Watson Research P.O. Box 704 Yorktown
/*	Heights, NY 10598, USA
/*--*/

/* System library. */

#include <sys_defs.h>
#include <sys/stat.h>
#include <stdlib.h>

/* Utility library. */

#include <msg.h>
#include <msg_vstream.h>
#include <dict.h>
#include <htable.h>
#include <vstring.h>
#include <vstream.h>
#include <stringops.h>
#include <name_mask.h>
#include <warn_stat.h>
#include <mymalloc.h>

/* Global library. */

#include <mail_params.h>
#include <mail_conf.h>
#include <mail_version.h>
#include <mail_run.h>
#include <mail_dict.h>

/* Application-specific. */

#include <postconf.h>

 /*
  * Global storage. See postconf.h for description.
  */
PC_PARAM_TABLE *param_table;
PC_MASTER_ENT *master_table;
int     cmd_mode = DEF_MODE;

 /*
  * Application fingerprinting.
  */
MAIL_VERSION_STAMP_DECLARE;

 /*
  * This program has so many command-line options that we have to implement a
  * compatibility matrix to weed out the conflicting option combinations, and
  * to alert the user about option combinations that have no effect.
  */

 /*
  * Options that are mutually-exclusive. First entry must specify the major
  * modes. Other entries specify conflicts between option modifiers.
  */
static const int incompat_options[] = {
    /* Major modes. */
    SHOW_SASL_SERV | SHOW_SASL_CLNT | EXP_DSN_TEMPL | SHOW_LOCKS | SHOW_MAPS \
    |DUMP_DSN_TEMPL | MAIN_PARAM | MASTER_ENTRY | MASTER_FIELD | MASTER_PARAM,
    /* Modifiers. */
    SHOW_DEFS | EDIT_CONF | SHOW_NONDEF | COMMENT_OUT | EDIT_EXCL,
    FOLD_LINE | EDIT_CONF | COMMENT_OUT | EDIT_EXCL,
    SHOW_EVAL | EDIT_CONF | COMMENT_OUT | EDIT_EXCL,
    MAIN_OVER | SHOW_DEFS | EDIT_CONF | COMMENT_OUT | EDIT_EXCL,
    HIDE_NAME | EDIT_CONF | COMMENT_OUT | EDIT_EXCL,
    0,
};

 /*
  * Options, and the only options that they are compatible with. There must
  * be one entry for each major mode. Other entries specify compatibility
  * between option modifiers.
  */
static const int compat_options[][2] = {
    /* Major modes. */
    {SHOW_SASL_SERV, 0},
    {SHOW_SASL_CLNT, 0},
    {EXP_DSN_TEMPL, 0},
    {SHOW_LOCKS, 0},
    {SHOW_MAPS, 0,},
    {DUMP_DSN_TEMPL, 0},
    {MAIN_PARAM, EDIT_CONF | EDIT_EXCL | COMMENT_OUT | FOLD_LINE | HIDE_NAME \
    |PARAM_CLASS | SHOW_EVAL | SHOW_DEFS | SHOW_NONDEF | MAIN_OVER},
    {MASTER_ENTRY, EDIT_CONF | EDIT_EXCL | COMMENT_OUT | FOLD_LINE | MAIN_OVER \
    |SHOW_EVAL},
    {MASTER_FIELD, EDIT_CONF | FOLD_LINE | HIDE_NAME | MAIN_OVER | SHOW_EVAL},
    {MASTER_PARAM, EDIT_CONF | EDIT_EXCL | FOLD_LINE | HIDE_NAME | MAIN_OVER \
    |SHOW_EVAL},
    /* Modifiers. */
    {PARAM_CLASS, MAIN_PARAM | SHOW_DEFS | SHOW_NONDEF},
    0,
};

 /*
  * Compatibility to string conversion support.
  */
static const NAME_MASK compat_names[] = {
    "-a", SHOW_SASL_SERV,
    "-A", SHOW_SASL_CLNT,
    "-b", EXP_DSN_TEMPL,
    "-C", PARAM_CLASS,
    "-d", SHOW_DEFS,
    "-e", EDIT_CONF,
    "-f", FOLD_LINE,
    "-F", MASTER_FIELD,
    "-h", HIDE_NAME,
    "-l", SHOW_LOCKS,
    "-m", SHOW_MAPS,
    "-M", MASTER_ENTRY,
    "-n", SHOW_NONDEF,
    "-o", MAIN_OVER,
    "-p", MAIN_PARAM,
    "-P", MASTER_PARAM,
    "-t", DUMP_DSN_TEMPL,
    "-x", SHOW_EVAL,
    "-X", EDIT_EXCL,
    "-#", COMMENT_OUT,
    0,
};

/* usage - enumerate parameters without compatibility info */

static void usage(const char *progname)
{
    msg_fatal("usage: %s"
	      " [-a (server SASL types)]"
	      " [-A (client SASL types)]"
	      " [-b (bounce templates)]"
	      " [-c config_dir]"
	      " [-c param_class]"
	      " [-d (parameter defaults)]"
	      " [-e (edit configuration)]"
	      " [-f (fold lines)]"
	      " [-F (master.cf fields)]"
	      " [-h (no names)]"
	      " [-l (lock types)]"
	      " [-m (map types)]"
	      " [-M (master.cf)]"
	      " [-n (non-default parameters)]"
	      " [-o name=value (override parameter value)]"
	      " [-p (main.cf, default)]"
	      " [-P (master.cf parameters)]"
	      " [-t (bounce templates)]"
	      " [-v (verbose)]"
	      " [-x (expand parameter values)]"
	      " [-X (exclude)]"
	      " [-# (comment-out)]"
	      " [name...]", progname);
}

/* check_exclusive_options - complain about mutually-exclusive options */

static void check_exclusive_options(int optval)
{
    const char *myname = "check_exclusive_options";
    const int *op;
    int     oval;
    unsigned mask;

    for (op = incompat_options; (oval = *op) != 0; op++) {
	oval &= optval;
	for (mask = ~0; (mask & oval) != 0; mask >>= 1) {
	    if ((mask & oval) != oval)
		msg_fatal("specify one of %s",
			  str_name_mask(myname, compat_names, oval));
	}
    }
}

/* check_compat_options - complain about incompatible options */

static void check_compat_options(int optval)
{
    const char *myname = "check_compat_options";
    VSTRING *buf1 = vstring_alloc(10);
    VSTRING *buf2 = vstring_alloc(10);
    const int (*op)[2];
    int     excess;

    for (op = compat_options; op[0][0] != 0; op++) {
	if ((optval & *op[0]) != 0
	    && (excess = (optval & ~((*op)[0] | (*op)[1]))) != 0)
	    msg_fatal("with option %s, do not specify %s",
		      str_name_mask_opt(buf1, myname, compat_names,
					(*op)[0], NAME_MASK_NUMBER),
		      str_name_mask_opt(buf2, myname, compat_names, excess,
					NAME_MASK_NUMBER));
    }
    vstring_free(buf1);
    vstring_free(buf2);
}

/* main */

int     main(int argc, char **argv)
{
    int     ch;
    int     fd;
    struct stat st;
    ARGV   *ext_argv = 0;
    int     param_class = PC_PARAM_MASK_CLASS;
    static const NAME_MASK param_class_table[] = {
	"builtin", PC_PARAM_FLAG_BUILTIN,
	"service", PC_PARAM_FLAG_SERVICE,
	"user", PC_PARAM_FLAG_USER,
	"all", PC_PARAM_MASK_CLASS,
	0,
    };
    ARGV   *override_params = 0;

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
    while ((ch = GETOPT(argc, argv, "aAbc:C:deEfFhlmMno:pPtvxX#")) > 0) {
	switch (ch) {
	case 'a':
	    cmd_mode |= SHOW_SASL_SERV;
	    break;
	case 'A':
	    cmd_mode |= SHOW_SASL_CLNT;
	    break;
	case 'b':
	    cmd_mode |= EXP_DSN_TEMPL;
	    if (ext_argv)
		msg_fatal("specify one of -b and -t");
	    ext_argv = argv_alloc(2);
	    argv_add(ext_argv, "bounce", "-SVnexpand_templates", (char *) 0);
	    break;
	case 'c':
	    if (setenv(CONF_ENV_PATH, optarg, 1) < 0)
		msg_fatal("out of memory");
	    break;
	case 'C':
	    param_class = name_mask_opt("-C option", param_class_table,
			      optarg, NAME_MASK_ANY_CASE | NAME_MASK_FATAL);
	    break;
	case 'd':
	    cmd_mode |= SHOW_DEFS;
	    break;
	case 'e':
	    cmd_mode |= EDIT_CONF;
	    break;
	case 'f':
	    cmd_mode |= FOLD_LINE;
	    break;
	case 'F':
	    cmd_mode |= MASTER_FIELD;
	    break;
	case '#':
	    cmd_mode |= COMMENT_OUT;
	    break;
	case 'h':
	    cmd_mode |= HIDE_NAME;
	    break;
	case 'l':
	    cmd_mode |= SHOW_LOCKS;
	    break;
	case 'm':
	    cmd_mode |= SHOW_MAPS;
	    break;
	case 'M':
	    cmd_mode |= MASTER_ENTRY;
	    break;
	case 'n':
	    cmd_mode |= SHOW_NONDEF;
	    break;
	case 'o':
	    cmd_mode |= MAIN_OVER;
	    if (override_params == 0)
		override_params = argv_alloc(2);
	    argv_add(override_params, optarg, (char *) 0);
	    break;
	case 'p':
	    cmd_mode |= MAIN_PARAM;
	    break;
	case 'P':
	    cmd_mode |= MASTER_PARAM;
	    break;
	case 't':
	    cmd_mode |= DUMP_DSN_TEMPL;
	    if (ext_argv)
		msg_fatal("specify one of -b and -t");
	    ext_argv = argv_alloc(2);
	    argv_add(ext_argv, "bounce", "-SVndump_templates", (char *) 0);
	    break;
	case 'x':
	    cmd_mode |= SHOW_EVAL;
	    break;
	case 'X':
	    /* This is irreversible, therefore require two-finger action. */
	    cmd_mode |= EDIT_EXCL;
	    break;
	case 'v':
	    msg_verbose++;
	    break;
	default:
	    usage(argv[0]);
	}
    }

    /*
     * Make all options explicit, before checking their compatibility.
     */
    if ((cmd_mode & incompat_options[0]) == 0)
	cmd_mode |= MAIN_PARAM;
    if ((cmd_mode & (MAIN_PARAM | MASTER_ENTRY | MASTER_FIELD | MASTER_PARAM))
	&& argv[optind] && strchr(argv[optind], '='))
	cmd_mode |= EDIT_CONF;

    /*
     * Sanity check.
     */
    check_exclusive_options(cmd_mode);
    check_compat_options(cmd_mode);

    if ((cmd_mode & EDIT_CONF) && argc == optind)
	msg_fatal("-e requires name=value argument");

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
    else if ((cmd_mode & (MASTER_ENTRY | MASTER_FIELD | MASTER_PARAM))
	     && !(cmd_mode & (EDIT_CONF | EDIT_EXCL | COMMENT_OUT))) {
	read_master(FAIL_ON_OPEN_ERROR);
	read_parameters();
	if (override_params)
	    set_parameters(override_params->argv);
	register_builtin_parameters(basename(argv[0]), getpid());
	register_service_parameters();
	register_user_parameters();
	if (cmd_mode & MASTER_FIELD)
	    show_master_fields(VSTREAM_OUT, cmd_mode, argc - optind, argv + optind);
	else if (cmd_mode & MASTER_PARAM)
	    show_master_params(VSTREAM_OUT, cmd_mode, argc - optind, argv + optind);
	else
	    show_master_entries(VSTREAM_OUT, cmd_mode, argc - optind, argv + optind);
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
     * Edit main.cf or master.cf.
     */
    else if (cmd_mode & (EDIT_CONF | COMMENT_OUT | EDIT_EXCL)) {
	if (optind == argc)
	    msg_fatal("missing service argument");
	if (cmd_mode & (MASTER_ENTRY | MASTER_FIELD | MASTER_PARAM)) {
	    edit_master(cmd_mode, argc - optind, argv + optind);
	} else {
	    edit_main(cmd_mode, argc - optind, argv + optind);
	}
    }

    /*
     * If showing non-default values, read main.cf.
     */
    else {
	if ((cmd_mode & SHOW_DEFS) == 0) {
	    read_parameters();
	    if (override_params)
		set_parameters(override_params->argv);
	}
	register_builtin_parameters(basename(argv[0]), getpid());

	/*
	 * Add service-dependent parameters (service names from master.cf)
	 * and user-defined parameters ($name macros in parameter values in
	 * main.cf and master.cf, but only if those names have a name=value
	 * in main.cf or master.cf).
	 */
	read_master(WARN_ON_OPEN_ERROR);
	register_service_parameters();
	if ((cmd_mode & SHOW_DEFS) == 0)
	    register_user_parameters();

	/*
	 * Show the requested values.
	 */
	show_parameters(VSTREAM_OUT, cmd_mode, param_class, argv + optind);

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
