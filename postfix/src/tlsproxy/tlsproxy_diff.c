/*++
/* NAME
/*	tlsproxy_diff 3
/* SUMMARY
/*	Diff TLS proxy client attribute lists
/* SYNOPSIS
/*	#include <tlsproxy_diff.h>
/*
/*	void     tlsp_log_config_diff(
/*	const char *server_cfg,
/*	const char *client_cfg)
/* DESCRIPTION
/*	tlsp_log_config_diff() logs the difference between TLS
/*	configuration attribute lists. The format is zer or more "name
/*	= value\n". The server_cfg argument specifies attributes from
/*	the tlsproxy server, and client_cfg lists attributes from a
/*	tlsproxy client.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* HISTORY
/* .ad
/* .fi
/*	This service was introduced with Postfix version 2.8.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/* 
/*	Wietse Venema
/*	Google, Inc.
/*	111 8th Avenue
/*	New York, NY 10011, USA
/*	
/*	Wietse Venema
/*	porcupine.org
/*--*/

 /*
  * System library.
  */
#ifdef USE_TLS

#include <sys_defs.h>
#include <string.h>

 /*
  * Utility library.
  */
#include <msg.h>
#include <mymalloc.h>
#include <split_at.h>
#include <vstring.h>

 /*
  * Internal API.
  */
#include <tlsproxy.h>
#include <tlsproxy_diff.h>

/* tlsp_config_diff - report server-client config differences */

void    tlsp_log_config_diff(const char *server_cfg, const char *client_cfg)
{
    VSTRING *diff_summary = vstring_alloc(100);
    char   *saved_server = mystrdup(server_cfg);
    char   *saved_client = mystrdup(client_cfg);
    char   *server_field;
    char   *client_field;
    char   *server_next;
    char   *client_next;

    /*
     * Not using argv_split(), because it would treat multiple consecutive
     * newline characters as one.
     */
    for (server_field = saved_server, client_field = saved_client;
	 server_field && client_field;
	 server_field = server_next, client_field = client_next) {
	server_next = split_at(server_field, '\n');
	client_next = split_at(client_field, '\n');
	if (strcmp(server_field, client_field) != 0) {
	    if (LEN(diff_summary) > 0)
		vstring_sprintf_append(diff_summary, "; ");
	    vstring_sprintf_append(diff_summary,
				   "(server) '%s' != (client) '%s'",
				   server_field, client_field);
	}
    }
    msg_warn("%s", STR(diff_summary));

    vstring_free(diff_summary);
    myfree(saved_client);
    myfree(saved_server);
}

#endif
