/*++
/* NAME
/*	cleanup_extracted 3
/* SUMMARY
/*	process extracted segment
/* SYNOPSIS
/*	#include "cleanup.h"
/*
/*	void	cleanup_extracted(state, type, buf, len)
/*	CLEANUP_STATE *state;
/*	int	type;
/*	const char *buf;
/*	int	len;
/* DESCRIPTION
/*	This module processes message records with information extracted
/*	from the initial message envelope or from the message content, or
/*	with recipients that are stored after the message content. It
/*	updates recipient records, and writes extracted information records
/*	to the output.
/*
/*	Arguments:
/* .IP state
/*	Queue file and message processing state. This state is updated
/*	as records are processed and as errors happen.
/* .IP type
/*	Record type.
/* .IP buf
/*	Record content.
/* .IP len
/*	Record content length.
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
#include <string.h>

/* Utility library. */

#include <msg.h>
#include <vstring.h>
#include <vstream.h>
#include <argv.h>
#include <mymalloc.h>
#include <nvtable.h>

/* Global library. */

#include <cleanup_user.h>
#include <record.h>
#include <rec_type.h>
#include <mail_params.h>
#include <ext_prop.h>
#include <mail_proto.h>

/* Application-specific. */

#include "cleanup.h"

#define STR(x)	vstring_str(x)

static void cleanup_extracted_non_rcpt(CLEANUP_STATE *, int, const char *, int);
static void cleanup_extracted_rcpt(CLEANUP_STATE *, int, const char *, int);

/* cleanup_extracted - initialize extracted segment */

void    cleanup_extracted(CLEANUP_STATE *state, int type,
			          const char *buf, int len)
{

    /*
     * Start the extracted segment.
     */
    cleanup_out_string(state, REC_TYPE_XTRA, "");

    /*
     * Pass control to the actual envelope processing routine.
     */
    state->action = cleanup_extracted_non_rcpt;
    cleanup_extracted_non_rcpt(state, type, buf, len);
}

/* cleanup_extracted_non_rcpt - process non-recipient records */

void    cleanup_extracted_non_rcpt(CLEANUP_STATE *state, int type,
				           const char *buf, int len)
{
    const char *encoding;

    if (msg_verbose)
	msg_info("extracted envelope %c %.*s", type, len, buf);

    if (strchr(REC_TYPE_EXTRACT, type) == 0) {
	msg_warn("%s: message rejected: "
		 "unexpected record type %d in extracted envelope",
		 state->queue_id, type);
	state->errs |= CLEANUP_STAT_BAD;
	return;
    }

    /*
     * The following records are taken away from the initial envelope segment
     * and may be overruled by information from header/body_checks; then they
     * are emitted at the start of the extracted envelope segment.
     * 
     * If we encounter these records here, then the message was subjected to
     * "postsuper -r" and we can ignore these records if we already have
     * information from header/body_checks.
     */
    if (type == REC_TYPE_FILT) {
	/* Our own header/body_checks information wins. */
	if (state->filter == 0)
	    state->filter = mystrdup(buf);
	return;
    }
    if (type == REC_TYPE_RDR) {
	/* Our own header/body_checks information wins. */
	if (state->redirect == 0)
	    state->redirect = mystrdup(buf);
	return;
    }

    /*
     * Ignore records that the cleanup server extracts from message headers.
     * These records may appear in "postsuper -r" email.
     */
    if (type == REC_TYPE_RRTO)
	/* Use our own headers extracted return address. */
	return;
    if (type == REC_TYPE_ERTO)
	/* Use our own headers extracted error address. */
	return;
    if (type == REC_TYPE_ATTR)
	/* Use our own headers extracted content encoding. */
	return;

    if (type != REC_TYPE_END && type != REC_TYPE_FROM
	&& type != REC_TYPE_DONE && type != REC_TYPE_ORCP) {
	/* Any other allowed non-recipient record. Pass through. */
	cleanup_out(state, type, buf, len);
	return;
    }

    /*
     * At the end of the non-recipient record section, emit optional
     * information from header/body_checks actions, from the start of the
     * extracted envelope, or from the initial envelope.
     */
    if (state->filter != 0)
	cleanup_out_string(state, REC_TYPE_FILT, state->filter);

    if (state->redirect != 0)
	cleanup_out_string(state, REC_TYPE_RDR, state->redirect);

    if ((encoding = nvtable_find(state->attr, MAIL_ATTR_ENCODING)) != 0)
	cleanup_out_format(state, REC_TYPE_ATTR, "%s=%s",
			   MAIL_ATTR_ENCODING, encoding);

    /*
     * Terminate the non-recipient records with the Return-Receipt-To and
     * Errors-To records. The queue manager relies on this information.
     */
    cleanup_out_string(state, REC_TYPE_RRTO, state->return_receipt ?
		       state->return_receipt : "");

    cleanup_out_string(state, REC_TYPE_ERTO, state->errors_to ?
		       state->errors_to : state->sender);

    /*
     * Pass control to the routine that processes the recipient portion of
     * the extracted segment.
     */
    state->action = cleanup_extracted_rcpt;
    cleanup_extracted_rcpt(state, type, buf, len);
}

/* cleanup_extracted_rcpt - process recipients in extracted segment */

static void cleanup_extracted_rcpt(CLEANUP_STATE *state, int type,
				           const char *buf, int len)
{
    char   *myname = "cleanup_extracted_rcpt";

    if (msg_verbose)
	msg_info("extracted envelope %c %.*s", type, len, buf);

    if (strchr(REC_TYPE_EXTRACT, type) == 0) {
	msg_warn("%s: message rejected: "
		 "unexpected record type %d in extracted envelope",
		 state->queue_id, type);
	state->errs |= CLEANUP_STAT_BAD;
	return;
    }
    if (type == REC_TYPE_RCPT) {
	if (state->orig_rcpt == 0)
	    state->orig_rcpt = mystrdup(buf);
	cleanup_addr_recipient(state, buf);
	myfree(state->orig_rcpt);
	state->orig_rcpt = 0;
	return;
    }
    if (type == REC_TYPE_DONE) {
	if (state->orig_rcpt != 0) {
	    myfree(state->orig_rcpt);
	    state->orig_rcpt = 0;
	}
	return;
    }
    if (state->orig_rcpt != 0) {
	/* REC_TYPE_ORCP must be followed by REC_TYPE_RCPT or REC_TYPE DONE. */
	msg_warn("%s: out-of-order original recipient record <%.200s>",
		 state->queue_id, buf);
	myfree(state->orig_rcpt);
	state->orig_rcpt = 0;
    }
    if (type == REC_TYPE_ORCP) {
	state->orig_rcpt = mystrdup(buf);
	return;
    }
    if (type != REC_TYPE_END) {
	msg_warn("%s: ignoring %s record after extracted envelope recipients",
		 state->queue_id, rec_type_name(type));
	return;
    }

    /*
     * On the way out, add the optional automatic BCC recipient.
     */
    if ((state->flags & CLEANUP_FLAG_BCC_OK)
	&& state->recip != 0 && *var_always_bcc)
	cleanup_addr_bcc(state, var_always_bcc);

    /*
     * Terminate the extracted segment.
     */
    cleanup_out_string(state, REC_TYPE_END, "");
    state->end_seen = 1;

    /*
     * vstream_fseek() would flush the buffer anyway, but the code just reads
     * better if we flush first, because it makes seek error handling more
     * straightforward.
     */
    if (vstream_fflush(state->dst)) {
	if (errno == EFBIG) {
	    msg_warn("%s: queue file size limit exceeded", state->queue_id);
	    state->errs |= CLEANUP_STAT_SIZE;
	} else {
	    msg_warn("%s: write queue file: %m", state->queue_id);
	    state->errs |= CLEANUP_STAT_WRITE;
	}
	return;
    }

    /*
     * Update the preliminary message size and count fields with the actual
     * values.
     */
    if (vstream_fseek(state->dst, 0L, SEEK_SET) < 0)
	msg_fatal("%s: vstream_fseek %s: %m", myname, cleanup_path);
    cleanup_out_format(state, REC_TYPE_SIZE, REC_TYPE_SIZE_FORMAT,
	    (REC_TYPE_SIZE_CAST1) (state->xtra_offset - state->data_offset),
		       (REC_TYPE_SIZE_CAST2) state->data_offset,
		       (REC_TYPE_SIZE_CAST3) state->rcpt_count);
}
