/*++
/* NAME
/*	proxymap 8
/* SUMMARY
/*	Postfix lookup table proxy server
/* SYNOPSIS
/*	\fBproxymap\fR [generic Postfix daemon options]
/* DESCRIPTION
/*	The \fBproxymap\fR server provides read-only table
/*	lookup service to Postfix client processes. The purpose
/*	of the service is:
/* .IP \(bu
/*	To overcome chroot restrictions. For example, a chrooted SMTP
/*	server needs access to the system passwd file in order to
/*	reject mail for non-existent local addresses.
/*	The solution is to specify:
/* .sp
/*	local_recipient_maps =
/* .ti +4
/*	proxy:unix:passwd.byname $alias_maps
/* .IP \(bu
/*	To consolidate the number of open lookup tables by sharing
/*	one open table among multiple processes. For example, to avoid
/*	problems due to "too many connections" to, e.g., mysql servers,
/*	specify:
/* .sp
/*	virtual_alias_maps =
/* .ti +4
/*	proxy:mysql:/etc/postfix/virtual.cf
/* PROXYMAP SERVICES
/* .ad
/* .fi
/*	The proxymap server implements the following requests:
/* .IP "\fBPROXY_REQ_OPEN\fI maptype:mapname flags\fR"
/*	Open the table with type \fImaptype\fR and name \fImapname\fR,
/*	as controlled by \fIflags\fR.
/*	The reply is the request completion status code and the
/*	map type dependent flags.
/* .IP "\fBPROXY_REQ_LOOKUP\fI maptype:mapname flags key\fR"
/*	Look up the data stored under the requested key.
/*	The reply is the request completion status code and
/*	the lookup result value.
/*	The \fImaptype:mapname\fR and \fIflags\fR are the same
/*	as with the \fBPROXY_REQ_OPEN\fR request.
/* .PP
/*	There is no close command. This does not seem to be useful
/*	because tables are meant to be shared among client processes.
/*
/*	The request completion status code is one of:
/* .IP \fBPROXY_STAT_OK\fR
/*	The requested table or lookup key was found.
/* .IP \fBPROXY_STAT_FAIL\fR
/*	The requested table or lookup key does not exist.
/* .IP \fBPROXY_STAT_BAD\fR
/*	The request was rejected (bad request parameter value).
/* .IP \fBPROXY_STAT_RETRY\fR
/*	The request was not completed.
/* MASTER INTERFACE
/* .ad
/* .fi
/*	The proxymap servers run under control by the Postfix master
/*	server.  Each server can handle multiple simultaneous connections.
/*	When all servers are busy while a client connects, the master
/*	creates a new proxymap server process, provided that the proxymap 
/*	server process limit is not exceeded.
/*	Each proxymap server terminates after serving \fB$max_use\fR clients
/*	or after \fB$max_idle\fR seconds of idle time.
/* SECURITY
/* .ad
/* .fi
/*	The proxymap server is not security-sensitive. It opens only
/*	tables that are approved via the \fBproxymap_filter\fR
/*	configuration parameter, does not talk to users, and
/*	can run at fixed low privilege, chrooted or not.
/* DIAGNOSTICS
/*	Problems and transactions are logged to \fBsyslogd\fR(8).
/* BUGS
/*	The proxymap server provides service to multiple clients,
/*	and must therefore not be used for tables that have high-latency
/*	lookups.
/* CONFIGURATION PARAMETERS
/* .ad
/* .fi
/*	The following main.cf parameters are especially relevant
/*	to this program. Use the \fBpostfix reload\fR command
/*	after a configuration change.
/* .IP \fBproxymap_filter\fR
/*	A list of zero or more parameter values that may contain
/*	Postfix lookup table references. Only table references that
/*	begin with \fBproxy:\fR are approved for access via the
/*	proxymap server.
/* SEE ALSO
/*	dict_proxy(3) proxy map client
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
#include <unistd.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <vstring.h>
#include <htable.h>
#include <stringops.h>
#include <dict.h>

/* Global library. */

#include <mail_conf.h>
#include <mail_params.h>
#include <mail_proto.h>
#include <dict_proxy.h>

/* Server skeleton. */

#include <mail_server.h>

/* Application-specific. */

 /*
  * XXX All but the last are needed here so that $name expansion dependencies
  * aren't too broken. The fix is to gather all parameter default settings in
  * one place.
  */
char   *var_local_rcpt_maps;
char   *var_virt_alias_maps;
char   *var_virt_alias_doms;
char   *var_virt_mbox_maps;
char   *var_virt_mbox_doms;
char   *var_relay_rcpt_maps;
char   *var_relay_domains;
char   *var_canonical_maps;
char   *var_send_canon_maps;
char   *var_rcpt_canon_maps;
char   *var_relocatedmaps;
char   *var_transport_maps;
char   *var_proxymap_filter;

 /*
  * The pre-approved, pre-parsed list of maps.
  */
static HTABLE *proxymap_filter;

 /*
  * Shared and static to reduce memory allocation overhead.
  */
static VSTRING *request;
static VSTRING *map_type_name_flags;
static VSTRING *map_type_name;
static VSTRING *key;

 /*
  * Silly little macros.
  */
#define STR(x)			vstring_str(x)
#define VSTREQ(x,y)		(strcmp(STR(x),y) == 0)

/* proxy_map_find - look up or open table */

static DICT *proxy_map_find(const char *map_type_name, int dict_flags)
{
    DICT   *dict;

#define PROXY_COLON	DICT_TYPE_PROXY ":"
#define PROXY_COLON_LEN	(sizeof(PROXY_COLON) - 1)
#define OPEN_FLAGS	O_RDONLY

    /*
     * Canonicalize the map name. If the map is not on the approved list,
     * deny the request.
     */
    while (strncmp(map_type_name, PROXY_COLON, PROXY_COLON_LEN) == 0)
	map_type_name += PROXY_COLON_LEN;
    if (htable_locate(proxymap_filter, map_type_name) == 0) {
	msg_warn("request for unapproved map: %s", map_type_name);
	return (0);
    }

    /*
     * Open one instance of a map for each combination of name+flags.
     */
    vstring_sprintf(map_type_name_flags, "%s:%o",
		    map_type_name, dict_flags);
    if ((dict = dict_handle(STR(map_type_name_flags))) == 0)
	dict = dict_open(map_type_name, OPEN_FLAGS, dict_flags);
    if (dict == 0)
	msg_panic("proxy_map_find: dict_open null result");
    dict_register(STR(map_type_name_flags), dict);
    return (dict);
}

/* proxymap_lookup_service - remote lookup service */

static void proxymap_lookup_service(VSTREAM *client_stream)
{
    int     status = PROXY_STAT_BAD;
    DICT   *dict;
    const char *value = "";
    int     dict_flags;

    if (attr_scan(client_stream, ATTR_FLAG_STRICT,
		  ATTR_TYPE_STR, MAIL_ATTR_TABLE, map_type_name,
		  ATTR_TYPE_NUM, MAIL_ATTR_FLAGS, &dict_flags,
		  ATTR_TYPE_STR, MAIL_ATTR_KEY, key,
		  ATTR_TYPE_END) == 3
	&& (dict = proxy_map_find(STR(map_type_name), dict_flags)) != 0) {

	if ((value = dict_get(dict, STR(key))) != 0) {
	    status = PROXY_STAT_OK;
	} else if (dict_errno == 0) {
	    status = PROXY_STAT_FAIL;
	    value = "";
	} else {
	    status = PROXY_STAT_RETRY;
	    value = "";
	}
    }

    /*
     * Respond to the client.
     */
    attr_print(client_stream, ATTR_FLAG_NONE,
	       ATTR_TYPE_NUM, MAIL_ATTR_STATUS, status,
	       ATTR_TYPE_STR, MAIL_ATTR_VALUE, value,
	       ATTR_TYPE_END);
}

/* proxymap_open_service - open remote lookup table */

static void proxymap_open_service(VSTREAM *client_stream)
{
    int     dict_flags;
    DICT   *dict;
    int     status = PROXY_STAT_BAD;
    int     flags = 0;

    if (attr_scan(client_stream, ATTR_FLAG_STRICT,
		  ATTR_TYPE_STR, MAIL_ATTR_TABLE, map_type_name,
		  ATTR_TYPE_NUM, MAIL_ATTR_FLAGS, &dict_flags,
		  ATTR_TYPE_END) == 2
	&& (dict = proxy_map_find(STR(map_type_name), dict_flags)) != 0) {

	status = PROXY_STAT_OK;
	flags = dict->flags;
    }

    /*
     * Respond to the client.
     */
    attr_print(client_stream, ATTR_FLAG_NONE,
	       ATTR_TYPE_NUM, MAIL_ATTR_STATUS, status,
	       ATTR_TYPE_NUM, MAIL_ATTR_FLAGS, flags,
	       ATTR_TYPE_END);
}

/* proxymap_service - perform service for client */

static void proxymap_service(VSTREAM *client_stream, char *unused_service,
			             char **argv)
{

    /*
     * Sanity check. This service takes no command-line arguments.
     */
    if (argv[0])
	msg_fatal("unexpected command-line argument: %s", argv[0]);

    /*
     * This routine runs whenever a client connects to the socket dedicated
     * to the address verification service. All connection-management stuff
     * is handled by the common code in multi_server.c.
     */
    if (attr_scan(client_stream,
		  ATTR_FLAG_MORE | ATTR_FLAG_STRICT,
		  ATTR_TYPE_STR, MAIL_ATTR_REQ, request,
		  ATTR_TYPE_END) == 1) {
	if (VSTREQ(request, PROXY_REQ_LOOKUP)) {
	    proxymap_lookup_service(client_stream);
	} else if (VSTREQ(request, PROXY_REQ_OPEN)) {
	    proxymap_open_service(client_stream);
	} else {
	    msg_warn("unrecognized request: \"%s\", ignored", STR(request));
	    attr_print(client_stream, ATTR_FLAG_NONE,
		       ATTR_TYPE_NUM, MAIL_ATTR_STATUS, PROXY_STAT_BAD,
		       ATTR_TYPE_END);
	}
    }
    vstream_fflush(client_stream);
}

/* post_jail_init - initialization after privilege drop */

static void post_jail_init(char *unused_name, char **unused_argv)
{
    const char *sep = " \t\r\n";
    char   *saved_filter;
    char   *bp;
    char   *type_name;

    request = vstring_alloc(10);
    map_type_name = vstring_alloc(10);
    map_type_name_flags = vstring_alloc(10);
    key = vstring_alloc(10);

    /*
     * Prepare the pre-approved list of proxied tables.
     */
    saved_filter = bp = mystrdup(var_proxymap_filter);
    proxymap_filter = htable_create(13);
    while ((type_name = mystrtok(&bp, sep)) != 0) {
	if (strncmp(type_name, PROXY_COLON, PROXY_COLON_LEN))
	    continue;
	do {
	    type_name += PROXY_COLON_LEN;
	} while (!strncmp(type_name, PROXY_COLON, PROXY_COLON_LEN));
	if (htable_find(proxymap_filter, type_name) == 0)
	    (void) htable_enter(proxymap_filter, type_name, (char *) 0);
    }
    myfree(saved_filter);
}

/* main - pass control to the multi-threaded skeleton */

int     main(int argc, char **argv)
{
    static CONFIG_STR_TABLE str_table[] = {
	VAR_LOCAL_RCPT_MAPS, DEF_LOCAL_RCPT_MAPS, &var_local_rcpt_maps, 0, 0,
	VAR_VIRT_ALIAS_MAPS, DEF_VIRT_ALIAS_MAPS, &var_virt_alias_maps, 0, 0,
	VAR_VIRT_ALIAS_DOMS, DEF_VIRT_ALIAS_DOMS, &var_virt_alias_doms, 0, 0,
	VAR_VIRT_MAILBOX_MAPS, DEF_VIRT_MAILBOX_MAPS, &var_virt_mbox_maps, 0, 0,
	VAR_VIRT_MAILBOX_DOMS, DEF_VIRT_MAILBOX_DOMS, &var_virt_mbox_doms, 0, 0,
	VAR_RELAY_RCPT_MAPS, DEF_RELAY_RCPT_MAPS, &var_relay_rcpt_maps, 0, 0,
	VAR_RELAY_DOMAINS, DEF_RELAY_DOMAINS, &var_relay_domains, 0, 0,
	VAR_CANONICAL_MAPS, DEF_CANONICAL_MAPS, &var_canonical_maps, 0, 0,
	VAR_SEND_CANON_MAPS, DEF_SEND_CANON_MAPS, &var_send_canon_maps, 0, 0,
	VAR_RCPT_CANON_MAPS, DEF_RCPT_CANON_MAPS, &var_rcpt_canon_maps, 0, 0,
	VAR_RELOCATED_MAPS, DEF_RELOCATED_MAPS, &var_relocatedmaps, 0, 0,
	VAR_TRANSPORT_MAPS, DEF_TRANSPORT_MAPS, &var_transport_maps, 0, 0,
	VAR_PROXYMAP_FILTER, DEF_PROXYMAP_FILTER, &var_proxymap_filter, 0, 0,
	0,
    };

    multi_server_main(argc, argv, proxymap_service,
		      MAIL_SERVER_STR_TABLE, str_table,
		      MAIL_SERVER_POST_INIT, post_jail_init,
		      0);
}
