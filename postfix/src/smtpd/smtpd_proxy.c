/*++
/* NAME
/*	smtpd_proto 3
/* SUMMARY
/*	SMTP server pass-through proxy client
/* SYNOPSIS
/*	#include <smtpd.h>
/*	#include <smtpd_proxy.h>
/*
/*	typedef struct {
/* .in +4
/*		/* other fields... */
/*		VSTREAM *proxy;		/* connection to SMTP proxy */
/*		VSTRING *proxy_reply;	/* last SMTP proxy response */
/*		/* other fields... */
/* .in -4
/*	} SMTPD_STATE;
/*
/*	int	smtpd_proxy_open(state, service, timeout, ehlo_name, mail_from)
/*	SMTPD_STATE *state;
/*	const char *service;
/*	int	timeout;
/*	const char *ehlo_name;
/*	const char *mail_from;
/*
/*	int	smtpd_proxy_cmd(state, expect, format, ...)
/*	SMTPD_STATE *state;
/*	int	expect;
/*	cont char *format;
/*
/*	void	smtpd_proxy_open(state)
/*	SMTPD_STATE *state;
/* RECORD-LEVEL ROUTINES
/*	int	smtpd_proxy_rec_put(stream, rec_type, data, len)
/*	VSTREAM *stream;
/*	int	rec_type;
/*	const char *data;
/*	int	len;
/*
/*	int	smtpd_proxy_rec_fprintf(stream, rec_type, format, ...)
/*	VSTREAM *stream;
/*	int	rec_type;
/*	cont char *format;
/* DESCRIPTION
/*	The functions in this module implement a pass-through proxy
/*	client.
/*
/*	In order to minimize the intrusiveness of pass-through proxying, 1) the
/*	proxy server must support the same MAIL FROM/RCPT syntax that Postfix
/*	supports, 2) the record-level routines for message content proxying
/*	have the same interface as the routines that are used for non-proxied
/*	mail.
/*
/*	smtpd_proxy_open() should be called after receiving the MAIL FROM
/*	command. It connects to the proxy service, sends EHLO, sends the
/*	MAIL FROM command, and receives the reply. A non-zero result means
/*	trouble: either the proxy is unavailable, or it did not send the
/*	expected reply.
/*	All results are reported via the state->proxy_reply field in a form
/*	that can be sent to the SMTP client. In case of error, the
/*	state->error_mask and state->err fields are updated.
/*	A state->proxy_reply field is created automatically; this field
/*	persists beyond the end of a proxy session.
/*
/*	smtpd_proxy_cmd() formats and sends the specified command to the
/*	proxy server, and receives the proxy server reply. A non-zero result
/*	means trouble: either the proxy is unavailable, or it did not send the
/*      expected reply.
/*	All results are reported via the state->proxy_reply field in a form
/*	that can be sent to the SMTP client. In case of error, the
/*	state->error_mask and state->err fields are updated.
/*
/*	smtpd_proxy_close() disconnects from a proxy server and resets
/*	the state->proxy field. The last proxy server reply or error
/*	description remains available via state->proxy-reply.
/*
/*	smtpd_proxy_rec_put() is a rec_put() clone that passes arbitrary
/*	message content records to the proxy server. The data is expected
/*	to be in SMTP dot-escaped form. All errors are reported as a
/*	REC_TYPE_ERROR result value.
/*
/*	smtpd_proxy_rec_fprintf() is a rec_fprintf() clone that formats
/*	message content and sends it to the proxy server. Leading dots are
/*	not escaped. All errors are reported as a REC_TYPE_ERROR result
/*	value.
/*
/* Arguments:
/* .IP server
/*	The SMTP proxy server host:port. The host or host: part is optional.
/* .IP timeout
/*	Time limit for connecting to the proxy server and for
/*	sending and receiving proxy server commands and replies.
/* .IP ehlo_name
/*	The EHLO Hostname that will be sent to the proxy server.
/* .IP mail_from
/*	The MAIL FROM command.
/* .IP state
/*	SMTP server state.
/* .IP expect
/*	Expected proxy server reply status code range. A warning is logged
/*	when an unexpected reply is received. Specify one of the following:
/* .RS
/* .IP SMTPD_PROX_STAT_ANY
/*	The caller has no expectation. Do not warn for unexpected replies.
/* .IP SMTPD_PROX_STAT_OK
/*	The caller expects a reply in the 200 range.
/* .IP SMTPD_PROX_STAT_MORE
/*	The caller expects a reply in the 300 range.
/* .IP SMTPD_PROX_STAT_DEFER
/* .IP SMTPD_PROX_STAT_FAIL
/*	The caller perversely expects a reply in the 400 and 500 range,
/*	respectively.
/* .RE
/* .IP format
/*	A format string.
/* .IP stream
/*	Connection to proxy server.
/* .IP data
/*	Pointer to the content of one message content record.
/* .IP len
/*	The length of a message content record.
/* SEE ALSO
/*	smtpd(8) Postfix smtp server
/* DIAGNOSTICS
/*	Fatal errors: memory allocation problem.
/*
/*	Warnings: unexpected response from proxy server, unable
/*	to connect to proxy server, proxy server read/write error.
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
#include <ctype.h>

/* Utility library. */

#include <msg.h>
#include <vstream.h>
#include <vstring.h>
#include <stringops.h>
#include <connect.h>

/* Global library. */

#include <mail_error.h>
#include <smtp_stream.h>
#include <cleanup_user.h>
#include <mail_params.h>
#include <rec_type.h>

/* Application-specific. */

#include <smtpd.h>
#include <smtpd_proxy.h>

 /*
  * SLMs.
  */
#define STR(x)	vstring_str(x)
#define LEN(x)	VSTRING_LEN(x)

/* smtpd_proxy_open - open proxy connection after MAIL FROM */

int     smtpd_proxy_open(SMTPD_STATE *state, const char *service,
			         int timeout, const char *ehlo_name,
			         const char *mail_from)
{
    int     fd;

    /*
     * This buffer persists beyond the end of a proxy session so we can
     * inspect the last command's reply.
     */
    if (state->proxy_buffer == 0)
	state->proxy_buffer = vstring_alloc(10);

    /*
     * Connect to proxy.
     */
    if ((fd = inet_connect(service, BLOCKING, timeout)) < 0) {
	state->error_mask |= MAIL_ERROR_SOFTWARE;
	state->err |= CLEANUP_STAT_PROXY;
	msg_warn("connect to proxy service %s: %m", service);
	vstring_sprintf(state->proxy_buffer,
			"451 Error: queue file write error");
	return (-1);
    }
    state->proxy = vstream_fdopen(fd, O_RDWR);
    vstream_control(state->proxy, VSTREAM_CTL_PATH, service, VSTREAM_CTL_END);
    smtp_timeout_setup(state->proxy, timeout);

    /*
     * Get server greeting banner.
     * 
     * XXX If this fails then we should not send the initial reply when the
     * client expects the MAIL FROM reply.
     */
    if (smtpd_proxy_cmd(state, SMTPD_PROX_STAT_OK, (char *) 0) != 0) {
	vstring_sprintf(state->proxy_buffer,
			"451 Error: queue file write error");
	smtpd_proxy_close(state);
	return (-1);
    }

    /*
     * Send our own EHLO command.
     * 
     * XXX If this fails then we should not send the EHLO reply when the client
     * expects the MAIL FROM reply.
     */
    if (smtpd_proxy_cmd(state, SMTPD_PROX_STAT_OK, "EHLO %s", ehlo_name) != 0) {
	vstring_sprintf(state->proxy_buffer,
			"451 Error: queue file write error");
	smtpd_proxy_close(state);
	return (-1);
    }

    /*
     * Pass-through the client's MAIL FROM command.
     */
    if (smtpd_proxy_cmd(state, SMTPD_PROX_STAT_OK, "%s", mail_from) != 0) {
	smtpd_proxy_close(state);
	return (-1);
    }
    return (0);
}

/* smtpd_proxy_comms_error - report proxy communication error */

static int smtpd_proxy_comms_error(VSTREAM *stream, int err)
{
    switch (err) {
	case SMTP_ERR_EOF:
	msg_warn("lost connection with proxy %s", VSTREAM_PATH(stream));
	return (err);
    case SMTP_ERR_TIME:
	msg_warn("timeout talking to proxy %s", VSTREAM_PATH(stream));
	return (err);
    default:
	msg_panic("smtpd_proxy_comms_error: unknown proxy %s stream error %d",
		  VSTREAM_PATH(stream), err);
    }
}

/* smtpd_proxy_cmd_error - report unexpected proxy reply */

static void smtpd_proxy_cmd_error(SMTPD_STATE *state, const char *fmt,
				          va_list ap)
{
    VSTRING *buf;

    /*
     * The command can be omitted at the start of an SMTP session. A null
     * format string is not documented as part of the official interface
     * because it is used only internally to this module.
     */
    buf = vstring_alloc(100);
    vstring_vsprintf(buf, fmt && *fmt ? fmt : "connection request", ap);
    msg_warn("proxy %s rejected \"%s\": \"%s\"", VSTREAM_PATH(state->proxy),
	     STR(buf), STR(state->proxy_buffer));
    vstring_free(buf);
}

/* smtpd_proxy_cmd - send command to proxy, receive reply */

int     smtpd_proxy_cmd(SMTPD_STATE *state, int expect, const char *fmt,...)
{
    va_list ap;
    char   *cp;
    int     last_char;
    int     err = 0;

    /*
     * Errors first. Be prepared for delayed errors from the DATA phase.
     */
    if (vstream_ftimeout(state->proxy)
	|| vstream_ferror(state->proxy)
	|| vstream_feof(state->proxy)
	|| ((err = vstream_setjmp(state->proxy) != 0)
	    && smtpd_proxy_comms_error(state->proxy, err))) {
	state->error_mask |= MAIL_ERROR_SOFTWARE;
	state->err |= CLEANUP_STAT_PROXY;
	vstring_sprintf(state->proxy_buffer,
			"451 Error: queue file write error");
	return (-1);
    }

    /*
     * The command can be omitted at the start of an SMTP session. A null
     * format string is not documented as part of the official interface
     * because it is used only internally to this module.
     */
    if (fmt && *fmt) {

	/*
	 * Format the command.
	 */
	va_start(ap, fmt);
	vstring_vsprintf(state->proxy_buffer, fmt, ap);
	va_end(ap);

	/*
	 * Optionally log the command first, so that we can see in the log
	 * what the program is trying to do.
	 */
	if (msg_verbose)
	    msg_info("> %s: %s", VSTREAM_PATH(state->proxy),
		     STR(state->proxy_buffer));

	/*
	 * Send the command to the proxy server. Since we're going to read a
	 * reply immediately, there is no need to flush buffers.
	 */
	smtp_fputs(STR(state->proxy_buffer), LEN(state->proxy_buffer),
		   state->proxy);
    }

    /*
     * Censor out non-printable characters in server responses and keep the
     * last line of multi-line responses.
     */
    for (;;) {
	last_char = smtp_get(state->proxy_buffer, state->proxy, var_line_limit);
	printable(STR(state->proxy_buffer), '?');
	if (last_char != '\n')
	    msg_warn("%s: response longer than %d: %.30s...",
		     VSTREAM_PATH(state->proxy), var_line_limit,
		     STR(state->proxy_buffer));
	if (msg_verbose)
	    msg_info("< %s: %s", VSTREAM_PATH(state->proxy),
		     STR(state->proxy_buffer));

	/*
	 * Parse the response into code and text. Ignore unrecognized
	 * garbage. This means that any character except space (or end of
	 * line) will have the same effect as the '-' line continuation
	 * character.
	 */
	for (cp = STR(state->proxy_buffer); *cp && ISDIGIT(*cp); cp++)
	     /* void */ ;
	if (cp - STR(state->proxy_buffer) == 3) {
	    if (*cp == '-')
		continue;
	    if (*cp == ' ' || *cp == 0)
		break;
	}
	msg_warn("received garbage from proxy %s: %.100s",
		 VSTREAM_PATH(state->proxy), STR(state->proxy_buffer));
    }

    /*
     * Log a warning in case the proxy does not send the expected response.
     * Silently accept any response when the client expressed no expectation.
     */
    if (expect != SMTPD_PROX_STAT_ANY
	&& expect != (STR(state->proxy_buffer)[0] - '0')) {
	va_start(ap, fmt);
	smtpd_proxy_cmd_error(state, fmt, ap);
	va_end(ap);
	return (-1);
    } else {
	return (0);
    }
}

/* smtpd_proxy_rec_put - send message content, rec_put() clone */

int     smtpd_proxy_rec_put(VSTREAM *stream, int rec_type,
			            const char *data, int len)
{
    int     err;

    /*
     * Errors first.
     */
    if (vstream_ftimeout(stream) || vstream_ferror(stream)
	|| vstream_feof(stream))
	return (REC_TYPE_ERROR);
    if ((err = vstream_setjmp(stream)) != 0)
	return (smtpd_proxy_comms_error(stream, err), REC_TYPE_ERROR);

    /*
     * Send one content record. Errors and results must be as with rec_put().
     */
    if (rec_type == REC_TYPE_NORM)
	smtp_fputs(data, len, stream);
    else
	smtp_fwrite(data, len, stream);
    return (rec_type);
}

/* smtpd_proxy_rec_fprintf - send message content, rec_fprintf() clone */

int     smtpd_proxy_rec_fprintf(VSTREAM *stream, int rec_type,
				        const char *fmt,...)
{
    va_list ap;
    int     err;

    /*
     * Errors first.
     */
    if (vstream_ftimeout(stream) || vstream_ferror(stream)
	|| vstream_feof(stream))
	return (REC_TYPE_ERROR);
    if ((err = vstream_setjmp(stream)) != 0)
	return (smtpd_proxy_comms_error(stream, err), REC_TYPE_ERROR);

    /*
     * Send one content record. Errors and results must be as with
     * rec_fprintf().
     */
    va_start(ap, fmt);
    if (rec_type != REC_TYPE_NORM)
	msg_panic("smtpd_proxy_rec_fprintf: need REC_TYPE_NORM");
    smtp_vprintf(stream, fmt, ap);
    va_end(ap);
    return (rec_type);
}

/* smtpd_proxy_close - close proxy connection */

void    smtpd_proxy_close(SMTPD_STATE *state)
{
    (void) vstream_fclose(state->proxy);
    state->proxy = 0;
}
