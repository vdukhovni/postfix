/*++
/* NAME
/*	bounce_log 3
/* SUMMARY
/*	bounce file API
/* SYNOPSIS
/*	#include <bounce_log.h>
/*
/*	typedef struct {
/* .in +4
/*	    /* Non-null: rcpt.address, dsn.{status,action,text} */
/*	    RECIPIENT	rcpt;
/*	    DSN		dsn;
/* .in -4
/*	} BOUNCE_LOG;
/*
/*	BOUNCE_LOG *bounce_log_open(queue, id, flags, mode)
/*	const char *queue;
/*	const char *id;
/*	int	flags;
/*	int	mode;
/*
/*	BOUNCE_LOG *bounce_log_read(bp)
/*	BOUNCE_LOG *bp;
/*
/*	void	bounce_log_rewind(bp)
/*	BOUNCE_LOG *bp;
/*
/*	BOUNCE_LOG *bounce_log_forge(rcpt, dsn)
/*	RECIPIENT *rcpt;
/*	DSN	*dsn;
/*
/*	void	bounce_log_close(bp)
/*	BOUNCE_LOG *bp;
/* DESCRIPTION
/*	This module implements a bounce/defer logfile API. Information
/*	is sanitized for control and non-ASCII characters. Fields not
/*	present in input are represented by empty strings.
/*
/*	bounce_log_open() opens the named bounce or defer logfile
/*	and returns a handle that must be used for further access.
/*	The result is a null pointer if the file cannot be opened.
/*	The caller is expected to inspect the errno code and deal
/*	with the problem.
/*
/*	bounce_log_read() reads the next record from the bounce or defer
/*	logfile (skipping over and warning about malformed data)
/*	and breaks out the recipient address, the recipient status
/*	and the text that explains why the recipient was undeliverable.
/*	bounce_log_read() returns a null pointer when no recipient was read,
/*	otherwise it returns its argument.
/*
/*	bounce_log_rewind() is a helper that seeks to the first recipient
/*	in an open bounce or defer logfile (skipping over recipients that
/*	are marked as done). The result is 0 in case of success, -1 in case
/*	of problems.
/*
/*	bounce_log_forge() forges one recipient status record
/*	without actually accessing a logfile. No copy is made
/*	of strings with recipient or status information.
/*	The result cannot be used for any logfile access operation
/*	and must be disposed of by passing it to bounce_log_close().
/*
/*	bounce_log_close() closes an open bounce or defer logfile and
/*	releases memory for the specified handle. The result is non-zero
/*	in case of I/O errors.
/*
/*	Arguments:
/* .IP queue
/*	The bounce or defer queue name.
/* .IP id
/*	The message queue id of bounce or defer logfile. This
/*	file has the same name as the original message file.
/* .IP flags
/*	File open flags, as with open(2).
/* .IP more
/*	File permissions, as with open(2).
/* .PP
/*	Recipient results:
/* .IP address
/*	The final recipient address in RFC 822 external form, or <>
/*	in case of the null recipient address.
/* .IP dsn_orcpt
/*	Null pointer or DSN original recipient.
/* .IP orig_addr
/*      Null pointer or the original recipient address in RFC 822
/*	external form.
/* .IP dsn_notify
/*	Zero or DSN notify flags.
/* .IP offset
/*	Zero or queue file offset of recipient record.
/* .PP
/*	Delivery status results:
/* .IP dsn_status
/*	RFC 3463-compatible enhanced status code (digit.digits.digits).
/* .IP dsn_action
/*	"delivered", "failed", "delayed" and so on.
/* .IP reason
/*	The text that explains why the recipient was undeliverable.
/* .IP dsn_dtype
/*	Null pointer or RFC 3464-compatible diagnostic type.
/* .IP dsn_dtext
/*	Null pointer or RFC 3464-compatible diagnostic text.
/* .IP dsn_mtype
/*	Null pointer or RFC 3464-compatible remote MTA type.
/* .IP dsn_mname
/*	Null pointer or RFC 3464-compatible remote MTA name.
/* .PP
/*	Other fields will be added as the code evolves.
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
#include <unistd.h>
#include <stdlib.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <vstream.h>
#include <vstring.h>
#include <vstring_vstream.h>
#include <stringops.h>

/* Global library. */

#include <mail_params.h>
#include <mail_proto.h>
#include <mail_queue.h>
#include <dsn_mask.h>
#include <recipient_list.h>
#include <dsn.h>
#include <bounce_log.h>

/* Application-specific. */

#define STR(x)		vstring_str(x)

/* bounce_log_open - open bounce read stream */

BOUNCE_LOG *bounce_log_open(const char *queue_name, const char *queue_id,
			            int flags, int mode)
{
    BOUNCE_LOG *bp;
    VSTREAM *fp;

#define STREQ(x,y)	(strcmp((x),(y)) == 0)

    /*
     * TODO: peek at the first byte to see if this is an old-style log
     * (<recipient>: text) or a new-style extensible log with multiple
     * attributes per recipient. That would not help during a transition from
     * old to new style, where one can expect to find mixed format files.
     * 
     * Kluge up default DSN status and action for old-style logfiles.
     */
    if ((fp = mail_queue_open(queue_name, queue_id, flags, mode)) == 0) {
	return (0);
    } else {
	bp = (BOUNCE_LOG *) mymalloc(sizeof(*bp));
	bp->fp = fp;
	bp->buf = vstring_alloc(100);
	bp->rcpt_buf = rcpb_create();
	bp->dsn_buf = dsb_create();
	if (STREQ(queue_name, MAIL_QUEUE_DEFER)) {
	    bp->compat_status = mystrdup("4.0.0");
	    bp->compat_action = mystrdup("delayed");
	} else {
	    bp->compat_status = mystrdup("5.0.0");
	    bp->compat_action = mystrdup("failed");
	}
	return (bp);
    }
}

/* bounce_log_read - read one record from bounce log file */

BOUNCE_LOG *bounce_log_read(BOUNCE_LOG *bp)
{
    char   *recipient;
    char   *text;
    char   *cp;
    int     state;

    /*
     * Our trivial logfile parser state machine.
     */
#define START	0				/* still searching */
#define FOUND	1				/* in logfile entry */

    /*
     * Initialize. See also DSN_FROM_DSN_BUF() and bounce_log_forge() for
     * null and non-null fields.
     */
    state = START;
    bp->rcpt.address = "(recipient address unavailable)";
    bp->dsn.status = bp->compat_status;
    bp->dsn.action = bp->compat_action;
    bp->dsn.reason = "(description unavailable)";

    bp->rcpt.orig_addr = 0;
    bp->rcpt.dsn_orcpt = 0;
    bp->rcpt.dsn_notify = 0;
    bp->rcpt.offset = 0;

    bp->dsn.dtype = 0;
    bp->dsn.dtext = 0;
    bp->dsn.mtype = 0;
    bp->dsn.mname = 0;

    /*
     * Support mixed logfile formats to make migration easier. The same file
     * can start with old-style records and end with new-style records. With
     * backwards compatibility, we even have old format followed by new
     * format within the same logfile entry!
     */
    while ((vstring_get_nonl(bp->buf, bp->fp) != VSTREAM_EOF)) {

	/*
	 * Logfile entries are separated by blank lines. Even the old ad-hoc
	 * logfile format has a blank line after the last record. This means
	 * we can safely use blank lines to detect the start and end of
	 * logfile entries.
	 */
	if (STR(bp->buf)[0] == 0) {
	    if (state == FOUND)
		return (bp);
	    state = START;
	    continue;
	}

	/*
	 * Sanitize. XXX This needs to be done more carefully with new-style
	 * logfile entries.
	 */
	cp = printable(STR(bp->buf), '?');

	if (state == START)
	    state = FOUND;

	/*
	 * New style logfile entries are in "name = value" format.
	 */
	if (ISALNUM(*cp)) {
	    const char *err;
	    char   *name;
	    char   *value;
	    long    offset;
	    long    notify;

	    /*
	     * Split into name and value.
	     */
	    if ((err = split_nameval(cp, &name, &value)) != 0) {
		msg_warn("%s: malformed record: %s", VSTREAM_PATH(bp->fp), err);
		continue;
	    }

	    /*
	     * Save attribute value.
	     */
	    if (STREQ(name, MAIL_ATTR_RECIP)) {
		bp->rcpt.address =
		    STR(vstring_strcpy(bp->rcpt_buf->address, *value ?
				       value : "(MAILER-DAEMON)"));
	    } else if (STREQ(name, MAIL_ATTR_ORCPT)) {
		bp->rcpt.orig_addr =
		    STR(vstring_strcpy(bp->rcpt_buf->orig_addr, *value ?
				       value : "(MAILER-DAEMON)"));
	    } else if (STREQ(name, MAIL_ATTR_DSN_ORCPT)) {
		if (*value)
		    bp->rcpt.dsn_orcpt =
			STR(vstring_strcpy(bp->rcpt_buf->dsn_orcpt, value));
	    } else if (STREQ(name, MAIL_ATTR_DSN_NOTIFY)) {
		if ((notify = atoi(value)) > 0 && DSN_NOTIFY_OK(notify))
		    bp->rcpt.dsn_notify = notify;
	    } else if (STREQ(name, MAIL_ATTR_OFFSET)) {
		if ((offset = atol(value)) > 0)
		    bp->rcpt.offset = offset;
	    } else if (STREQ(name, MAIL_ATTR_DSN_STATUS)) {
		if (*value)
		    bp->dsn.status =
			STR(vstring_strcpy(bp->dsn_buf->status, value));
	    } else if (STREQ(name, MAIL_ATTR_DSN_ACTION)) {
		if (*value)
		    bp->dsn.action =
			STR(vstring_strcpy(bp->dsn_buf->action, value));
	    } else if (STREQ(name, MAIL_ATTR_DSN_DTYPE)) {
		if (*value)
		    bp->dsn.dtype =
			STR(vstring_strcpy(bp->dsn_buf->dtype, value));
	    } else if (STREQ(name, MAIL_ATTR_DSN_DTEXT)) {
		if (*value)
		    bp->dsn.dtext =
			STR(vstring_strcpy(bp->dsn_buf->dtext, value));
	    } else if (STREQ(name, MAIL_ATTR_DSN_MTYPE)) {
		if (*value)
		    bp->dsn.mtype =
			STR(vstring_strcpy(bp->dsn_buf->mtype, value));
	    } else if (STREQ(name, MAIL_ATTR_DSN_MNAME)) {
		if (*value)
		    bp->dsn.mname =
			STR(vstring_strcpy(bp->dsn_buf->mname, value));
	    } else if (STREQ(name, MAIL_ATTR_WHY)) {
		if (*value)
		    bp->dsn.reason =
			STR(vstring_strcpy(bp->dsn_buf->reason, value));
	    } else {
		msg_warn("%s: unknown attribute name: %s, ignored",
			 VSTREAM_PATH(bp->fp), name);
	    }
	    continue;
	}

	/*
	 * Old-style logfile record. Find the recipient address.
	 */
	if (*cp != '<') {
	    msg_warn("%s: malformed record: %.30s...",
		     VSTREAM_PATH(bp->fp), cp);
	    continue;
	}
	recipient = cp + 1;
	if ((cp = strstr(recipient, ">: ")) == 0) {
	    msg_warn("%s: malformed record: %.30s...",
		     VSTREAM_PATH(bp->fp), cp);
	    continue;
	}
	*cp = 0;
	vstring_strcpy(bp->rcpt_buf->address, *recipient ?
		       recipient : "(MAILER-DAEMON)");
	bp->rcpt.address = STR(bp->rcpt_buf->address);

	/*
	 * Find the text that explains why mail was not deliverable.
	 */
	text = cp + 2;
	while (*text && ISSPACE(*text))
	    text++;
	vstring_strcpy(bp->dsn_buf->reason, text);
	if (*text)
	    bp->dsn.reason = STR(bp->dsn_buf->reason);
    }
    return (0);
}

/* bounce_log_forge - forge one recipient status record */

BOUNCE_LOG *bounce_log_forge(RECIPIENT *rcpt, DSN *dsn)
{
    BOUNCE_LOG *bp;

    /*
     * Create a partial record. No point copying information that doesn't
     * need to be.
     */
    bp = (BOUNCE_LOG *) mymalloc(sizeof(*bp));
    bp->fp = 0;
    bp->buf = 0;
    bp->rcpt_buf = 0;
    bp->dsn_buf = 0;
    bp->compat_status = 0;
    bp->compat_action = 0;

    bp->rcpt = *rcpt;
    bp->dsn = *dsn;

    /*
     * Finalize. See also DSN_FROM_DSN_BUF() and bounce_log_read() for null
     * and non-null fields.
     */
#define NULL_OR_EMPTY(s) ((s) == 0 || *(s) == 0)
#define EMPTY_STRING(s)	((s) != 0 && *(s) == 0)

    /*
     * Replace null pointers and empty strings by place holders.
     */
    if (bp->rcpt.address == 0)
	bp->rcpt.address = "(recipient address unavailable)";
    if (NULL_OR_EMPTY(bp->dsn.status))
	bp->dsn.status = "(unavailable)";
    if (NULL_OR_EMPTY(bp->dsn.action))
	bp->dsn.action = "(unavailable)";
    if (NULL_OR_EMPTY(bp->dsn.reason))
	bp->dsn.reason = "(description unavailable)";

    /*
     * Replace empty strings by null pointers.
     */
    if (EMPTY_STRING(bp->rcpt.orig_addr))
	bp->rcpt.orig_addr = 0;
    if (EMPTY_STRING(bp->rcpt.dsn_orcpt))
	bp->rcpt.dsn_orcpt = 0;

    if (EMPTY_STRING(bp->dsn.dtype))
	bp->dsn.dtype = 0;
    if (EMPTY_STRING(bp->dsn.dtext))
	bp->dsn.dtext = 0;
    if (EMPTY_STRING(bp->dsn.mtype))
	bp->dsn.mtype = 0;
    if (EMPTY_STRING(bp->dsn.mname))
	bp->dsn.mname = 0;
    return (bp);
}

/* bounce_log_close - close bounce reader stream */

int     bounce_log_close(BOUNCE_LOG *bp)
{
    int     ret;

    if (bp->fp)
	ret = vstream_fclose(bp->fp);
    else
	ret = 0;
    if (bp->buf)
	vstring_free(bp->buf);
    if (bp->rcpt_buf)
	rcpb_free(bp->rcpt_buf);
    if (bp->dsn_buf)
	dsb_free(bp->dsn_buf);
    if (bp->compat_status)
	myfree(bp->compat_status);
    if (bp->compat_action)
	myfree(bp->compat_action);
    myfree((char *) bp);

    return (ret);
}
