/*++
/* NAME
/*	postscreen_haproxy 3
/* SUMMARY
/*	haproxy protocol adapter
/* SYNOPSIS
/*	#include <postscreen_haproxy.h>
/*
/*	void	psc_endpt_haproxy_lookup(smtp_client_stream,
/*			void *lookup_done(status, smtp_client_stream,
/*				smtp_client_addr, smtp_client_port,
/*				smtp_server_addr, smtp_server_port))
/*	VSTRING	*smtp_client_stream;
/*	int	status;
/*	MAI_HOSTADDR_STR *smtp_client_addr;
/*	MAI_SERVPORT_STR *smtp_client_port;
/*	MAI_HOSTADDR_STR *smtp_server_addr;
/*	MAI_SERVPORT_STR *smtp_server_port;
/* DESCRIPTION
/*	psc_endpt_haproxy_lookup() looks up connection endpoint
/*	information via the haproxy protocol.  Arguments and results
/*	conform to the postscreen_endpt(3) API.
/*
/*	The following summarizes what the Postfix SMTP server expects
/*	from an up-stream proxy adapter.
/* .IP \(bu
/*	Validate address and port syntax. Permit only protocols
/*	that are configured with the main.cf:inet_protocols
/*	setting.
/* .IP \(bu
/*	Convert IPv4-in-IPv6 address syntax to IPv4 form, when both
/*	IPv4 and IPv6 support are enabled with main.cf:inet_protocols.
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
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <events.h>
#include <myaddrinfo.h>
#include <vstream.h>
#include <vstring.h>
#include <stringops.h>

/* Global library. */

#include <haproxy_srvr.h>
#include <mail_params.h>

/* Application-specific. */

#include <postscreen.h>
#include <postscreen_haproxy.h>

 /*
  * Per-session state.
  */
typedef struct {
    VSTREAM *stream;
    PSC_ENDPT_LOOKUP_FN notify;
    VSTRING *buffer;
} PSC_HAPROXY_STATE;

/* psc_endpt_haproxy_event - read or time event */

static void psc_endpt_haproxy_event(int event, char *context)
{
    const char *myname = "psc_endpt_haproxy_event";
    PSC_HAPROXY_STATE *state = (PSC_HAPROXY_STATE *) context;
    int     status = 0;
    MAI_HOSTADDR_STR smtp_client_addr;
    MAI_SERVPORT_STR smtp_client_port;
    MAI_HOSTADDR_STR smtp_server_addr;
    MAI_SERVPORT_STR smtp_server_port;
    int     last_char = 0;
    const char *err;
    VSTRING *escape_buf;

    /*
     * Basic event processing.
     */
    switch (event) {
    case EVENT_TIME:
	msg_warn("haproxy read: time limit exceeded");
	status = -1;
	break;
    case EVENT_READ:
	if ((last_char = VSTREAM_GETC(state->stream)) == VSTREAM_EOF) {
	    if (vstream_ferror(state->stream))
		msg_warn("haproxy read: %m");
	    else
		msg_warn("haproxy read: lost connection");
	    status = -1;
	    break;
	}
	if (VSTRING_LEN(state->buffer) >= HAPROXY_MAX_LEN) {
	    msg_warn("haproxy read: line too long");
	    status = -1;
	    break;
	}
	VSTRING_ADDCH(state->buffer, last_char);
	break;
    }

    /*
     * Parse the haproxy line. Note: the haproxy_srvr_parse() routine
     * performs address protocol checks, address and port syntax checks, and
     * converts IPv4-in-IPv6 address string syntax (:ffff::1.2.3.4) to IPv4
     * syntax where permitted by the main.cf:inet_protocols setting.
     */
    if (status == 0 && last_char == '\n') {
	VSTRING_TERMINATE(state->buffer);
	if ((err = haproxy_srvr_parse(vstring_str(state->buffer),
				      &smtp_client_addr, &smtp_client_port,
			      &smtp_server_addr, &smtp_server_port)) != 0) {
	    escape_buf = vstring_alloc(HAPROXY_MAX_LEN + 2);
	    escape(escape_buf, vstring_str(state->buffer),
		   VSTRING_LEN(state->buffer));
	    msg_warn("haproxy read: %s: %s", err, vstring_str(escape_buf));
	    status = -1;
	    vstring_free(escape_buf);
	}
    }

    /*
     * Are we done yet?
     */
    if (status < 0 || last_char == '\n') {
	PSC_CLEAR_EVENT_REQUEST(vstream_fileno(state->stream),
				psc_endpt_haproxy_event, context);
	vstream_control(state->stream,
			VSTREAM_CTL_BUFSIZE, VSTREAM_BUFSIZE,
			VSTREAM_CTL_END);
	state->notify(status, state->stream,
		      &smtp_client_addr, &smtp_client_port,
		      &smtp_server_addr, &smtp_server_port);
	/* Note: the stream may be closed at this point. */
	vstring_free(state->buffer);
	myfree((char *) state);
    }
}

/* psc_endpt_haproxy_lookup - event-driven haproxy client */

void    psc_endpt_haproxy_lookup(VSTREAM *stream,
				         PSC_ENDPT_LOOKUP_FN notify)
{
    const char *myname = "psc_endpt_haproxy_lookup";
    PSC_HAPROXY_STATE *state;

    /*
     * Prepare the per-session state. XXX To improve overload behavior,
     * maintain a pool of these so that we can reduce memory allocator
     * activity.
     */
    state = (PSC_HAPROXY_STATE *) mymalloc(sizeof(*state));
    state->stream = stream;
    state->notify = notify;
    state->buffer = vstring_alloc(100);

    /*
     * We don't assume that the haproxy line will be unfragmented. Therefore,
     * we use read(2) instead of recv(..., MSG_PEEK).
     * 
     * We must not read(2) past the <CR><LF> that terminates the haproxy line.
     * Therefore we force one-character read(2) calls.
     * 
     * We want to (eventually) build this on top of a reusable line read
     * routine, once we have figured out an easy-to-use and efficient API.
     */
    vstream_control(stream, VSTREAM_CTL_BUFSIZE, 1, VSTREAM_CTL_END);

    /*
     * Read the haproxy line.
     */
    PSC_READ_EVENT_REQUEST(vstream_fileno(stream), psc_endpt_haproxy_event,
			   (char *) state, var_psc_uproxy_tmout);
}
