/*++
/* NAME
/*	postscreen_smtpd 3
/* SUMMARY
/*	postscreen built-in SMTP server engine
/* SYNOPSIS
/*	#include <postscreen.h>
/*
/*	void	ps_smtpd_init(void)
/*
/*	void	ps_smtpd_tests(state)
/*	PS_STATE *state;
/* DESCRIPTION
/*	ps_smtpd_init() performs one-time per-process initialization.
/*
/*	ps_smtpd_tests() starts up an SMTP server engine for deep
/*	protocol tests and for collecting helo/sender/recipient
/*	information.
/*
/*	Unlike the Postfix SMTP server, this engine does not announce
/*	PIPELINING support. This exposes spambots that pipeline
/*	their commands anyway. To pass this test, the client has
/*	to speak SMTP all the way to the RCPT TO command.
/*
/*	No support is announced for AUTH, STARTTLS, XCLIENT or
/*	XFORWARD. Clients that need this should be whitelisted or
/*	should talk directly to the submission service.  Support
/*	for STARTTLS may be added later.
/*
/*	The engine rejects RCPT TO and VRFY commands with the
/*	state->rcpt_reply response which depends on program history,
/*	rejects ETRN with a generic response, and closes the
/*	connection after QUIT.
/*
/*	Since this engine defers or rejects all non-junk commands,
/*	there is no point maintaining separate counters for "error"
/*	commands and "junk" commands.  Instead, the engine maintains
/*	a per-session command counter, and terminates the session
/*	with a 421 reply when the command count exceeds the limit.
/*
/*	We limit the command count so that we don't have to worry
/*	about becoming blocked while sending responses (20 replies
/*	of about 40 bytes plus greeting banners). Otherwise we would
/*	have to make the output event-driven, just like the input.
/* PROTOCOL INSPECTION VERSUS CONTENT INSPECTION
/*	The goal of postscreen is to keep spambots away from Postfix.
/*	To recognize spambots, postscreen measures properties of
/*	the client IP address and of the client SMTP protocol
/*	implementation.  These client properties don't change with
/*	each delivery attempt.  Therefore it is possible to make a
/*	long-term decision after a single measurement.  For example,
/*	allow a good client to skip the DNSBL test for 24 hours,
/*	or to skip the pipelining test for one week.
/*
/*	If postscreen were to measure properties of message content
/*	(MIME compliance, etc.) then it would measure properties
/*	that may change with each delivery attempt.  Here, it would
/*	be wrong to make a long-term decision after a single
/*	measurement. Instead, postscreen would need to develop a
/*	ranking based on the content of multiple messages from the
/*	same client.
/*
/*	Many spambots avoid spamming the same site repeatedly.
/*	Thus, postscreen must make decisions after a single
/*	measurement. Message content is not a good indicator for
/*	making long-term decisions after single measurements, and
/*	that is why postscreen does not inspect message content.
/* REJECTING RCPT TO VERSUS SENDING LIVE SOCKETS TO SMTPD(8)
/*	When deep protocol tests are enabled, postscreen rejects
/*	the RCPT TO command from a good client, and forces it to
/*	deliver mail in a later session. This is why deep protocol
/*	tests have a longer expiration time than pre-handshake
/*	tests.
/*
/*	Instead, postscreen could send the network socket to smtpd(8)
/*	and ship the session history (including TLS and other SMTP
/*	or non-SMTP attributes) as auxiliary data. The Postfix SMTP
/*	server would then use new code to replay the session history,
/*	and would use existing code to validate the client, helo,
/*	sender and recipient address.
/*
/*	Such an approach would increase the implementation and
/*	maintenance effort, because:
/*
/*	1) New replay code would be needed in smtpd(8), such that
/*	the HELO, EHLO, and MAIL command handlers can delay their
/*	error responses until the RCPT TO reply.
/*
/*	2) postscreen(8) would have to implement more of smtpd(8)'s
/*	syntax checks, to avoid confusing delayed "syntax error"
/*	and other error responses syntax error responses while
/*	replaying history.
/*
/*	3) New code would be needed in postscreen(8) and smtpd(8)
/*	to send and receive the session history (including TLS and
/*	other SMTP or non-SMTP attributes) as auxiliary data while
/*	sending the network socket from postscreen(8) to smtpd(8).
/* REJECTING RCPT TO VERSUS PROXYING LIVE SESSIONS TO SMTPD(8)
/*	An alternative would be to proxy the session history to a
/*	real Postfix SMTP process, presumably passing TLS and other
/*	attributes via an extended XCLIENT implementation. That
/*	would require all the work described in 2) above, plus
/*	duplication of all the features of the smtpd(8) TLS engine,
/*	plus additional XCLIENT support for a lot more attributes.
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
#include <ctype.h>

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

/* Utility library. */

#include <msg.h>
#include <stringops.h>
#include <mymalloc.h>
#include <iostuff.h>
#include <vstring.h>

/* Global library. */

#include <mail_params.h>
#include <mail_proto.h>
#include <is_header.h>
#include <string_list.h>

/* Application-specific. */

#include <postscreen.h>

 /*
  * Plan for future body processing. See smtp-sink.c. For now, we have no
  * per-session push-back except for the single-character push-back that
  * VSTREAM guarantees after we read one character.
  */
#define PS_SMTPD_HAVE_PUSH_BACK(state)	(0)
#define PS_SMTPD_PUSH_BACK_CHAR(state, ch) \
	vstream_ungetc((state)->smtp_client_stream, (ch))
#define PS_SMTPD_NEXT_CHAR(state) \
	VSTREAM_GETC((state)->smtp_client_stream)

 /*
  * Dynamic reply strings. To minimize overhead we format these once.
  */
static char *ps_smtpd_greeting;		/* smtp banner */
static char *ps_smtpd_helo_reply;	/* helo reply */
static char *ps_smtpd_ehlo_reply;	/* multi-line ehlo reply */
static char *ps_smtpd_timeout_reply;	/* timeout reply */
static char *ps_smtpd_421_reply;	/* generic final_reply value */

 /*
  * Forward declaration, needed by PS_CLEAR_EVENT_REQUEST.
  */
static void ps_smtpd_time_event(int, char *);

 /*
  * Command parser support.
  */
#define PS_SMTPD_NEXT_TOKEN(ptr) mystrtok(&(ptr), " ")

 /*
  * Encapsulation. We must not forget turn off input/timer events when we
  * terminate the SMTP protocol engine.
  * 
  * It would be safer to turn off input/timer events after each event, and to
  * turn on input/timer events again when we want more input. But experience
  * with the Postfix smtp-source and smtp-sink tools shows that this would
  * noticeably increase the run-time cost.
  */
#define PS_CLEAR_EVENT_DROP_SESSION_STATE(state, event, reply) do { \
	PS_CLEAR_EVENT_REQUEST(vstream_fileno((state)->smtp_client_stream), \
				   (event), (char *) (state)); \
	PS_DROP_SESSION_STATE((state), (reply)); \
    } while (0);

#define PS_CLEAR_EVENT_HANGUP(state, event) do { \
	PS_CLEAR_EVENT_REQUEST(vstream_fileno((state)->smtp_client_stream), \
				   (event), (char *) (state)); \
	ps_hangup_event(state); \
    } while (0);

/* ps_helo_cmd - record HELO and respond */

static int ps_helo_cmd(PS_STATE *state, char *args)
{
    char   *helo_name = PS_SMTPD_NEXT_TOKEN(args);

    /*
     * smtpd(8) incompatibility: we ignore extra words; smtpd(8) saves them.
     */
    if (helo_name == 0)
	return (PS_SEND_REPLY(state, "501 Syntax: HELO hostname\r\n"));

    PS_STRING_UPDATE(state->helo_name, helo_name);
    PS_STRING_RESET(state->sender);
    /* Don't downgrade state->protocol, in case some test depends on this. */
    return (PS_SEND_REPLY(state, ps_smtpd_helo_reply));
}

/* ps_ehlo_cmd - record EHLO and respond */

static int ps_ehlo_cmd(PS_STATE *state, char *args)
{
    char   *helo_name = PS_SMTPD_NEXT_TOKEN(args);

    /*
     * smtpd(8) incompatibility: we ignore extra words; smtpd(8) saves them.
     */
    if (helo_name == 0)
	return (PS_SEND_REPLY(state, "501 Syntax: EHLO hostname\r\n"));

    PS_STRING_UPDATE(state->helo_name, helo_name);
    PS_STRING_RESET(state->sender);
    state->protocol = MAIL_PROTO_ESMTP;
    return (PS_SEND_REPLY(state, ps_smtpd_ehlo_reply));
}

/* ps_extract_addr - extract MAIL/RCPT address, unquoted form */

static char *ps_extract_addr(VSTRING *result, const char *string)
{
    const unsigned char *cp = (const unsigned char *) string;
    int     stop_at;
    int     inquote = 0;

    /*
     * smtpd(8) incompatibility: we allow more invalid address forms, and we
     * don't strip @site1,site2:user@site3 route addresses. We are not going
     * to deliver them so we won't have to worry about addresses that end up
     * being nonsense after stripping. This may have to change when we pass
     * the socket to a real SMTP server and replay message envelope commands.
     */

    /* Skip SP characters. */
    while (*cp && *cp == ' ')
	cp++;

    /* Choose the terminator for <addr> or bare addr. */
    if (*cp == '<') {
	cp++;
	stop_at = '>';
    } else {
	stop_at = ' ';
    }

    /* Skip to terminator or end. */
    VSTRING_RESET(result);
    for ( /* void */ ; *cp; cp++) {
	if (!inquote && *cp == stop_at)
	    break;
	if (*cp == '"') {
	    inquote = !inquote;
	} else {
	    if (*cp == '\\' && *++cp == 0)
		break;
	    VSTRING_ADDCH(result, *cp);
	}
    }
    VSTRING_TERMINATE(result);
    return (STR(result));
}

/* ps_mail_cmd - record MAIL and respond */

static int ps_mail_cmd(PS_STATE *state, char *args)
{
    char   *colon;
    char   *addr;

    /*
     * smtpd(8) incompatibility: we never reject the sender, and we ignore
     * additional arguments.
     */
    if (var_ps_helo_required && state->helo_name == 0)
	return (PS_SEND_REPLY(state,
			      "503 5.5.1 Error: send HELO/EHLO first\r\n"));
    if (state->sender != 0)
	return (PS_SEND_REPLY(state,
			      "503 5.5.1 Error: nested MAIL command\r\n"));
    if (args == 0 || (colon = strchr(args, ':')) == 0)
	return (PS_SEND_REPLY(state,
			      "501 5.5.4 Syntax: MAIL FROM:<address>\r\n"));
    if ((addr = ps_extract_addr(ps_temp, colon + 1)) == 0)
	return (PS_SEND_REPLY(state,
			      "501 5.1.7 Bad sender address syntax\r\n"));
    PS_STRING_UPDATE(state->sender, addr);
    return (PS_SEND_REPLY(state, "250 2.1.0 Ok\r\n"));
}

/* ps_rcpt_cmd record RCPT and respond */

static int ps_rcpt_cmd(PS_STATE *state, char *args)
{
    char   *colon;
    char   *addr;

    /*
     * smtpd(8) incompatibility: we reject all recipients, and ignore
     * additional arguments.
     */
    if (state->sender == 0)
	return (PS_SEND_REPLY(state,
			      "503 5.5.1 Error: need MAIL command\r\n"));
    if (args == 0 || (colon = strchr(args, ':')) == 0)
	return (PS_SEND_REPLY(state,
			      "501 5.5.4 Syntax: RCPT TO:<address>\r\n"));
    if ((addr = ps_extract_addr(ps_temp, colon + 1)) == 0)
	return (PS_SEND_REPLY(state,
			      "501 5.1.3 Bad recipient address syntax\r\n"));
    msg_info("NOQUEUE: reject: RCPT from [%s]: %.*s; "
	     "from=<%s>, to=<%s>, proto=%s, helo=<%s>",
	     state->smtp_client_addr,
	     (int) strlen(state->rcpt_reply) - 2, state->rcpt_reply,
	     state->sender, addr, state->protocol,
	     state->helo_name ? state->helo_name : "");
    return (PS_SEND_REPLY(state, state->rcpt_reply));
}

/* ps_data_cmd - respond to DATA and disconnect */

static int ps_data_cmd(PS_STATE *state, char *args)
{

    /*
     * smtpd(8) incompatibility: we reject all requests.
     */
    if (PS_SMTPD_NEXT_TOKEN(args) != 0)
	return (PS_SEND_REPLY(state,
			      "501 5.5.4 Syntax: DATA\r\n"));
    if (state->sender == 0)
	return (PS_SEND_REPLY(state,
			      "503 5.5.1 Error: need RCPT command\r\n"));

    /*
     * We really would like to hang up the connection as early as possible,
     * so that we dont't have to deal with broken zombies that fall silent at
     * the first reject response. For now we rely on stress-dependent command
     * read timeouts.
     * 
     * If we proceed into the data phase, enforce over-all DATA time limit.
     */
    return (PS_SEND_REPLY(state,
			  "554 5.5.1 Error: no valid recipients\r\n"));
}

/* ps_rset_cmd - reset, send 250 OK */

static int ps_rset_cmd(PS_STATE *state, char *unused_args)
{
    PS_STRING_RESET(state->sender);
    return (PS_SEND_REPLY(state, "250 2.0.0 Ok\r\n"));
}

/* ps_noop_cmd - respond to something */

static int ps_noop_cmd(PS_STATE *state, char *unused_args)
{
    return (PS_SEND_REPLY(state, "250 2.0.0 Ok\r\n"));
}

/* ps_vrfy_cmd - respond to VRFY */

static int ps_vrfy_cmd(PS_STATE *state, char *args)
{

    /*
     * smtpd(8) incompatibility: we reject all requests, and ignore
     * additional arguments.
     */
    if (PS_SMTPD_NEXT_TOKEN(args) == 0)
	return (PS_SEND_REPLY(state,
			      "501 5.5.4 Syntax: VRFY address\r\n"));
    if (var_ps_disable_vrfy)
	return (PS_SEND_REPLY(state,
			      "502 5.5.1 VRFY command is disabled\r\n"));
    return (PS_SEND_REPLY(state, state->rcpt_reply));
}

/* ps_etrn_cmd - reset, send 250 OK */

static int ps_etrn_cmd(PS_STATE *state, char *args)
{

    /*
     * smtpd(8) incompatibility: we reject all requests, and ignore
     * additional arguments.
     */
    if (var_ps_helo_required && state->helo_name == 0)
	return (PS_SEND_REPLY(state,
			      "503 5.5.1 Error: send HELO/EHLO first\r\n"));
    if (PS_SMTPD_NEXT_TOKEN(args) == 0)
	return (PS_SEND_REPLY(state,
			      "500 Syntax: ETRN domain\r\n"));
    return (PS_SEND_REPLY(state, "458 Unable to queue messages\r\n"));
}

/* ps_quit_cmd - respond to QUIT and disconnect */

static int ps_quit_cmd(PS_STATE *state, char *unused_args)
{
    const char *myname = "ps_quit_cmd";

    PS_CLEAR_EVENT_DROP_SESSION_STATE(state, ps_smtpd_time_event,
				      "221 2.0.0 Bye\r\n");
    /* Caution: state is now a dangling pointer. */
    return (0);
}

/* ps_smtpd_time_event - handle per-session time limit */

static void ps_smtpd_time_event(int event, char *context)
{
    const char *myname = "ps_smtpd_time_event";
    PS_STATE *state = (PS_STATE *) context;

    if (msg_verbose > 1)
	msg_info("%s: sq=%d cq=%d event %d on smtp socket %d from %s:%s flags=%s",
		 myname, ps_post_queue_length, ps_check_queue_length,
		 event, vstream_fileno(state->smtp_client_stream),
		 state->smtp_client_addr, state->smtp_client_port,
		 ps_print_state_flags(state->flags, myname));

    msg_info("COMMAND TIME LIMIT from %s", state->smtp_client_addr);
    PS_CLEAR_EVENT_DROP_SESSION_STATE(state, ps_smtpd_time_event,
				      ps_smtpd_timeout_reply);
}

 /*
  * The table of all SMTP commands that we know.
  */
typedef struct {
    const char *name;
    int     (*action) (PS_STATE *, char *);
    int     flags;			/* see below */
} PS_SMTPD_COMMAND;

#define PS_SMTPD_CMD_FLAG_NONE		(0)	/* no flags (i.e. disabled) */
#define PS_SMTPD_CMD_FLAG_ENABLE	(1<<0)	/* command is enabled */
#define PS_SMTPD_CMD_FLAG_DESTROY	(1<<1)	/* dangling pointer alert */

static const PS_SMTPD_COMMAND command_table[] = {
    "HELO", ps_helo_cmd, PS_SMTPD_CMD_FLAG_ENABLE,
    "EHLO", ps_ehlo_cmd, PS_SMTPD_CMD_FLAG_ENABLE,
    "XCLIENT", ps_noop_cmd, PS_SMTPD_CMD_FLAG_NONE,
    "XFORWARD", ps_noop_cmd, PS_SMTPD_CMD_FLAG_NONE,
    "AUTH", ps_noop_cmd, PS_SMTPD_CMD_FLAG_NONE,
    "MAIL", ps_mail_cmd, PS_SMTPD_CMD_FLAG_ENABLE,
    "RCPT", ps_rcpt_cmd, PS_SMTPD_CMD_FLAG_ENABLE,
    "DATA", ps_data_cmd, PS_SMTPD_CMD_FLAG_ENABLE,
    /* ".", ps_dot_cmd, PS_SMTPD_CMD_FLAG_NONE, */
    "RSET", ps_rset_cmd, PS_SMTPD_CMD_FLAG_ENABLE,
    "NOOP", ps_noop_cmd, PS_SMTPD_CMD_FLAG_ENABLE,
    "VRFY", ps_vrfy_cmd, PS_SMTPD_CMD_FLAG_ENABLE,
    "ETRN", ps_etrn_cmd, PS_SMTPD_CMD_FLAG_ENABLE,
    "QUIT", ps_quit_cmd, PS_SMTPD_CMD_FLAG_ENABLE | PS_SMTPD_CMD_FLAG_DESTROY,
    0,
};

/* ps_smtpd_read_event - pseudo responder */

static void ps_smtpd_read_event(int event, char *context)
{
    const char *myname = "ps_smtpd_read_event";
    PS_STATE *state = (PS_STATE *) context;
    int     ch;
    struct cmd_trans {
	int     state;
	int     want;
	int     next_state;
    };

#define PS_SMTPD_CMD_ST_ANY		0
#define PS_SMTPD_CMD_ST_CR		1
#define PS_SMTPD_CMD_ST_CR_LF		2

    static const struct cmd_trans cmd_trans[] = {
	PS_SMTPD_CMD_ST_ANY, '\r', PS_SMTPD_CMD_ST_CR,
	PS_SMTPD_CMD_ST_CR, '\n', PS_SMTPD_CMD_ST_CR_LF,
	0, 0, 0,
    };
    const struct cmd_trans *transp;
    char   *cmd_buffer_ptr;
    char   *command;
    const PS_SMTPD_COMMAND *cmdp;
    int     write_stat;

    if (msg_verbose > 1)
	msg_info("%s: sq=%d cq=%d event %d on smtp socket %d from %s:%s flags=%s",
		 myname, ps_post_queue_length, ps_check_queue_length,
		 event, vstream_fileno(state->smtp_client_stream),
		 state->smtp_client_addr, state->smtp_client_port,
		 ps_print_state_flags(state->flags, myname));

    /*
     * Basic liveness requirements.
     * 
     * Drain all input in the VSTREAM buffer, otherwise this socket will not
     * receive further read event notification until the client disconnects!
     * 
     * Don't try to read input before it has arrived, otherwise we would starve
     * the pseudo threads of other sessions. Get out of here as soon as the
     * VSTREAM read buffer dries up. Do not look for more input in kernel
     * buffers. That input wasn't likely there when ps_smtpd_read_event() was
     * called. Also, yielding the pseudo thread will improve fairness for
     * other pseudo threads.
     */
#define PS_SMTPD_BUFFER_EMPTY(state) \
	(!PS_SMTPD_HAVE_PUSH_BACK(state) \
	&& vstream_peek(state->smtp_client_stream) <= 0)

    /*
     * Note: on entry into this function the VSTREAM buffer is still empty,
     * so we test the "no more input" condition at the bottom of the loops.
     */
    for (;;) {

	/*
	 * Read one command line, possibly one fragment at a time.
	 */
	for (;;) {

	    if ((ch = PS_SMTPD_NEXT_CHAR(state)) == VSTREAM_EOF) {
		PS_CLEAR_EVENT_HANGUP(state, ps_smtpd_time_event);
		return;
	    }

	    /*
	     * Sanity check. We don't want to store infinitely long commands.
	     */
	    if (state->read_state == PS_SMTPD_CMD_ST_ANY
		&& VSTRING_LEN(state->cmd_buffer) >= var_line_limit) {
		msg_info("COMMAND LENGTH LIMIT from %s",
			 state->smtp_client_addr);
		PS_CLEAR_EVENT_DROP_SESSION_STATE(state, ps_smtpd_time_event,
						  ps_smtpd_421_reply);
		return;
	    }
	    VSTRING_ADDCH(state->cmd_buffer, ch);

	    /*
	     * Try to match the current character desired by the state
	     * machine. If that fails, try to restart the machine with a
	     * match for its first state. smtpd(8) incompatibility: we
	     * require that lines end in <CR><LF>, while smtpd(8) allows
	     * lines ending in <CR><LF> and bare <LF>.
	     */
	    for (transp = cmd_trans; transp->state != state->read_state; transp++)
		if (transp->want == 0)
		    msg_panic("%s: command_read: unknown state: %d",
			      myname, state->read_state);
	    if (ch == transp->want)
		state->read_state = transp->next_state;
	    else if (ch == cmd_trans[0].want)
		state->read_state = cmd_trans[0].next_state;
	    else
		state->read_state = PS_SMTPD_CMD_ST_ANY;
	    if (state->read_state == PS_SMTPD_CMD_ST_CR_LF) {
		vstring_truncate(state->cmd_buffer,
				 VSTRING_LEN(state->cmd_buffer) - 2);
		break;
	    }

	    /*
	     * Bare newline test.
	     */
	    if (ch == '\n') {
		if ((state->flags & PS_STATE_MASK_BARLF_TODO_SKIP)
		    == PS_STATE_FLAG_BARLF_TODO) {
		    msg_info("BARE NEWLINE from %s", state->smtp_client_addr);
		    PS_FAIL_SESSION_STATE(state, PS_STATE_FLAG_BARLF_FAIL);
		    PS_UNPASS_SESSION_STATE(state, PS_STATE_FLAG_BARLF_PASS);
		    state->barlf_stamp = PS_TIME_STAMP_DISABLED;	/* XXX */
		    /* Skip this test for the remainder of this session. */
		    PS_SKIP_SESSION_STATE(state, "bare newline test",
					  PS_STATE_FLAG_BARLF_SKIP);
		    switch (ps_barlf_action) {
		    case PS_ACT_DROP:
			PS_CLEAR_EVENT_DROP_SESSION_STATE(state,
							ps_smtpd_time_event,
					    "521 5.5.1 Protocol error\r\n");
			return;
		    case PS_ACT_ENFORCE:
			PS_ENFORCE_SESSION_STATE(state,
					    "550 5.5.1 Protocol error\r\n");
			break;
		    case PS_ACT_IGNORE:
			PS_UNFAIL_SESSION_STATE(state,
						PS_STATE_FLAG_BARLF_FAIL);
			/* Temporarily whitelist until something expires. */
			PS_PASS_SESSION_STATE(state, "bare newline test",
					      PS_STATE_FLAG_BARLF_PASS);
			state->barlf_stamp = event_time() + ps_min_ttl;
			break;
		    default:
			msg_panic("%s: unknown bare_newline action value %d",
				  myname, ps_barlf_action);
		    }
		}
		vstring_truncate(state->cmd_buffer,
				 VSTRING_LEN(state->cmd_buffer) - 1);
		break;
	    }

	    /*
	     * Yield this pseudo thread when the VSTREAM buffer is empty in
	     * the middle of a command.
	     * 
	     * XXX Do not reset the read timeout. The entire command must be
	     * received within the time limit.
	     */
	    if (PS_SMTPD_BUFFER_EMPTY(state))
		return;
	}

	/*
	 * Terminate the command line, and reset the command buffer write
	 * pointer and state machine in preparation for the next command. For
	 * this to work as expected, VSTRING_RESET() must be non-destructive.
	 */
	VSTRING_TERMINATE(state->cmd_buffer);
	state->read_state = PS_SMTPD_CMD_ST_ANY;
	VSTRING_RESET(state->cmd_buffer);

	/*
	 * Process the command line.
	 * 
	 * Caution: some command handlers terminate the session and destroy the
	 * session state structure. When this happens we must leave the SMTP
	 * engine to avoid a dangling pointer problem.
	 */
	cmd_buffer_ptr = vstring_str(state->cmd_buffer);
	if (msg_verbose)
	    msg_info("< %s:%s: %s", state->smtp_client_addr,
		     state->smtp_client_port, cmd_buffer_ptr);

	/* Parse the command name. */
	if ((command = PS_SMTPD_NEXT_TOKEN(cmd_buffer_ptr)) == 0)
	    command = "";

	/*
	 * The non-SMTP, PIPELINING and command COUNT tests depend on the
	 * client command handler.
	 * 
	 * Caution: cmdp->name and cmdp->action may be null on loop exit.
	 */
	for (cmdp = command_table; cmdp->name != 0; cmdp++)
	    if (strcasecmp(command, cmdp->name) == 0)
		break;

	/* Non-SMTP command test. */
	if ((state->flags & PS_STATE_MASK_NSMTP_TODO_SKIP)
	    == PS_STATE_FLAG_NSMTP_TODO && cmdp->name == 0
	    && (is_header(command)
		|| (*var_ps_forbid_cmds
		    && string_list_match(ps_forbid_cmds, command)))) {
	    printable(command, '?');
	    msg_info("NON-SMTP COMMAND from %s %.100s",
		     state->smtp_client_addr, command);
	    PS_FAIL_SESSION_STATE(state, PS_STATE_FLAG_NSMTP_FAIL);
	    PS_UNPASS_SESSION_STATE(state, PS_STATE_FLAG_NSMTP_PASS);
	    state->nsmtp_stamp = PS_TIME_STAMP_DISABLED;	/* XXX */
	    /* Skip this test for the remainder of this SMTP session. */
	    PS_SKIP_SESSION_STATE(state, "non-smtp test",
				  PS_STATE_FLAG_NSMTP_SKIP);
	    switch (ps_nsmtp_action) {
	    case PS_ACT_DROP:
		PS_CLEAR_EVENT_DROP_SESSION_STATE(state,
						  ps_smtpd_time_event,
		   "521 5.7.0 Error: I can break rules, too. Goodbye.\r\n");
		return;
	    case PS_ACT_ENFORCE:
		PS_ENFORCE_SESSION_STATE(state,
					 "550 5.5.1 Protocol error\r\n");
		break;
	    case PS_ACT_IGNORE:
		PS_UNFAIL_SESSION_STATE(state,
					PS_STATE_FLAG_NSMTP_FAIL);
		/* Temporarily whitelist until something else expires. */
		PS_PASS_SESSION_STATE(state, "non-smtp test",
				      PS_STATE_FLAG_NSMTP_PASS);
		state->nsmtp_stamp = event_time() + ps_min_ttl;
		break;
	    default:
		msg_panic("%s: unknown non_smtp_command action value %d",
			  myname, ps_nsmtp_action);
	    }
	}
	/* Command PIPELINING test. */
	if ((state->flags & PS_STATE_MASK_PIPEL_TODO_SKIP)
	    == PS_STATE_FLAG_PIPEL_TODO && !PS_SMTPD_BUFFER_EMPTY(state)) {
	    printable(command, '?');
	    msg_info("COMMAND PIPELINING from %s after %.100s",
		     state->smtp_client_addr, command);
	    PS_FAIL_SESSION_STATE(state, PS_STATE_FLAG_PIPEL_FAIL);
	    PS_UNPASS_SESSION_STATE(state, PS_STATE_FLAG_PIPEL_PASS);
	    state->pipel_stamp = PS_TIME_STAMP_DISABLED;	/* XXX */
	    /* Skip this test for the remainder of this SMTP session. */
	    PS_SKIP_SESSION_STATE(state, "pipelining test",
				  PS_STATE_FLAG_PIPEL_SKIP);
	    switch (ps_pipel_action) {
	    case PS_ACT_DROP:
		PS_CLEAR_EVENT_DROP_SESSION_STATE(state,
						  ps_smtpd_time_event,
					    "521 5.5.1 Protocol error\r\n");
		return;
	    case PS_ACT_ENFORCE:
		PS_ENFORCE_SESSION_STATE(state,
					 "550 5.5.1 Protocol error\r\n");
		break;
	    case PS_ACT_IGNORE:
		PS_UNFAIL_SESSION_STATE(state,
					PS_STATE_FLAG_PIPEL_FAIL);
		/* Temporarily whitelist until something else expires. */
		PS_PASS_SESSION_STATE(state, "pipelining test",
				      PS_STATE_FLAG_PIPEL_PASS);
		state->pipel_stamp = event_time() + ps_min_ttl;
		break;
	    default:
		msg_panic("%s: unknown pipelining action value %d",
			  myname, ps_pipel_action);
	    }
	}

	/*
	 * The following tests don't pass until the client gets all the way
	 * to the RCPT TO command. However, the client can still fail these
	 * tests with some later command.
	 */
	if (cmdp->action == ps_rcpt_cmd) {
	    if ((state->flags & PS_STATE_MASK_BARLF_TODO_PASS_FAIL)
		== PS_STATE_FLAG_BARLF_TODO) {
		PS_PASS_SESSION_STATE(state, "bare newline test",
				      PS_STATE_FLAG_BARLF_PASS);
		/* XXX Reset to PS_TIME_STAMP_DISABLED on failure. */
		state->barlf_stamp = event_time() + var_ps_barlf_ttl;
	    }
	    if ((state->flags & PS_STATE_MASK_NSMTP_TODO_PASS_FAIL)
		== PS_STATE_FLAG_NSMTP_TODO) {
		PS_PASS_SESSION_STATE(state, "non-smtp test",
				      PS_STATE_FLAG_NSMTP_PASS);
		/* XXX Reset to PS_TIME_STAMP_DISABLED on failure. */
		state->nsmtp_stamp = event_time() + var_ps_nsmtp_ttl;
	    }
	    if ((state->flags & PS_STATE_MASK_PIPEL_TODO_PASS_FAIL)
		== PS_STATE_FLAG_PIPEL_TODO) {
		PS_PASS_SESSION_STATE(state, "pipelining test",
				      PS_STATE_FLAG_PIPEL_PASS);
		/* XXX Reset to PS_TIME_STAMP_DISABLED on failure. */
		state->pipel_stamp = event_time() + var_ps_pipel_ttl;
	    }
	}
	/* Command COUNT limit test. */
	if (++state->command_count > var_ps_cmd_count
	    && cmdp->action != ps_quit_cmd) {
	    msg_info("COMMAND COUNT LIMIT from %s", state->smtp_client_addr);
	    PS_CLEAR_EVENT_DROP_SESSION_STATE(state, ps_smtpd_time_event,
					      ps_smtpd_421_reply);
	    return;
	}
	/* Finally, execute the command. */
	if (cmdp->name == 0 || (cmdp->flags & PS_SMTPD_CMD_FLAG_ENABLE) == 0) {
	    write_stat = PS_SEND_REPLY(state,
			     "502 5.5.2 Error: command not recognized\r\n");
	} else {
	    write_stat = cmdp->action(state, cmd_buffer_ptr);
	    if (cmdp->flags & PS_SMTPD_CMD_FLAG_DESTROY)
		return;
	}

	/*
	 * Terminate the session after a write error.
	 */
	if (write_stat < 0) {
	    PS_CLEAR_EVENT_HANGUP(state, ps_smtpd_time_event);
	    return;
	}

	/*
	 * Reset the command read timeout before reading the next command.
	 */
	event_request_timer(ps_smtpd_time_event, (char *) state,
			    PS_EFF_CMD_TIME_LIMIT);

	/*
	 * Yield this pseudo thread when the VSTREAM buffer is empty.
	 */
	if (PS_SMTPD_BUFFER_EMPTY(state))
	    return;
    }
}

/* ps_smtpd_tests - per-session deep protocol test initialization */

void    ps_smtpd_tests(PS_STATE *state)
{
    static char *myname = "ps_smtpd_tests";

    /*
     * Report errors and progress in the context of this test.
     */
    PS_BEGIN_TESTS(state, "tests after SMTP handshake");

    /*
     * Initialize per-session state that is used only by the dummy engine:
     * the command read buffer and the command read state machine.
     */
    state->cmd_buffer = vstring_alloc(100);
    state->read_state = PS_SMTPD_CMD_ST_ANY;

    /*
     * Opportunistically make postscreen more useful by turning on the
     * pipelining and non-SMTP command tests when a pre-handshake test
     * failed, or when some deep test is configured as enabled.
     * 
     * XXX Make "opportunistically" configurable for each test.
     */
    state->flags |= (PS_STATE_FLAG_PIPEL_TODO | PS_STATE_FLAG_NSMTP_TODO | \
		     PS_STATE_FLAG_BARLF_TODO);

    /*
     * Send no SMTP banner to pregreeting clients. This eliminates a lot of
     * "NON-SMTP COMMAND" events, and improves sender/recipient logging.
     */
    if ((state->flags & PS_STATE_FLAG_PREGR_FAIL) == 0
	&& PS_SEND_REPLY(state, ps_smtpd_greeting) != 0) {
	ps_hangup_event(state);
	return;
    }

    /*
     * Wait for the client to respond.
     */
    PS_READ_EVENT_REQUEST2(vstream_fileno(state->smtp_client_stream),
			   ps_smtpd_read_event, ps_smtpd_time_event,
			   (char *) state, PS_EFF_CMD_TIME_LIMIT);
}

/* ps_smtpd_init - per-process deep protocol test initialization */

void    ps_smtpd_init(void)
{

    /*
     * Initialize the server banner.
     */
    vstring_sprintf(ps_temp, "220 %s\r\n", var_smtpd_banner);
    ps_smtpd_greeting = mystrdup(STR(ps_temp));

    /*
     * Initialize the HELO reply.
     */
    vstring_sprintf(ps_temp, "250 %s\r\n", var_myhostname);
    ps_smtpd_helo_reply = mystrdup(STR(ps_temp));

    /*
     * Initialize the EHLO reply.
     */
    vstring_sprintf(ps_temp,
		    "250-%s\r\n",		/* NOT: PIPELINING */
		    var_myhostname);
    if (var_message_limit)
	vstring_sprintf_append(ps_temp,
			       "250-SIZE %lu\r\n",
			       (unsigned long) var_message_limit);
    else
	vstring_sprintf_append(ps_temp,
			       "250-SIZE\r\n");
    if (var_disable_vrfy_cmd == 0)
	vstring_sprintf_append(ps_temp,
			       "250-VRFY\r\n");
    vstring_sprintf_append(ps_temp,
			   "250-ETRN\r\n"
			   "250-ENHANCEDSTATUSCODES\r\n"
			   "250-8BITMIME\r\n"
			   "250 DSN\r\n");
    ps_smtpd_ehlo_reply = mystrdup(STR(ps_temp));

    /*
     * Initialize the 421 timeout reply.
     */
    vstring_sprintf(ps_temp, "421 4.4.2 %s Error: timeout exceeded\r\n",
		    var_myhostname);
    ps_smtpd_timeout_reply = mystrdup(STR(ps_temp));

    /*
     * Initialize the generic 421 reply.
     */
    vstring_sprintf(ps_temp, "421 %s Service unavailable - try again later\r\n",
		    var_myhostname);
    ps_smtpd_421_reply = mystrdup(STR(ps_temp));
}
