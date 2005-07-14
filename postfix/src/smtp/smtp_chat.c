/*++
/* NAME
/*	smtp_chat 3
/* SUMMARY
/*	SMTP client request/response support
/* SYNOPSIS
/*	#include "smtp.h"
/*
/*	typedef struct {
/* .in +4
/*		int code;
/*		char *dsn;
/*		char *str;
/*		VSTRING *dsn_buf;
/*		VSTRING *str_buf;
/* .in -4
/*	} SMTP_RESP;
/*
/*	void	smtp_chat_cmd(session, format, ...)
/*	SMTP_SESSION *session;
/*	char	*format;
/*
/*	SMTP_RESP *smtp_chat_resp(session)
/*	SMTP_SESSION *session;
/*
/*	void	smtp_chat_notify(session)
/*	SMTP_SESSION *session;
/*
/*	void	smtp_chat_init(session)
/*	SMTP_SESSION *session;
/*
/*	void	smtp_chat_reset(session)
/*	SMTP_SESSION *session;
/* DESCRIPTION
/*	This module implements SMTP client support for request/reply
/*	conversations, and maintains a limited SMTP transaction log.
/*
/*	smtp_chat_cmd() formats a command and sends it to an SMTP server.
/*	Optionally, the command is logged.
/*
/*	smtp_chat_resp() reads one SMTP server response. It separates the
/*	numerical status code from the text, and concatenates multi-line
/*	responses to one string, using a newline as separator.
/*	Optionally, the server response is logged.
/*
/*	smtp_chat_notify() sends a copy of the SMTP transaction log
/*	to the postmaster for review. The postmaster notice is sent only
/*	when delivery is possible immediately. It is an error to call
/*	smtp_chat_notify() when no SMTP transaction log exists.
/*
/*	smtp_chat_init() initializes the per-session transaction log.
/*	This must be done at the beginning of a new SMTP session.
/*
/*	smtp_chat_reset() resets the transaction log. This is
/*	typically done at the beginning or end of an SMTP session,
/*	or within a session to discard non-error information.
/* DIAGNOSTICS
/*	Fatal errors: memory allocation problem, server response exceeds
/*	configurable limit.
/*	All other exceptions are handled by long jumps (see smtp_stream(3)).
/* SEE ALSO
/*	smtp_stream(3) SMTP session I/O support
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

#include "smtp.h"

#define LEN	VSTRING_LEN

/* smtp_chat_init - initialize SMTP transaction log */

void    smtp_chat_init(SMTP_SESSION *session)
{
    session->history = 0;
}

/* smtp_chat_reset - reset SMTP transaction log */

void    smtp_chat_reset(SMTP_SESSION *session)
{

    if (session->history) {
	argv_free(session->history);
	session->history = 0;
    }
}

/* smtp_chat_append - append record to SMTP transaction log */

static void smtp_chat_append(SMTP_SESSION *session, char *direction, char *data)
{
    char   *line;

    if (session->history == 0)
	session->history = argv_alloc(10);
    line = concatenate(direction, data, (char *) 0);
    argv_add(session->history, line, (char *) 0);
    myfree(line);
}

/* smtp_chat_cmd - send an SMTP command */

void    smtp_chat_cmd(SMTP_SESSION *session, char *fmt,...)
{
    va_list ap;

    /*
     * Format the command, and update the transaction log.
     */
    va_start(ap, fmt);
    vstring_vsprintf(session->buffer, fmt, ap);
    va_end(ap);
    smtp_chat_append(session, "Out: ", STR(session->buffer));

    /*
     * Optionally log the command first, so we can see in the log what the
     * program is trying to do.
     */
    if (msg_verbose)
	msg_info("> %s: %s", session->namaddr, STR(session->buffer));

    /*
     * Send the command to the SMTP server.
     */
    smtp_fputs(STR(session->buffer), LEN(session->buffer), session->stream);

    /*
     * Force flushing of output does not belong here. It is done in the
     * smtp_loop() main protocol loop when reading the server response, and
     * in smtp_helo() when reading the EHLO response after sending the EHLO
     * command.
     * 
     * If we do forced flush here, then we must longjmp() on error, and a
     * matching "prepare for disaster" error handler must be set up before
     * every smtp_chat_cmd() call.
     */
#if 0

    /*
     * Flush unsent data to avoid timeouts after slow DNS lookups.
     */
    if (time((time_t *) 0) - vstream_ftime(session->stream) > 10)
	vstream_fflush(session->stream);

    /*
     * Abort immediately if the connection is broken.
     */
    if (vstream_ftimeout(session->stream))
	vstream_longjmp(session->stream, SMTP_ERR_TIME);
    if (vstream_ferror(session->stream))
	vstream_longjmp(session->stream, SMTP_ERR_EOF);
#endif
}

/* smtp_chat_resp - read and process SMTP server response */

SMTP_RESP *smtp_chat_resp(SMTP_SESSION *session)
{
    static SMTP_RESP rdata;
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
	last_char = smtp_get(session->buffer, session->stream, var_line_limit);
	printable(STR(session->buffer), '?');
	if (last_char != '\n')
	    msg_warn("%s: response longer than %d: %.30s...",
		     session->namaddr, var_line_limit, STR(session->buffer));
	if (msg_verbose)
	    msg_info("< %s: %.100s", session->namaddr, STR(session->buffer));

	/*
	 * Defend against a denial of service attack by limiting the amount
	 * of multi-line text that we are willing to store.
	 */
	if (LEN(rdata.str_buf) < var_line_limit) {
	    if (LEN(rdata.str_buf))
		VSTRING_ADDCH(rdata.str_buf, '\n');
	    vstring_strcat(rdata.str_buf, STR(session->buffer));
	    smtp_chat_append(session, "In:  ", STR(session->buffer));
	}

	/*
	 * Parse into code and text. Ignore unrecognized garbage. This means
	 * that any character except space (or end of line) will have the
	 * same effect as the '-' line continuation character.
	 */
	for (cp = STR(session->buffer); *cp && ISDIGIT(*cp); cp++)
	     /* void */ ;
	if ((three_digs = (cp - STR(session->buffer) == 3)) != 0) {
	    if (*cp == '-')
		continue;
	    if (*cp == ' ' || *cp == 0)
		break;
	}
	session->error_mask |= MAIL_ERROR_PROTOCOL;
    }

    /*
     * Extract RFC 821 reply code and RFC 2034 detail. Use a default detail
     * code if none was given.
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
	rdata.code = atoi(STR(session->buffer));
	if (strchr("245", STR(session->buffer)[0]) != 0) {
	    for (cp = STR(session->buffer) + 4; *cp == ' '; cp++)
		 /* void */ ;
	    if ((len = dsn_valid(cp)) > 0 && *cp == *STR(session->buffer)) {
		vstring_strncpy(rdata.dsn_buf, cp, len);
	    } else {
		vstring_strcpy(rdata.dsn_buf, "0.0.0");
		STR(rdata.dsn_buf)[0] = STR(session->buffer)[0];
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

static void print_line(const char *str, ssize_t len, ssize_t indent, char *context)
{
    VSTREAM *notice = (VSTREAM *) context;

    post_mail_fprintf(notice, " %*s%.*s", (int) indent, "", (int) len, str);
}

/* smtp_chat_notify - notify postmaster */

void    smtp_chat_notify(SMTP_SESSION *session)
{
    char   *myname = "smtp_chat_notify";
    VSTREAM *notice;
    char  **cpp;

    /*
     * Sanity checks.
     */
    if (session->history == 0)
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
    post_mail_fprintf(notice, "Subject: %s SMTP client: errors from %s",
		      var_mail_name, session->namaddr);
    post_mail_fputs(notice, "");
    post_mail_fprintf(notice, "Unexpected response from %s.", session->namaddr);
    post_mail_fputs(notice, "");
    post_mail_fputs(notice, "Transcript of session follows.");
    post_mail_fputs(notice, "");
    argv_terminate(session->history);
    for (cpp = session->history->argv; *cpp; cpp++)
	line_wrap(printable(*cpp, '?'), LENGTH, INDENT, print_line,
		  (char *) notice);
    (void) post_mail_fclose(notice);
}
