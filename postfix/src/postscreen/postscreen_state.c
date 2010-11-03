/*++
/* NAME
/*	postscreen_state 3
/* SUMMARY
/*	postscreen session state and queue length management
/* SYNOPSIS
/*	#include <postscreen.h>
/*
/*	PS_STATE *ps_new_session_state(stream, addr, port)
/*	VSTREAM *stream;
/*	const char *addr;
/*	const char *port;
/*
/*	void	ps_free_session_state(state)
/*	PS_STATE *state;
/*
/*	char	*ps_print_state_flags(flags, context)
/*	int	flags;
/*	const char *context;
/*
/*	void	PS_ADD_SERVER_STATE(state, server_fd)
/*	PS_STATE *state;
/*	int	server_fd;
/*
/*	void	PS_DEL_CLIENT_STATE(state)
/*	PS_STATE *state;
/*
/*	void	PS_DROP_SESSION_STATE(state, final_reply)
/*	PS_STATE *state;
/*	const char *final_reply;
/*
/*	void	PS_ENFORCE_SESSION_STATE(state, rcpt_reply)
/*	PS_STATE *state;
/*	const char *rcpt_reply;
/*
/*	void	PS_PASS_SESSION_STATE(state, testname, pass_flag)
/*	PS_STATE *state;
/*	const char *testname;
/*	int	pass_flag;
/*
/*	void	PS_FAIL_SESSION_STATE(state, fail_flag)
/*	PS_STATE *state;
/*	int	fail_flag;
/*
/*	void	PS_UNFAIL_SESSION_STATE(state, fail_flag)
/*	PS_STATE *state;
/*	int	fail_flag;
/* DESCRIPTION
/*	This module maintains per-client session state, and two
/*	global file descriptor counters:
/* .IP ps_check_queue_length
/*	The total number of remote SMTP client sockets.
/* .IP ps_post_queue_length
/*	The total number of server file descriptors that are currently
/*	in use for client file descriptor passing. This number
/*	equals the number of client file descriptors in transit.
/* .PP
/*	ps_new_session_state() creates a new session state object
/*	for the specified client stream, and increments the
/*	ps_check_queue_length counter.  The flags and per-test time
/*	stamps are initialized with PS_INIT_TESTS().  The addr and
/*	port arguments are null-terminated strings with the remote
/*	SMTP client endpoint. The _reply members are set to
/*	polite "try again" SMTP replies. The protocol member is set
/*	to "SMTP".
/*
/*	The ps_stress variable is set to non-zero when
/*	ps_check_queue_length passes over a high-water mark.
/*
/*	ps_free_session_state() destroys the specified session state
/*	object, closes the applicable I/O channels, and decrements
/*	the applicable file descriptor counters: ps_check_queue_length
/*	and ps_post_queue_length.
/*
/*	The ps_stress variable is reset to zero when ps_check_queue_length
/*	passes under a low-water mark.
/*
/*	ps_print_state_flags() converts per-session flags into
/*	human-readable form. The context is for error reporting.
/*	The result is overwritten upon each call.
/*
/*	PS_ADD_SERVER_STATE() updates the specified session state
/*	object with the specified server file descriptor, and
/*	increments the global ps_post_queue_length file descriptor
/*	counter.
/*
/*	PS_DEL_CLIENT_STATE() updates the specified session state
/*	object, closes the client stream, and decrements the global
/*	ps_check_queue_length file descriptor counter.
/*
/*	PS_DROP_SESSION_STATE() updates the specified session state
/*	object and closes the client stream after sending the
/*	specified SMTP reply.
/*
/*	PS_ENFORCE_SESSION_STATE() updates the specified session
/*	state object. It arranges that the built-in SMTP engine
/*	logs sender/recipient information and rejects all RCPT TO
/*	commands with the specified SMTP reply.
/*
/*	PS_PASS_SESSION_STATE() sets the specified "pass" flag.
/*	The testname is used for debug logging.
/*
/*	PS_FAIL_SESSION_STATE() sets the specified "fail" flag.
/*
/*	PS_UNFAIL_SESSION_STATE() unsets the specified "fail" flag.
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

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <name_mask.h>

/* Global library. */

#include <mail_proto.h>

/* Master server protocols. */

#include <mail_server.h>

/* Application-specific. */

#include <postscreen.h>

/* ps_new_session_state - fill in connection state for event processing */

PS_STATE *ps_new_session_state(VSTREAM *stream,
			               const char *addr,
			               const char *port)
{
    PS_STATE *state;

    state = (PS_STATE *) mymalloc(sizeof(*state));
    PS_INIT_TESTS(state);
    if ((state->smtp_client_stream = stream) != 0)
	ps_check_queue_length++;
    state->smtp_server_fd = (-1);
    state->smtp_client_addr = mystrdup(addr);
    state->smtp_client_port = mystrdup(port);
    state->test_name = "TEST NAME HERE";
    state->dnsbl_reply = 0;
    state->final_reply = "421 4.3.2 Service currently unavailable\r\n";
    state->rcpt_reply = "450 4.3.2 Service currently unavailable\r\n";
    state->command_count = 0;
    state->protocol = MAIL_PROTO_SMTP;
    state->helo_name = 0;
    state->sender = 0;
    state->cmd_buffer = 0;
    state->read_state = 0;

    /*
     * Update the stress level.
     */
    if (ps_stress == 0
	&& ps_check_queue_length >= ps_check_queue_length_hiwat) {
	ps_stress = 1;
	msg_info("entering STRESS mode with %d connections",
		 ps_check_queue_length);
    }
    return (state);
}

/* ps_free_session_state - destroy connection state including connections */

void    ps_free_session_state(PS_STATE *state)
{
    if (state->smtp_client_stream != 0) {
	event_server_disconnect(state->smtp_client_stream);
	ps_check_queue_length--;
    }
    if (state->smtp_server_fd >= 0) {
	close(state->smtp_server_fd);
	ps_post_queue_length--;
    }
    myfree(state->smtp_client_addr);
    myfree(state->smtp_client_port);
    if (state->dnsbl_reply)
	vstring_free(state->dnsbl_reply);
    if (state->helo_name)
	myfree(state->helo_name);
    if (state->sender)
	myfree(state->sender);
    if (state->cmd_buffer)
	vstring_free(state->cmd_buffer);
    myfree((char *) state);

    if (ps_check_queue_length < 0 || ps_post_queue_length < 0)
	msg_panic("bad queue length: check_queue=%d, post_queue=%d",
		  ps_check_queue_length, ps_post_queue_length);

    /*
     * Update the stress level.
     */
    if (ps_stress != 0
	&& ps_check_queue_length <= ps_check_queue_length_lowat) {
	ps_stress = 0;
	msg_info("leaving STRESS mode with %d connections",
		 ps_check_queue_length);
    }
}

/* ps_print_state_flags - format state flags */

const char *ps_print_state_flags(int flags, const char *context)
{
    static const NAME_MASK flags_mask[] = {
	"NOFORWARD", PS_STATE_FLAG_NOFORWARD,
	"NEW", PS_STATE_FLAG_NEW,
	"BLIST_FAIL", PS_STATE_FLAG_BLIST_FAIL,
	"HANGUP", PS_STATE_FLAG_HANGUP,
	"CACHE_EXPIRED", PS_STATE_FLAG_CACHE_EXPIRED,

	"PENAL_UPDATE", PS_STATE_FLAG_PENAL_UPDATE,
	"PENAL_FAIL", PS_STATE_FLAG_PENAL_FAIL,

	"PREGR_FAIL", PS_STATE_FLAG_PREGR_FAIL,
	"PREGR_PASS", PS_STATE_FLAG_PREGR_PASS,
	"PREGR_TODO", PS_STATE_FLAG_PREGR_TODO,
	"PREGR_DONE", PS_STATE_FLAG_PREGR_DONE,

	"DNSBL_FAIL", PS_STATE_FLAG_DNSBL_FAIL,
	"DNSBL_PASS", PS_STATE_FLAG_DNSBL_PASS,
	"DNSBL_TODO", PS_STATE_FLAG_DNSBL_TODO,
	"DNSBL_DONE", PS_STATE_FLAG_DNSBL_DONE,

	"PIPEL_FAIL", PS_STATE_FLAG_PIPEL_FAIL,
	"PIPEL_PASS", PS_STATE_FLAG_PIPEL_PASS,
	"PIPEL_TODO", PS_STATE_FLAG_PIPEL_TODO,
	"PIPEL_SKIP", PS_STATE_FLAG_PIPEL_SKIP,

	"NSMTP_FAIL", PS_STATE_FLAG_NSMTP_FAIL,
	"NSMTP_PASS", PS_STATE_FLAG_NSMTP_PASS,
	"NSMTP_TODO", PS_STATE_FLAG_NSMTP_TODO,
	"NSMTP_SKIP", PS_STATE_FLAG_NSMTP_SKIP,

	"BARLF_FAIL", PS_STATE_FLAG_BARLF_FAIL,
	"BARLF_PASS", PS_STATE_FLAG_BARLF_PASS,
	"BARLF_TODO", PS_STATE_FLAG_BARLF_TODO,
	"BARLF_SKIP", PS_STATE_FLAG_BARLF_SKIP,
	0,
    };

    return (str_name_mask_opt((VSTRING *) 0, context, flags_mask, flags,
			      NAME_MASK_PIPE | NAME_MASK_NUMBER));
}
