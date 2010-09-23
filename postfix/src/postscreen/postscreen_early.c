/*++
/* NAME
/*	postscreen_early 3
/* SUMMARY
/*	postscreen pre-handshake tests
/* SYNOPSIS
/*	#include <postscreen.h>
/*
/*	void	ps_early_init(void)
/*
/*	void	ps_early_tests(state)
/*	PS_STATE *state;
/* DESCRIPTION
/*	ps_early_tests() performs protocol tests before the SMTP
/*	handshake: the pregreet test and the DNSBL test. Control
/*	is passed to the ps_smtpd_tests() routine as appropriate.
/*
/*	ps_early_init() performs one-time initialization.
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
#include <sys/socket.h>

/* Utility library. */

#include <msg.h>
#include <stringops.h>
#include <mymalloc.h>
#include <vstring.h>

/* Global library. */

#include <mail_params.h>

/* Application-specific. */

#include <postscreen.h>

static char *ps_teaser_greeting;

/* ps_early_event - handle pre-greet, EOF, and DNSBL results. */

static void ps_early_event(int event, char *context)
{
    const char *myname = "ps_early_event";
    PS_STATE *state = (PS_STATE *) context;
    char    read_buf[PS_READ_BUF_SIZE];
    int     read_count;
    int     dnsbl_score;
    DELTA_TIME elapsed;
    const char *dnsbl_name;

    if (msg_verbose > 1)
	msg_info("%s: sq=%d cq=%d event %d on smtp socket %d from %s:%s flags=%s",
		 myname, ps_post_queue_length, ps_check_queue_length,
		 event, vstream_fileno(state->smtp_client_stream),
		 state->smtp_client_addr, state->smtp_client_port,
		 ps_print_state_flags(state->flags, myname));

    PS_CLEAR_EVENT_REQUEST(vstream_fileno(state->smtp_client_stream),
			   ps_early_event, context);

    /*
     * XXX Be sure to empty the DNSBL lookup buffer otherwise we have a
     * memory leak.
     * 
     * XXX We can avoid "forgetting" to do this by keeping a pointer to the
     * DNSBL lookup buffer in the PS_STATE structure. This also allows us to
     * shave off a hash table lookup when retrieving the DNSBL result.
     */
    switch (event) {

	/*
	 * We reached the end of the early tests time limit.
	 */
    case EVENT_TIME:

	/*
	 * Check if the SMTP client spoke before its turn.
	 */
	if ((state->flags & PS_STATE_FLAG_PREGR_TODO_FAIL)
	    == PS_STATE_FLAG_PREGR_TODO) {
	    state->pregr_stamp = event_time() + var_ps_pregr_ttl;
	    PS_PASS_SESSION_STATE(state, "pregreet test",
				  PS_STATE_FLAG_PREGR_PASS);
	}
	if ((state->flags & PS_STATE_FLAG_PREGR_FAIL)
	    && ps_pregr_action == PS_ACT_IGNORE) {
	    PS_UNFAIL_SESSION_STATE(state, PS_STATE_FLAG_PREGR_FAIL);
	    /* Not: PS_PASS_SESSION_STATE. Repeat this test the next time. */
	}

	/*
	 * If the client is DNS blocklisted, drop the connection, send the
	 * client to a dummy protocol engine, or continue to the next test.
	 */
#define PS_DNSBL_FORMAT \
	"%s 5.7.1 Service unavailable; client [%s] blocked using %s\r\n"

	if (state->flags & PS_STATE_FLAG_DNSBL_TODO) {
	    dnsbl_score =
		ps_dnsbl_retrieve(state->smtp_client_addr, &dnsbl_name);
	    if (dnsbl_score < var_ps_dnsbl_thresh) {
		state->dnsbl_stamp = event_time() + var_ps_dnsbl_ttl;
		PS_PASS_SESSION_STATE(state, "dnsbl test",
				      PS_STATE_FLAG_DNSBL_PASS);
	    } else {
		msg_info("DNSBL rank %d for %s",
			 dnsbl_score, state->smtp_client_addr);
		PS_FAIL_SESSION_STATE(state, PS_STATE_FLAG_DNSBL_FAIL);
		switch (ps_dnsbl_action) {
		case PS_ACT_DROP:
		    state->dnsbl_reply = vstring_sprintf(vstring_alloc(100),
						     PS_DNSBL_FORMAT, "521",
				       state->smtp_client_addr, dnsbl_name);
		    PS_DROP_SESSION_STATE(state, STR(state->dnsbl_reply));
		    return;
		case PS_ACT_ENFORCE:
		    state->dnsbl_reply = vstring_sprintf(vstring_alloc(100),
						     PS_DNSBL_FORMAT, "550",
				       state->smtp_client_addr, dnsbl_name);
		    PS_ENFORCE_SESSION_STATE(state, STR(state->dnsbl_reply));
		    break;
		case PS_ACT_IGNORE:
		    PS_UNFAIL_SESSION_STATE(state, PS_STATE_FLAG_DNSBL_FAIL);
		    /* Not: PS_PASS_SESSION_STATE. Repeat this test. */
		    break;
		default:
		    msg_panic("%s: unknown dnsbl action value %d",
			      myname, ps_dnsbl_action);

		}
	    }
	}

	/*
	 * Pass the connection to a real SMTP server, or enter the dummy
	 * engine for deep tests.
	 */
	if (state->flags & (PS_STATE_FLAG_NOFORWARD | PS_STATE_FLAG_SMTPD_TODO))
	    ps_smtpd_tests(state);
	else
	    ps_conclude(state);
	return;

	/*
	 * EOF, or the client spoke before its turn. We simply drop the
	 * connection, or we continue waiting and allow DNS replies to
	 * trickle in.
	 * 
	 * XXX Reset the pregreet timer when the DNS results are complete.
	 */
    default:
	if ((read_count = recv(vstream_fileno(state->smtp_client_stream),
			  read_buf, sizeof(read_buf) - 1, MSG_PEEK)) <= 0) {
	    /* Avoid memory leak. */
	    if (state->flags & PS_STATE_FLAG_DNSBL_TODO)
		(void) ps_dnsbl_retrieve(state->smtp_client_addr, &dnsbl_name);
	    /* XXX Wait for DNS replies to come in. */
	    ps_hangup_event(state);
	    return;
	}
	read_buf[read_count] = 0;
	msg_info("PREGREET %d after %s from %s: %.100s", read_count,
		 ps_format_delta_time(ps_temp, state->start_time, &elapsed),
		 state->smtp_client_addr, printable(read_buf, '?'));
	PS_FAIL_SESSION_STATE(state, PS_STATE_FLAG_PREGR_FAIL);
	switch (ps_pregr_action) {
	case PS_ACT_DROP:
	    /* Avoid memory leak. */
	    if (state->flags & PS_STATE_FLAG_DNSBL_TODO)
		(void) ps_dnsbl_retrieve(state->smtp_client_addr, &dnsbl_name);
	    PS_DROP_SESSION_STATE(state, "521 5.5.1 Protocol error\r\n");
	    return;
	case PS_ACT_ENFORCE:
	    /* We call ps_dnsbl_retrieve() when the timer expires. */
	    PS_ENFORCE_SESSION_STATE(state, "550 5.5.1 Protocol error\r\n");
	    break;
	case PS_ACT_IGNORE:
	    /* We call ps_dnsbl_retrieve() when the timer expires. */
	    /* We must handle this case after the timer expires. */
	    break;
	default:
	    msg_panic("%s: unknown pregreet action value %d",
		      myname, ps_pregr_action);
	}

	/*
	 * Terminate the greet delay if we're just waiting for the pregreet
	 * test to complete. It is safe to call ps_early_event directly,
	 * since we are already in that function.
	 * 
	 * XXX After this code passes all tests, swap around the two blocks in
	 * this switch statement and fall through from EVENT_READ into
	 * EVENT_TIME, instead of calling ps_early_event recursively.
	 */
	state->flags |= PS_STATE_FLAG_PREGR_DONE;
	if (elapsed.dt_sec >= PS_EFF_GREET_WAIT
	    || ((state->flags & PS_STATE_FLAG_EARLY_DONE)
		== PS_STATE_FLAGS_TODO_TO_DONE(state->flags & PS_STATE_FLAG_EARLY_TODO)))
	    ps_early_event(EVENT_TIME, context);
	else
	    event_request_timer(ps_early_event, context,
				PS_EFF_GREET_WAIT - elapsed.dt_sec);
	return;
    }
}

/* ps_early_dnsbl_event - cancel pregreet timer if waiting for DNS only */

static void ps_early_dnsbl_event(int unused_event, char *context)
{
    const char *myname = "ps_early_dnsbl_event";
    PS_STATE *state = (PS_STATE *) context;

    if (msg_verbose)
	msg_info("%s: notify %s:%s", myname, PS_CLIENT_ADDR_PORT(state));

    /*
     * Terminate the greet delay if we're just waiting for DNSBL lookup to
     * complete. Don't call ps_early_event directly, that would result in a
     * dangling pointer.
     */
    state->flags |= PS_STATE_FLAG_DNSBL_DONE;
    if ((state->flags & PS_STATE_FLAG_EARLY_DONE)
    == PS_STATE_FLAGS_TODO_TO_DONE(state->flags & PS_STATE_FLAG_EARLY_TODO))
	event_request_timer(ps_early_event, context, EVENT_NULL_DELAY);
}

/* ps_early_tests - start the early (before protocol) tests */

void    ps_early_tests(PS_STATE *state)
{
    const char *myname = "ps_early_tests";

    /*
     * Report errors and progress in the context of this test.
     */
    PS_BEGIN_TESTS(state, "tests before SMTP handshake");

    /*
     * Run a PREGREET test. Send half the greeting banner, by way of teaser,
     * then wait briefly to see if the client speaks before its turn.
     */
    if ((state->flags & PS_STATE_FLAG_PREGR_TODO) != 0
	&& ps_teaser_greeting != 0
	&& ps_send_reply(vstream_fileno(state->smtp_client_stream),
			 state->smtp_client_addr, state->smtp_client_port,
			 ps_teaser_greeting) != 0) {
	ps_hangup_event(state);
	return;
    }

    /*
     * Run a DNS blocklist query.
     */
    if ((state->flags & PS_STATE_FLAG_DNSBL_TODO) != 0)
	ps_dnsbl_request(state->smtp_client_addr, ps_early_dnsbl_event,
			    (char *) state);

    /*
     * Wait for the client to respond or for DNS lookup to complete.
     */
    if ((state->flags & PS_STATE_FLAG_PREGR_TODO) != 0)
	PS_READ_EVENT_REQUEST(vstream_fileno(state->smtp_client_stream),
			 ps_early_event, (char *) state, PS_EFF_GREET_WAIT);
    else
	event_request_timer(ps_early_event, (char *) state, PS_EFF_GREET_WAIT);
}

/* ps_early_init - initialize early tests */

void    ps_early_init(void)
{
    if (*var_ps_pregr_banner) {
	vstring_sprintf(ps_temp, "220-%s\r\n", var_ps_pregr_banner);
	ps_teaser_greeting = mystrdup(STR(ps_temp));
    }
}
