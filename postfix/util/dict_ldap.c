/*++
/* NAME
/*	dict_ldap 3
/* SUMMARY
/*	dictionary manager interface to LDAP maps
/* SYNOPSIS
/*	#include <dict_ldap.h>
/*
/*	DICT    *dict_ldap_open(attribute, dummy, dict_flags)
/*	const char *attribute;
/*	int     dummy;
/*	int     dict_flags;
/* DESCRIPTION
/*	dict_ldap_open() makes LDAP user information accessible via
/*	the generic dictionary operations described in dict_open(3).
/*
/*	Arguments:
/* .IP ldapsource
/*	The prefix which will be used to obtain configuration parameters
/*	for this search. If it's 'ldapone', the configuration variables below 
/*	would look like 'ldapone_server_host', 'ldapone_search_base', and so
/*	on in main.cf.
/* .IP dummy
/*	Not used; this argument exists only for compatibility with
/*	the dict_open(3) interface.
/* .PP
/*	Configuration parameters:
/* .IP \fIldapsource_\fRserver_host
/*	The host at which all LDAP queries are directed.
/* .IP \fIldapsource_\fRserver_port
/*	The port the LDAP server listens on.
/* .IP \fIldapsource_\fRsearch_base
/*	The LDAP search base, for example: \fIO=organization name, C=country\fR.
/* .IP \fIldapsource_\fRtimeout
/*	Deadline for LDAP open() and LDAP search() .
/* .IP \fIldapsource_\fRquery_filter
/*	The filter used to search for directory entries, for example
/*	\fI(mailacceptinggeneralid=%s)\fR.
/* .IP \fIldapsource_\fRresult_attribute
/*	The attribute returned by the search, in which we expect to find
/*	RFC822 addresses, for example \fImaildrop\fR.
/* .IP \fIldapsource_\fRbind
/*	Whether or not to bind to the server -- LDAP v3 implementations don't
/*	require it, which saves some overhead.
/* .IP \fIldapsource_\fRbind_dn
/*	If you must bind to the server, do it with this distinguished name ...
/* .IP \fIldapsource_\fRbind_pw
/*	\&... and this password.
/* BUGS
/*	Of course not! :)
/* SEE ALSO
/*	dict(3) generic dictionary manager
/* DIAGNOSTICS
/*	Warnings: unable to connect to server, unable to bind to server.
/* AUTHOR(S)
/*	Prabhat K Singh
/*	VSNL, Bombay, India.
/*	prabhat@giasbm01.vsnl.net.in
/*
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10532, USA
/* 
/*	John Hensley
/*	Merit Network, Inc.
/*	hensley@merit.edu
/*
/*--*/

/* System library. */

#include "sys_defs.h"

#ifdef HAS_LDAP

#include <sys/time.h>
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>
#include <lber.h>
#include <ldap.h>

/* Utility library. */

#include "msg.h"
#include "mymalloc.h"
#include "vstring.h"
#include "dict.h"
#include "dict_ldap.h"

 /*
  * Grr.. this module should sit in the global library, because it interacts
  * with application-specific configuration parameters. I will have to
  * generalize the manner in which new dictionary types can register
  * themselves, including their configuration file parameters.
  */

/* 
 * structure containing all the configuration parameters for a given
 * LDAP source, plus its connection handle
 */
typedef struct {
    DICT    dict;                        /* generic member */
    char    *ldapsource;
    char    *server_host;
    int     server_port;
    char    *search_base;
    char    *query_filter;
    char    *result_attribute;
    int     bind;
    char    *bind_dn;
    char    *bind_pw;
    int     timeout;
    LDAP    *ld;
} DICT_LDAP;

 /*
  * LDAP connection timeout support.
  */
static jmp_buf env;

static void dict_ldap_timeout(int unused_sig)
{
    longjmp(env, 1);
}

/* dict_ldap_lookup - find database entry */

static const char *dict_ldap_lookup(DICT *dict, const char *name)
{
    char   *myname = "dict_ldap_lookup";
    DICT_LDAP *dict_ldap = (DICT_LDAP *) dict;
    static VSTRING *result;
    int LDAP_UNBIND = 0;
    LDAPMessage *res = 0; 
    LDAPMessage *entry = 0; 
    struct timeval tv;
    VSTRING *filter_buf = 0;
    char  **attr_values;
    long i = 0, j = 0;
    int rc = 0;
    void    (*saved_alarm) (int);

    dict_errno = 0;

    /*
     * Initialize.
     */
    if (result == 0)
        result = vstring_alloc(2);

    vstring_strcpy(result,"");

    if (msg_verbose)
        msg_info("%s: In dict_ldap_lookup", myname);

    if (dict_ldap->ld == 0) {
        msg_warn("%s: no existing connection for ldapsource %s, reopening", 
                 myname, dict_ldap->ldapsource);
        if (msg_verbose)
            msg_info("%s: connecting to server %s", myname, 
                     dict_ldap->server_host);

        if ((saved_alarm = signal(SIGALRM, dict_ldap_timeout)) == SIG_ERR)
            msg_fatal("%s: signal: %m", myname);

        alarm(dict_ldap->timeout);
        if (setjmp(env) == 0)
            dict_ldap->ld = ldap_open(dict_ldap->server_host, 
                           (int) dict_ldap->server_port);
        alarm(0);

        if (signal(SIGALRM, saved_alarm) == SIG_ERR)
            msg_fatal("%s: signal: %m", myname);

        if (msg_verbose)
            msg_info("%s: after ldap_open", myname);

        if (dict_ldap->ld == 0) {
            msg_fatal("%s: Unable to contact LDAP server %s",
                     myname, dict_ldap->server_host);
        } else {
            /*
             * If this server requires us to bind, do so.
             */
            if (dict_ldap->bind) {
                if (msg_verbose)
                    msg_info("%s: about to bind: server %s, base %s", myname, 
                         dict_ldap->server_host, dict_ldap->search_base);

                rc = ldap_bind_s(dict_ldap->ld, dict_ldap->search_base, NULL, 
                                 LDAP_AUTH_SIMPLE);
                if (rc != LDAP_SUCCESS) {
                    msg_fatal("%s: Unable to bind with search base %s at server %s (%d -- %s): ", myname, dict_ldap->search_base, dict_ldap->server_host, rc, ldap_err2string(rc));
                } else {
                    if (msg_verbose)
                        msg_info("%s: Successful bind to server %s with search base %s(%d -- %s): ", myname, dict_ldap->search_base, dict_ldap->server_host, rc, ldap_err2string(rc));
                }
            }
            if (msg_verbose)
                msg_info("%s: cached connection handle for LDAP source %s",
                         myname, dict_ldap->ldapsource);
        }
    }

    /*
     * Look for entries matching query_filter.
     */
    tv.tv_sec = dict_ldap->timeout;
    tv.tv_usec = 0;
    filter_buf = vstring_alloc(30);
    vstring_sprintf(filter_buf, dict_ldap->query_filter, name);

    if (msg_verbose)
        msg_info("%s: searching with filter %s", myname, 
                 vstring_str(filter_buf));

    if ((rc=ldap_search_st(dict_ldap->ld, dict_ldap->search_base, 
                           LDAP_SCOPE_SUBTREE,
                           vstring_str(filter_buf), 
                           0, 0, &tv, &res)) != LDAP_SUCCESS) {

        msg_info("%s: right after search", myname);

        msg_warn("%s: Unable to search base %s at server %s (%d -- %s): ", 
                 myname, dict_ldap->search_base, dict_ldap->server_host, rc, 
                 ldap_err2string(rc));
        LDAP_UNBIND = 1;

    } else {
        /*
         * Extract the requested result_attribute.
         */
        if (msg_verbose)
            msg_info("%s: search completed", myname);

        if ((entry = ldap_first_entry(dict_ldap->ld, res)) != 0) {
            attr_values = ldap_get_values(dict_ldap->ld, entry, 
                                          dict_ldap->result_attribute);
            /*
             * Append each returned address to the result list.
             */
            while (attr_values[i]) {
                if (VSTRING_LEN(result) > 0)
                    vstring_strcat(result, ",");
                vstring_strcat(result, attr_values[i]);
                i++;
            }
            ldap_value_free(attr_values);
            if (msg_verbose)
                msg_info("%s: search returned: %s", myname, vstring_str(result));
        } else {
            if (msg_verbose)
                msg_info("%s: search returned nothing", myname);
        }
    }

    /*
     * Cleanup. Always return with dict_errno set when we were unable to
     * perform the query.
     */
    if (res != 0)
        ldap_msgfree(res);
    else
        dict_errno = 1;
    if (LDAP_UNBIND == 1) {
        /* 
         * there was an LDAP problem; free the handle
         */
        ldap_unbind(dict_ldap->ld);
        dict_ldap->ld = 0;
        if (msg_verbose)
            msg_info("%s: freed connection handle for LDAP source %s", 
                     myname, dict_ldap->ldapsource);
    }
    if (filter_buf != 0)
        vstring_free(filter_buf);
    return (entry != 0 ? vstring_str(result) : 0);
}

/* dict_ldap_update - add or update database entry */

static void dict_ldap_update(DICT *dict, const char *unused_name,
                                     const char *unused_value)
{
    msg_fatal("dict_ldap_update: operation not implemented");
}

/* dict_ldap_close - disassociate from data base */

static void dict_ldap_close(DICT *dict)
{
    char   *myname = "dict_ldap_close";
    DICT_LDAP *dict_ldap = (DICT_LDAP *) dict;

    if (dict_ldap->ld)
        ldap_unbind(dict_ldap->ld);

    myfree(dict_ldap->ldapsource);
    myfree(dict_ldap->server_host);
    myfree(dict_ldap->search_base);
    myfree(dict_ldap->query_filter);
    myfree(dict_ldap->result_attribute);
    myfree(dict_ldap->bind_dn);
    myfree(dict_ldap->bind_pw);
    myfree((char *)dict_ldap);
}

/* dict_ldap_open - create association with data base */

DICT   *dict_ldap_open(const char *ldapsource, int dummy, int dict_flags)
{
    char   *myname = "dict_ldap_open";
    DICT_LDAP *dict_ldap;
    VSTRING *config_param;
    int rc = 0;
    void    (*saved_alarm) (int);

    dict_ldap = (DICT_LDAP *) mymalloc(sizeof(*dict_ldap));
    dict_ldap->dict.lookup = dict_ldap_lookup;
    dict_ldap->dict.update = dict_ldap_update;
    dict_ldap->dict.close = dict_ldap_close;
    dict_ldap->dict.fd = -1;
    dict_ldap->dict.flags = dict_flags | DICT_FLAG_FIXED;

    if (msg_verbose)
        msg_info("%s: using LDAP source %s", myname, ldapsource);

    dict_ldap->ldapsource = mystrdup(ldapsource);

    config_param = vstring_alloc(15);
    vstring_sprintf(config_param, "%s_server_host", ldapsource);

    dict_ldap->server_host = 
        mystrdup((char *)get_config_str(vstring_str(config_param), 
                 "localhost",0,0));
    if (msg_verbose)
        msg_info("%s: %s is %s", myname, vstring_str(config_param), 
                 dict_ldap->server_host);

    /* get configured value of "ldapsource_server_port"; default to 
    /* LDAP_PORT (389) */
    vstring_sprintf(config_param, "%s_server_port", ldapsource);
    dict_ldap->server_port = 
        get_config_int(vstring_str(config_param),LDAP_PORT,0,0);
    if (msg_verbose)
        msg_info("%s: %s is %d", myname, vstring_str(config_param), 
                 dict_ldap->server_port);

    vstring_sprintf(config_param, "%s_search_base", ldapsource);
    dict_ldap->search_base = 
        mystrdup((char *)get_config_str(vstring_str(config_param),"",0,0));
    if (msg_verbose)
        msg_info("%s: %s is %s", myname, vstring_str(config_param), 
                 dict_ldap->search_base);

    /* get configured value of "ldapsource_timeout"; default to 10 */
    vstring_sprintf(config_param, "%s_timeout", ldapsource);
    dict_ldap->timeout = get_config_int(config_param,10,0,0);
    if (msg_verbose)
        msg_info("%s: %s is %d", myname, vstring_str(config_param), 
                 dict_ldap->timeout);

    vstring_sprintf(config_param, "%s_query_filter", ldapsource);
    dict_ldap->query_filter = 
        mystrdup((char *)get_config_str(vstring_str(config_param),
                 "(mailacceptinggeneralid=%s)",0,0));
    if (msg_verbose)
        msg_info("%s: %s is %s", myname, vstring_str(config_param), 
                 dict_ldap->query_filter);

    vstring_sprintf(config_param, "%s_result_attribute", ldapsource);
    dict_ldap->result_attribute = 
        mystrdup((char *)get_config_str(vstring_str(config_param),
                                        "maildrop",0,0));
    if (msg_verbose)
        msg_info("%s: %s is %s", myname, vstring_str(config_param), 
                 dict_ldap->result_attribute);

    /* get configured value of "ldapsource_bind"; default to true */
    vstring_sprintf(config_param, "%s_bind", ldapsource);
    dict_ldap->bind = get_config_bool(vstring_str(config_param), 1);
    if (msg_verbose)
        msg_info("%s: %s is %d", myname, vstring_str(config_param), 
                 dict_ldap->bind);

    /* get configured value of "ldapsource_bind_dn"; default to "" */
    vstring_sprintf(config_param, "%s_bind_dn", ldapsource);
    dict_ldap->bind_dn = 
        mystrdup((char *)get_config_str(vstring_str(config_param),"",0,0));
    if (msg_verbose)
        msg_info("%s: %s is %s", myname, vstring_str(config_param), 
                 dict_ldap->bind_dn);

    /* get configured value of "ldapsource_bind_pw"; default to "" */
    vstring_sprintf(config_param, "%s_bind_pw", ldapsource);
    dict_ldap->bind_pw = 
        mystrdup((char *)get_config_str(vstring_str(config_param),"",0,0));
    if (msg_verbose)
        msg_info("%s: %s is %s", myname, vstring_str(config_param), 
                 dict_ldap->bind_pw);

    /* 
     * establish the connection to the LDAP server
     */

    if (msg_verbose)
        msg_info("%s: connecting to server %s", myname, 
                 dict_ldap->server_host);

    if ((saved_alarm = signal(SIGALRM, dict_ldap_timeout)) == SIG_ERR)
        msg_fatal("%s: signal: %m", myname);

    alarm(dict_ldap->timeout);
    if (setjmp(env) == 0)
        dict_ldap->ld = ldap_open(dict_ldap->server_host, 
                       (int) dict_ldap->server_port);
    alarm(0);

    if (signal(SIGALRM, saved_alarm) == SIG_ERR)
        msg_fatal("%s: signal: %m", myname);

    if (msg_verbose)
        msg_info("%s: after ldap_open", myname);

    if (dict_ldap->ld == 0) {
        msg_fatal("%s: Unable to contact LDAP server %s",
                 myname, dict_ldap->server_host);
    } else {
        /*
         * If this server requires us to bind, do so.
         */
        if (dict_ldap->bind) {
            if (msg_verbose)
                msg_info("%s: about to bind: server %s, base %s", myname, 
                     dict_ldap->server_host, dict_ldap->search_base);

            rc = ldap_bind_s(dict_ldap->ld, dict_ldap->search_base, NULL, 
                             LDAP_AUTH_SIMPLE);
            if (rc != LDAP_SUCCESS) {
                msg_fatal("%s: Unable to bind with search base %s at server %s (%d -- %s): ", myname, dict_ldap->search_base, dict_ldap->server_host, rc, ldap_err2string(rc));
            } else {
                if (msg_verbose)
                    msg_info("%s: Successful bind to server %s with search base %s(%d -- %s): ", myname, dict_ldap->search_base, dict_ldap->server_host, rc, ldap_err2string(rc));
            }
        }
        if (msg_verbose)
            msg_info("%s: cached connection handle for LDAP source %s",
                     myname, dict_ldap->ldapsource);
    }

    return (&dict_ldap->dict);
}

#endif
