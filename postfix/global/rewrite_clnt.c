/*++
/* NAME
/*	rewrite_clnt 3
/* SUMMARY
/*	address rewrite service client
/* SYNOPSIS
/*	#include <vstring.h>
/*	#include <rewrite_clnt.h>
/*
/*	VSTRING	*rewrite_clnt(ruleset, address, result)
/*	const char *ruleset;
/*	const char *address;
/*
/*	VSTRING	*rewrite_clnt_internal(ruleset, address, result)
/*	const char *ruleset;
/*	const char *address;
/*	VSTRING	*result;
/* DESCRIPTION
/*	This module implements a mail address rewriting client.
/*
/*	rewrite_clnt() sends a rule set name and external-form address to the
/*	rewriting service and returns the resulting external-form address.
/*	In case of communication failure the program keeps trying until the
/*	mail system shuts down.
/*
/*	rewrite_clnt_internal() performs the same functionality but takes
/*	input in internal (unquoted) form, and produces output in internal
/*	(unquoted) form.
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
#include <errno.h>

/* Utility library. */

#include <msg.h>
#include <vstring.h>
#include <vstream.h>
#include <vstring_vstream.h>
#include <events.h>
#include <iostuff.h>
#include <quote_822_local.h>

/* Global library. */

#include "mail_proto.h"
#include "mail_params.h"
#include "rewrite_clnt.h"

/* Application-specific. */

static VSTREAM *rewrite_fp = 0;
static void rewrite_clnt_disconnect(void);

/* rewrite_clnt_read - disconnect after EOF */

static void rewrite_clnt_read(int unused_event, char *unused_context)
{
    rewrite_clnt_disconnect();
}

/* rewrite_clnt_time - disconnect after timeout */

static void rewrite_clnt_time(char *unused_context)
{
    rewrite_clnt_disconnect();
}

/* rewrite_clnt_disconnect - disconnect from rewrite service */

static void rewrite_clnt_disconnect(void)
{

    /*
     * Be sure to disable read and timer events.
     */
    if (msg_verbose)
	msg_info("rewrite service disconnect");
    event_disable_readwrite(vstream_fileno(rewrite_fp));
    event_cancel_timer(rewrite_clnt_time, (char *) 0);
    (void) vstream_fclose(rewrite_fp);
    rewrite_fp = 0;
}

/* rewrite_clnt_connect - connect to rewrite service */

static void rewrite_clnt_connect(void)
{

    /*
     * Register a read event so that we can clean up when the remote side
     * disconnects, and a timer event so we can cleanup an idle connection.
     */
    rewrite_fp = mail_connect_wait(MAIL_CLASS_PRIVATE, MAIL_SERVICE_REWRITE);
    close_on_exec(vstream_fileno(rewrite_fp), CLOSE_ON_EXEC);
    event_enable_read(vstream_fileno(rewrite_fp), rewrite_clnt_read, (char *) 0);
    event_request_timer(rewrite_clnt_time, (char *) 0, var_ipc_idle_limit);
}

/* rewrite_clnt - rewrite address to (transport, next hop, recipient) */

VSTRING *rewrite_clnt(const char *rule, const char *addr, VSTRING *result)
{
    char   *myname = "rewrite_clnt";

    /*
     * Keep trying until we get a complete response. The rewrite service is
     * CPU bound and making the client asynchronous would just complicate the
     * code.
     */
#define STR vstring_str

    for (;;) {
	if (rewrite_fp == 0)
	    rewrite_clnt_connect();
	else
	    event_request_timer(rewrite_clnt_time, (char *) 0,
				var_ipc_idle_limit);
	if (mail_print(rewrite_fp, "%s %s %s", REWRITE_ADDR, rule, addr),
	    vstream_fflush(rewrite_fp)) {
	    if (msg_verbose || errno != EPIPE)
		msg_warn("%s: bad write: %m", myname);
	} else if (mail_scan(rewrite_fp, "%s", result) != 1) {
	    if (msg_verbose || errno != EPIPE)
		msg_warn("%s: bad read: %m", myname);
	} else {
	    if (msg_verbose)
		msg_info("rewrite_clnt: %s: %s -> %s",
			 rule, addr, vstring_str(result));
	    if (addr[0] != 0 && STR(result)[0] == 0)
		msg_warn("%s: null result for: <%s>", myname, addr);
	    else
		return (result);
	}
	sleep(10);				/* XXX make configurable */
	rewrite_clnt_disconnect();
    }
}

/* rewrite_clnt_internal - rewrite from/to internal form */

VSTRING *rewrite_clnt_internal(const char *ruleset, const char *addr, VSTRING *result)
{
    VSTRING *temp = vstring_alloc(100);

    /*
     * Convert the address from internal address form to external RFC822
     * form, then rewrite it. After rewriting, convert to internal form.
     */
    quote_822_local(temp, addr);
    rewrite_clnt(ruleset, STR(temp), temp);
    unquote_822_local(result, STR(temp));
    vstring_free(temp);
    return (result);
}

#ifdef TEST

#include <stdlib.h>
#include <string.h>
#include <msg_vstream.h>
#include <vstring_vstream.h>
#include <config.h>
#include <mail_params.h>

static NORETURN usage(char *myname)
{
    msg_fatal("usage: %s [-v] [rule address...]", myname);
}

static void rewrite(char *rule, char *addr, VSTRING *reply)
{
    rewrite_clnt(rule, addr, reply);
    vstream_printf("%-10s %s\n", "rule", rule);
    vstream_printf("%-10s %s\n", "address", addr);
    vstream_printf("%-10s %s\n", "result", STR(reply));
    vstream_fflush(VSTREAM_OUT);
}

main(int argc, char **argv)
{
    VSTRING *reply;
    int     ch;
    char   *rule;
    char   *addr;

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
    reply = vstring_alloc(1);

    if (argc > optind) {
	for (;;) {
	    if ((rule = argv[optind++]) == 0)
		break;
	    if ((addr = argv[optind++]) == 0)
		usage(argv[0]);
	    rewrite(rule, addr, reply);
	}
    } else {
	VSTRING *buffer = vstring_alloc(1);

	while (vstring_fgets_nonl(buffer, VSTREAM_IN)) {
	    if ((rule = strtok(STR(buffer), " \t,")) == 0
		|| (addr = strtok((char *) 0, " \t,")) == 0)
		usage(argv[0]);
	    rewrite(rule, addr, reply);
	}
	vstring_free(buffer);
    }
    vstring_free(reply);
}

#endif
