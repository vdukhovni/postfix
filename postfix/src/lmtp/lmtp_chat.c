/*++
/* NAME
/*	lmtp_chat 3
/* SUMMARY
/*	LMTP client request/response support
/* SYNOPSIS
/*	#include "lmtp.h"
/*
/*	typedef struct {
/* .in +4
/*		int code;
/*		char *dsn;
/*		char *str;
/*		VSTRING *dsn_buf;
/*		VSTRING *str_buf;
/* .in -4
/*	} LMTP_RESP;
/*
/*	void	lmtp_chat_cmd(state, format, ...)
/*	LMTP_STATE *state;
/*	char	*format;
/*
/*	LMTP_RESP *lmtp_chat_resp(state)
/*	LMTP_STATE *state;
/*
/*	void	lmtp_chat_notify(state)
/*	LMTP_STATE *state;
/*
/*	void	lmtp_chat_reset(state)
/*	LMTP_STATE *state;
/* DESCRIPTION
/*	This module implements LMTP client support for request/reply
/*	conversations, and maintains a limited LMTP transaction log.
/*
/*	lmtp_chat_cmd() formats a command and sends it to an LMTP server.
/*	Optionally, the command is logged.
/*
/*	lmtp_chat_resp() reads one LMTP server response. It separates the
/*	numerical status code from the text, and concatenates multi-line
/*	responses to one string, using a newline as separator.
/*	Optionally, the server response is logged.
/*
/*	lmtp_chat_notify() sends a copy of the LMTP transaction log
/*	to the postmaster for review. The postmaster notice is sent only
/*	when delivery is possible immediately. It is an error to call
/*	lmtp_chat_notify() when no LMTP transaction log exists.
/*
/*	lmtp_chat_reset() resets the transaction log. This is
/*	typically done at the beginning or end of an LMTP session,
/*	or within a session to discard non-error information.
/*	In addition, lmtp_chat_reset() resets the per-session error
/*	status bits and flags.
/*
/*	lmtp_chat_fake() constructs a synthetic LMTP server response
/*	from its arguments.
/* DIAGNOSTICS
/*	Fatal errors: memory allocation problem, server response exceeds
/*	configurable limit.
/*	All other exceptions are handled by long jumps (see smtp_stream(3)).
/* SEE ALSO
/*	smtp_stream(3) LMTP session I/O support
/*	msg(3) generic logging interface
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*
/*	Alterations for LMTP by:
/*	Philip A. Prindeville
/*	Mirapoint, Inc.
/*	USA.
/*
/*	Additional work on LMTP by:
/*	Amos Gouaux
/*	University of Texas at Dallas
/*	P.O. Box 830688, MC34
/*	Richardson, TX 75083, USA
/*--*/

/* System library. */

#include <sys_defs.h>
#include <stdlib.h>			/* 44BSD stdarg.h uses abort() */
#include <stdarg.h>
#include <ctype.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>

/* Utility library. */

#include <msg.h>
#include <vstring.h>
#include <vstream.h>
#include <argv.h>
#include <stringops.h>
#include <line_wrap.h>
#include <mymalloc.h>

/* Global library. */

#include <recipient_list.h>
#include <deliver_request.h>
#include <smtp_stream.h>
#include <mail_params.h>
#include <mail_addr.h>
#include <post_mail.h>
#include <mail_error.h>
#include <dsn_util.h>

/* Application-specific. */

#include "lmtp.h"

#define LEN	VSTRING_LEN

/* lmtp_chat_reset - reset LMTP transaction log */

void    lmtp_chat_reset(LMTP_STATE *state)
{
    if (state->history) {
	argv_free(state->history);
	state->history = 0;
    }
    /* What's status without history? */
    state->status = 0;
    state->error_mask = 0;
}

/* lmtp_chat_append - append record to LMTP transaction log */

static void lmtp_chat_append(LMTP_STATE *state, char *direction, char *data)
{
    char   *line;

    if (state->history == 0)
	state->history = argv_alloc(10);
    line = concatenate(direction, data, (char *) 0);
    argv_add(state->history, line, (char *) 0);
    myfree(line);
}

/* lmtp_chat_cmd - send an LMTP command */

void    lmtp_chat_cmd(LMTP_STATE *state, char *fmt,...)
{
    LMTP_SESSION *session = state->session;
    va_list ap;

    /*
     * Format the command, and update the transaction log.
     */
    va_start(ap, fmt);
    vstring_vsprintf(state->buffer, fmt, ap);
    va_end(ap);
    lmtp_chat_append(state, "Out: ", STR(state->buffer));

    /*
     * Optionally log the command first, so we can see in the log what the
     * program is trying to do.
     */
    if (msg_verbose)
	msg_info("> %s: %s", session->namaddr, STR(state->buffer));

    /*
     * Send the command to the LMTP server.
     */
    smtp_fputs(STR(state->buffer), LEN(state->buffer), session->stream);
}

/* lmtp_chat_resp - read and process LMTP server response */

LMTP_RESP *lmtp_chat_resp(LMTP_STATE *state)
{
    static LMTP_RESP rdata;
    LMTP_SESSION *session = state->session;
    char   *cp;
    int     last_char;
    int     three_digs = 0;
    size_t  len;

    /*
     * Initialize the response data buffer.
     */
    if (rdata.str_buf == 0) {
	rdata.dsn_buf = vstring_alloc(10);
	rdata.str_buf = vstring_alloc(100);
    }

    /*
     * Censor out non-printable characters in server responses. Concatenate
     * multi-line server responses. Separate the status code from the text.
     * Leave further parsing up to the application.
     */
    VSTRING_RESET(rdata.str_buf);
    for (;;) {
	last_char = smtp_get(state->buffer, session->stream, var_line_limit);
	printable(STR(state->buffer), '?');
	if (last_char != '\n')
	    msg_warn("%s: response longer than %d: %.30s...",
		     session->namaddr, var_line_limit, STR(state->buffer));
	if (msg_verbose)
	    msg_info("< %s: %s", session->namaddr, STR(state->buffer));

	/*
	 * Defend against a denial of service attack by limiting the amount
	 * of multi-line text that we are willing to store.
	 */
	if (LEN(rdata.str_buf) < var_line_limit) {
	    if (VSTRING_LEN(rdata.str_buf))
		VSTRING_ADDCH(rdata.str_buf, '\n');
	    vstring_strcat(rdata.str_buf, STR(state->buffer));
	    lmtp_chat_append(state, "In:  ", STR(state->buffer));
	}

	/*
	 * Parse into code and text. Ignore unrecognized garbage. This means
	 * that any character except space (or end of line) will have the
	 * same effect as the '-' line continuation character.
	 */
	for (cp = STR(state->buffer); *cp && ISDIGIT(*cp); cp++)
	     /* void */ ;
	if ((three_digs = (cp - STR(state->buffer) == 3)) != 0) {
	    if (*cp == '-')
		continue;
	    if (*cp == ' ' || *cp == 0)
		break;
	}

	/*
	 * XXX Do not ignore garbage when ESMTP command pipelining is turned
	 * on. After sending ".<CR><LF>QUIT<CR><LF>", Postfix might recognize
	 * the server's 2XX QUIT reply as a 2XX END-OF-DATA reply after
	 * garbage, causing mail to be lost. Instead, make a long jump so
	 * that all recipients of multi-recipient mail get consistent
	 * treatment.
	 */
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	if (state->features & LMTP_FEATURE_PIPELINING) {
	    msg_warn("non-LMTP response from %s: %.100s",
		     session->namaddr, STR(state->buffer));
	    vstream_longjmp(session->stream, SMTP_ERR_PROTO);
	}
    }

    /*
     * Extract RFC 821 reply code and RFC 2034 detail code. Use a default
     * detail code if none was given.
     * 
     * Ignore out-of-protocol enhanced status codes: codes that accompany 3XX
     * replies, or codes whose initial digit is out of sync with the reply
     * code.
     * 
     * XXX Potential stability problem. In order to save memory, the queue
     * manager stores DSNs in a compact manner:
     * 
     * - empty strings are represented by null pointers,
     * 
     * - the status and reason are required to be non-empty.
     * 
     * Other Postfix daemons inherit this behavior, because they use the same
     * DSN support code. This means that everything that receives DSNs must
     * cope with null pointers for the optional DSN attributes, and that
     * everything that provides DSN information must provide a non-empty
     * status and reason, otherwise the DSN support code wil panic().
     * 
     * Thus, when the remote server sends a malformed reply (or 3XX out of
     * context) we should not panic() in DSN_COPY() just because we don't
     * have a status. Robustness suggests that we supply a status here, and
     * that we leave it up to the down-stream code to override the
     * server-supplied status in case of an error we can't detect here, such
     * as an out-of-order server reply.
     */
    VSTRING_TERMINATE(rdata.str_buf);
    vstring_strcpy(rdata.dsn_buf, "5.5.0");	/* SAFETY! protocol error */
    if (three_digs != 0) {
	rdata.code = atoi(STR(state->buffer));
	if (strchr("245", STR(state->buffer)[0]) != 0) {
	    for (cp = STR(state->buffer) + 4; *cp == ' '; cp++)
		 /* void */ ;
	    if ((len = dsn_valid(cp)) > 0 && *cp == *STR(state->buffer)) {
		vstring_strncpy(rdata.dsn_buf, cp, len);
	    } else {
		vstring_strcpy(rdata.dsn_buf, "0.0.0");
		STR(rdata.dsn_buf)[0] = STR(state->buffer)[0];
	    }
	}
    } else {
	rdata.code = 0;
    }
    rdata.dsn = STR(rdata.dsn_buf);
    rdata.str = STR(rdata.str_buf);
    return (&rdata);
}

/* print_line - line_wrap callback */

static void print_line(const char *str, int len, int indent, char *context)
{
    VSTREAM *notice = (VSTREAM *) context;

    post_mail_fprintf(notice, " %*s%.*s", indent, "", len, str);
}

/* lmtp_chat_notify - notify postmaster */

void    lmtp_chat_notify(LMTP_STATE *state)
{
    char   *myname = "lmtp_chat_notify";
    LMTP_SESSION *session = state->session;
    VSTREAM *notice;
    char  **cpp;

    /*
     * Sanity checks.
     */
    if (state->history == 0)
	msg_panic("%s: no conversation history", myname);
    if (msg_verbose)
	msg_info("%s: notify postmaster", myname);

    /*
     * Construct a message for the postmaster, explaining what this is all
     * about. This is junk mail: don't send it when the mail posting service
     * is unavailable, and use the double bounce sender address, to prevent
     * mail bounce wars. Always prepend one space to message content that we
     * generate from untrusted data.
     */
#define NULL_TRACE_FLAGS	0
#define LENGTH	78
#define INDENT	4

    notice = post_mail_fopen_nowait(mail_addr_double_bounce(),
				    var_error_rcpt,
				    CLEANUP_FLAG_MASK_INTERNAL,
				    NULL_TRACE_FLAGS);
    if (notice == 0) {
	msg_warn("postmaster notify: %m");
	return;
    }
    post_mail_fprintf(notice, "From: %s (Mail Delivery System)",
		      mail_addr_mail_daemon());
    post_mail_fprintf(notice, "To: %s (Postmaster)", var_error_rcpt);
    post_mail_fprintf(notice, "Subject: %s LMTP client: errors from %s",
		      var_mail_name, session->namaddr);
    post_mail_fputs(notice, "");
    post_mail_fprintf(notice, "Unexpected response from %s.", session->namaddr);
    post_mail_fputs(notice, "");
    post_mail_fputs(notice, "Transcript of session follows.");
    post_mail_fputs(notice, "");
    argv_terminate(state->history);
    for (cpp = state->history->argv; *cpp; cpp++)
	line_wrap(printable(*cpp, '?'), LENGTH, INDENT, print_line,
		  (char *) notice);
    (void) post_mail_fclose(notice);
}
