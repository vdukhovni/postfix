/*++
/* NAME
/*	bounce_flush_service 3
/* SUMMARY
/*	send non-delivery report to sender, server side
/* SYNOPSIS
/*	#include "bounce_service.h"
/*
/*	int     bounce_flush_service(queue_name, queue_id, sender)
/*	char	*queue_name;
/*	char	*queue_id;
/*	char	*sender;
/* DESCRIPTION
/*	This module implements the server side of the bounce_flush()
/*	(send bounce message) request.
/*
/*	When a message bounces, a full copy is sent to the originator,
/*	and a copy of the diagnostics with message headers is sent to
/*	the postmaster.  The result is non-zero when the operation
/*	should be tried again.
/*
/*	When a single bounce is sent, the sender address is the empty
/*	address.  When a double bounce is sent, the sender is taken
/*	from the configuration parameter \fIdouble_bounce_sender\fR.
/* DIAGNOSTICS
/*	Fatal error: error opening existing file. Warnings: corrupt
/*	message file. A corrupt message is saved to the "corrupt"
/*	queue for further inspection.
/* BUGS
/* SEE ALSO
/*	bounce(3) basic bounce service client interface
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
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

/* Utility library. */

#include <msg.h>
#include <vstring.h>
#include <vstream.h>
#include <vstring_vstream.h>
#include <mymalloc.h>
#include <stringops.h>
#include <events.h>
#include <line_wrap.h>
#include <name_mask.h>

/* Global library. */

#include <mail_queue.h>
#include <mail_proto.h>
#include <quote_822_local.h>
#include <mail_params.h>
#include <canon_addr.h>
#include <is_header.h>
#include <record.h>
#include <rec_type.h>
#include <config.h>
#include <post_mail.h>
#include <mail_addr.h>
#include <mark_corrupt.h>
#include <mail_error.h>

/* Application-specific. */

#include "bounce_service.h"

#define STR vstring_str

/* bounce_header - generate bounce message header */

static int bounce_header(VSTREAM *bounce, VSTRING *buf, char *dest)
{

    /*
     * Print a minimal bounce header. The cleanup service will add other
     * headers and will make all addresses fully qualified.
     */
    post_mail_fprintf(bounce, "From: %s (Mail Delivery System)",
		      MAIL_ADDR_MAIL_DAEMON);
    post_mail_fprintf(bounce, *dest == 0 ?
		      "Subject: Postmaster Copy: Undelivered Mail" :
		      "Subject: Undelivered Mail Returned to Sender");
    quote_822_local(buf, *dest == 0 ? mail_addr_postmaster() : dest);
    post_mail_fprintf(bounce, "To: %s", STR(buf));
    post_mail_fputs(bounce, "");
    return (vstream_ferror(bounce));
}

/* bounce_boilerplate - generate boiler-plate text */

static int bounce_boilerplate(VSTREAM *bounce, VSTRING *buf)
{

    /*
     * Print the message body with the problem report. XXX For now, we use a
     * fixed bounce template. We could use a site-specific parametrized
     * template with ${name} macros and we could do wonderful things such as
     * word wrapping to make the text look nicer. No matter how hard we would
     * try, receiving bounced mail will always suck.
     */
    post_mail_fprintf(bounce, "This is the %s program at host %s.",
		      var_mail_name, var_myhostname);
    post_mail_fputs(bounce, "");
    post_mail_fprintf(bounce,
	       "I'm sorry to have to inform you that the message returned");
    post_mail_fprintf(bounce,
	       "below could not be delivered to one or more destinations.");
    post_mail_fputs(bounce, "");
    post_mail_fprintf(bounce,
		      "For further assistance, please contact <%s>",
		      STR(canon_addr_external(buf, MAIL_ADDR_POSTMASTER)));
    post_mail_fputs(bounce, "");
    post_mail_fprintf(bounce,
	       "If you do so, please include this problem report. You can");
    post_mail_fprintf(bounce,
		   "delete your own text from the message returned below.");
    post_mail_fputs(bounce, "");
    post_mail_fprintf(bounce, "\t\t\tThe %s program", var_mail_name);
    return (vstream_ferror(bounce));
}

/* bounce_print - line_wrap callback */

static void bounce_print(const char *str, int len, int indent, char *context)
{
    VSTREAM *bounce = (VSTREAM *) context;

    post_mail_fprintf(bounce, "%*s%.*s", indent, "", len, str);
}

/* bounce_diagnostics - send bounce log report */

static int bounce_diagnostics(char *service, VSTREAM *bounce, VSTRING *buf, char *queue_id)
{
    VSTREAM *log;

    /*
     * If the bounce log cannot be found, do not raise a fatal run-time
     * error. There is nothing we can do about the error, and all we are
     * doing is to inform the sender of a delivery problem, Bouncing a
     * message does not have to be a perfect job. But if the system IS
     * running out of resources, raise a fatal run-time error and force a
     * backoff.
     */
    if ((log = mail_queue_open(service, queue_id, O_RDONLY, 0)) == 0) {
	if (errno != ENOENT)
	    msg_fatal("open %s %s: %m", service, queue_id);
	post_mail_fputs(bounce, "");
	post_mail_fputs(bounce, "\t--- Delivery error report unavailable ---");
	post_mail_fputs(bounce, "");
    }

    /*
     * Append a copy of the delivery error log. Again, we're doing a best
     * effort, so there is no point raising a fatal run-time error in case of
     * a logfile read error. Wrap long lines, filter non-printable
     * characters, and prepend one blank, so this data can safely be piped
     * into other programs.
     */
    else {

#define LENGTH	79
#define INDENT	4
	post_mail_fputs(bounce, "");
	post_mail_fputs(bounce, "\t--- Delivery error report follows ---");
	post_mail_fputs(bounce, "");
	while (vstream_ferror(bounce) == 0 && vstring_fgets_nonl(buf, log)) {
	    printable(STR(buf), '_');
	    line_wrap(STR(buf), LENGTH, INDENT, bounce_print, (char *) bounce);
	    if (vstream_ferror(bounce) != 0)
		break;
	}
	if (vstream_fclose(log))
	    msg_warn("read bounce log %s: %m", queue_id);
    }
    return (vstream_ferror(bounce));
}

/* bounce_original - send a copy of the original to the victim */

static int bounce_original(char *service, VSTREAM *bounce, VSTRING *buf,
		         char *queue_name, char *queue_id, int headers_only)
{
    int     status = 0;
    VSTREAM *src;
    int     rec_type;
    int     bounce_length;

    /*
     * If the original message cannot be found, do not raise a run-time
     * error. There is nothing we can do about the error, and all we are
     * doing is to inform the sender of a delivery problem. Bouncing a
     * message does not have to be a perfect job. But if the system IS
     * running out of resources, raise a fatal run-time error and force a
     * backoff.
     */
    if ((src = mail_queue_open(queue_name, queue_id, O_RDONLY, 0)) == 0) {
	if (errno != ENOENT)
	    msg_fatal("open %s %s: %m", service, queue_id);
	post_mail_fputs(bounce, "\t--- Undelivered message unavailable ---");
	return (vstream_ferror(bounce));
    }

    /*
     * Append a copy of the rejected message.
     */
    post_mail_fputs(bounce, "\t--- Undelivered message follows ---");
    post_mail_fputs(bounce, "");

    /*
     * Skip over the original message envelope records. If the envelope is
     * corrupted just send whatever we can (remember this is a best effort,
     * it does not have to be perfect).
     */
    while ((rec_type = rec_get(src, buf, 0)) > 0)
	if (rec_type == REC_TYPE_MESG)
	    break;

    /*
     * Copy the original message contents. Limit the amount of bounced text
     * so there is a better chance of the bounce making it back. We're doing
     * raw record output here so that we don't throw away binary transparency
     * yet.
     */
#define IS_HEADER(s) (ISSPACE(*(s)) || is_header(s))

    bounce_length = 0;
    while (status == 0 && (rec_type = rec_get(src, buf, 0)) > 0) {
	if (rec_type != REC_TYPE_NORM && rec_type != REC_TYPE_CONT)
	    break;
	if (headers_only && !IS_HEADER(vstring_str(buf)))
	    break;
	if (var_bounce_limit == 0 || bounce_length < var_bounce_limit) {
	    bounce_length += VSTRING_LEN(buf);
	    status = (REC_PUT_BUF(bounce, rec_type, buf) != rec_type);
	}
    }
    if (headers_only == 0 && rec_type != REC_TYPE_XTRA)
	status |= mark_corrupt(src);
    if (vstream_fclose(src))
	msg_warn("read message file %s %s: %m", queue_name, queue_id);
    return (status);
}

/* bounce_flush_service - send a bounce */

int     bounce_flush_service(char *service, char *queue_name,
			             char *queue_id, char *recipient)
{
    VSTRING *buf = vstring_alloc(100);
    const char *double_bounce_addr;
    int     status = 1;
    VSTREAM *bounce;

#define NULL_RECIPIENT		MAIL_ADDR_EMPTY	/* special address */
#define NULL_SENDER		MAIL_ADDR_EMPTY	/* special address */
#define TO_POSTMASTER(addr)	(*(addr) == 0)
#define NULL_CLEANUP_FLAGS	0
#define BOUNCE_HEADERS		1
#define BOUNCE_ALL		0

    /*
     * The choice of sender address depends on recipient address. For a
     * single bounce (typically a non-delivery notification to the message
     * originator), the sender address is the empty string. For a double
     * bounce (typically a failed single bounce, or a postmaster notification
     * that was produced by any of the mail processes) the sender address is
     * defined by the var_double_bounce_sender configuration variable. When a
     * double bounce cannot be delivered, the local delivery agent gives
     * special treatment to the resulting bounce message.
     */
    double_bounce_addr = mail_addr_double_bounce();

    /*
     * Connect to the cleanup service, and request that the cleanup service
     * takes no special actions in case of problems.
     */
    if ((bounce = post_mail_fopen_nowait(TO_POSTMASTER(recipient) ?
					 double_bounce_addr : NULL_SENDER,
					 recipient, NULL_CLEANUP_FLAGS,
					 "BOUNCE")) != 0) {

	/*
	 * Send the bounce message header, some boilerplate text that
	 * pretends that we are a polite mail system, the text with reason
	 * for the bounce, and a copy of the original message.
	 */
	if (bounce_header(bounce, buf, recipient) == 0
	    && bounce_boilerplate(bounce, buf) == 0
	    && bounce_diagnostics(service, bounce, buf, queue_id) == 0)
	    bounce_original(service, bounce, buf, queue_name, queue_id, BOUNCE_ALL);

	/*
	 * Finish the bounce, and retrieve the completion status.
	 */
	status = post_mail_fclose(bounce);
    }

    /*
     * If not sending to the postmaster or double-bounce pseudo accounts,
     * send a postmaster copy as if it is a double bounce, so it will not
     * bounce in case of error. This time, block while waiting for resources
     * to become available. We know they were available just a split second
     * ago.
     */
    if (status == 0 && !TO_POSTMASTER(recipient)
	&& strcasecmp(recipient, double_bounce_addr) != 0
	&& (MAIL_ERROR_BOUNCE & name_mask(mail_error_masks, var_notify_classes))) {

	/*
	 * Send the text with reason for the bounce, and the headers of the
	 * original message. Don't bother sending the boiler-plate text.
	 */
	bounce = post_mail_fopen(double_bounce_addr, NULL_RECIPIENT,
				 NULL_CLEANUP_FLAGS, "BOUNCE");
	if (bounce_header(bounce, buf, NULL_RECIPIENT) == 0
	    && bounce_diagnostics(service, bounce, buf, queue_id) == 0)
	    bounce_original(service, bounce, buf, queue_name, queue_id, BOUNCE_HEADERS);

	/*
	 * Finish the bounce, and update the completion status.
	 */
	status |= post_mail_fclose(bounce);
    }

    /*
     * Examine the completion status. Delete the bounce log file only when
     * the bounce was posted successfully.
     */
    if (status == 0 && mail_queue_remove(service, queue_id) && errno != ENOENT)
	msg_fatal("remove %s %s: %m", service, queue_id);

    /*
     * Cleanup.
     */
    vstring_free(buf);

    return (status);
}
