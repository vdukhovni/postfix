/*++
/* NAME
/*	resolve_clnt 3
/* SUMMARY
/*	address resolve service client (internal forms)
/* SYNOPSIS
/*	#include <resolve_clnt.h>
/*
/*	typedef struct {
/* .in +4
/*		VSTRING *transport;
/*		VSTRING *nexthop
/*		VSTRING *recipient;
/* .in -4
/*	} RESOLVE_REPLY;
/*
/*	void	resolve_clnt_init(reply)
/*	RESOLVE_REPLY *reply;
/*
/*	void	resolve_clnt_query(address, reply)
/*	const char *address
/*	RESOLVE_REPLY *reply;
/*
/*	void	resolve_clnt_free(reply)
/*	RESOLVE_REPLY *reply;
/* DESCRIPTION
/*	This module implements a mail address resolver client.
/*
/*	resolve_clnt_init() initializes a reply data structure for use
/*	by resolve_clnt_query(). The structure is destroyed by passing
/*	it to resolve_clnt_free().
/*
/*	resolve_clnt_query() sends an internal-form recipient address
/*	(user@domain) to the resolver daemon and returns the resulting
/*	transport name, next_hop host name, and internal-form recipient
/*	address. In case of communication failure the program keeps trying
/*	until the mail system goes down.
/* DIAGNOSTICS
/*	Warnings: communication failure. Fatal error: mail system is down.
/* SEE ALSO
/*	mail_proto(3h) low-level mail component glue.
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
#include <unistd.h>
#include <string.h>
#include <errno.h>

/* Utility library. */

#include <msg.h>
#include <vstream.h>
#include <vstring.h>
#include <vstring_vstream.h>
#include <events.h>
#include <iostuff.h>

/* Global library. */

#include "mail_proto.h"
#include "mail_params.h"
#include "resolve_clnt.h"

/* Application-specific. */

static VSTREAM *resolve_fp = 0;
static void resolve_clnt_disconnect(void);

/* resolve_clnt_read - disconnect after EOF */

static void resolve_clnt_read(int unused_event, char *unused_context)
{
    resolve_clnt_disconnect();
}

/* resolve_clnt_time - disconnect after idle timeout */

static void resolve_clnt_time(char *unused_context)
{
    resolve_clnt_disconnect();
}

/* resolve_clnt_disconnect - disconnect from resolve service */

static void resolve_clnt_disconnect(void)
{

    /*
     * Be sure to disable read and timer events.
     */
    if (msg_verbose)
	msg_info("resolve service disconnect");
    event_disable_readwrite(vstream_fileno(resolve_fp));
    event_cancel_timer(resolve_clnt_time, (char *) 0);
    (void) vstream_fclose(resolve_fp);
    resolve_fp = 0;
}

/* resolve_clnt_connect - connect to resolve service */

static void resolve_clnt_connect(void)
{

    /*
     * Register a read event so that we can clean up when the remote side
     * disconnects, and a timer event so we can cleanup an idle connection.
     */
    resolve_fp = mail_connect_wait(MAIL_CLASS_PRIVATE, MAIL_SERVICE_REWRITE);
    close_on_exec(vstream_fileno(resolve_fp), CLOSE_ON_EXEC);
    event_enable_read(vstream_fileno(resolve_fp), resolve_clnt_read, (char *) 0);
    event_request_timer(resolve_clnt_time, (char *) 0, var_ipc_idle_limit);
}

/* resolve_clnt_init - initialize reply */

void    resolve_clnt_init(RESOLVE_REPLY *reply)
{
    reply->transport = vstring_alloc(100);
    reply->nexthop = vstring_alloc(100);
    reply->recipient = vstring_alloc(100);
}

/* resolve_clnt_query - resolve address to (transport, next hop, recipient) */

void    resolve_clnt_query(const char *addr, RESOLVE_REPLY *reply)
{
    char   *myname = "resolve_clnt_query";

    /*
     * Keep trying until we get a complete response. The resolve service is
     * CPU bound; making the client asynchronous would just complicate the
     * code.
     */
#define STR vstring_str

    for (;;) {
	if (resolve_fp == 0)
	    resolve_clnt_connect();
	else
	    event_request_timer(resolve_clnt_time, (char *) 0,
				var_ipc_idle_limit);
	if (mail_print(resolve_fp, "%s %s", RESOLVE_ADDR, addr)
	    || vstream_fflush(resolve_fp)) {
	    if (msg_verbose || errno != EPIPE)
		msg_warn("%s: bad write: %m", myname);
	} else if (mail_scan(resolve_fp, "%s %s %s", reply->transport,
			     reply->nexthop, reply->recipient) != 3) {
	    if (msg_verbose || errno != EPIPE)
		msg_warn("%s: bad read: %m", myname);
	} else {
	    if (msg_verbose)
		msg_info("%s: `%s' -> t=`%s' h=`%s' r=`%s'",
			 myname, addr, STR(reply->transport),
			 STR(reply->nexthop), STR(reply->recipient));
	    if (STR(reply->transport)[0] == 0)
		msg_warn("%s: null transport result for: <%s>", myname, addr);
	    else if (STR(reply->recipient)[0] == 0)
		msg_warn("%s: null recipient result for: <%s>", myname, addr);
	    else
		break;
	}
	sleep(10);				/* XXX make configurable */
	resolve_clnt_disconnect();
    }
}

/* resolve_clnt_free - destroy reply */

void    resolve_clnt_free(RESOLVE_REPLY *reply)
{
    reply->transport = vstring_free(reply->transport);
    reply->nexthop = vstring_free(reply->nexthop);
    reply->recipient = vstring_free(reply->recipient);
}

#ifdef TEST

#include <stdlib.h>
#include <msg_vstream.h>
#include <vstring_vstream.h>
#include <config.h>

static NORETURN usage(char *myname)
{
    msg_fatal("usage: %s [-v] [address...]", myname);
}

static void resolve(char *addr, RESOLVE_REPLY *reply)
{
    resolve_clnt_query(addr, reply);
    vstream_printf("%-10s %s\n", "address", addr);
    vstream_printf("%-10s %s\n", "transport", STR(reply->transport));
    vstream_printf("%-10s %s\n", "nexthop", *STR(reply->nexthop) ?
		   STR(reply->nexthop) : "[none]");
    vstream_printf("%-10s %s\n", "recipient", STR(reply->recipient));
    vstream_fflush(VSTREAM_OUT);
}

main(int argc, char **argv)
{
    RESOLVE_REPLY reply;
    int     ch;

    msg_vstream_init(argv[0], VSTREAM_ERR);

    read_config();
    msg_info("using config files in %s", var_config_dir);
    if (chdir(var_queue_dir) < 0)
	msg_fatal("chdir %s: %m", var_queue_dir);

    while ((ch = GETOPT(argc, argv, "v")) > 0) {
	switch (ch) {
	case 'v':
	    msg_verbose++;
	    break;
	default:
	    usage(argv[0]);
	}
    }
    resolve_clnt_init(&reply);

    if (argc > optind) {
	while (argv[optind]) {
	    resolve(argv[optind], &reply);
	    optind++;
	}
    } else {
	VSTRING *buffer = vstring_alloc(1);

	while (vstring_fgets_nonl(buffer, VSTREAM_IN)) {
	    resolve(STR(buffer), &reply);
	}
    }
}

#endif
