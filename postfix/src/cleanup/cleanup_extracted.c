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
/*	from message content, or with recipients that are stored after the
/*	message content. It updates recipient records, and writes extracted
/*	information records to the output.
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

static void cleanup_extracted_process(CLEANUP_STATE *, int, const char *, int);

/* cleanup_extracted - initialize extracted segment */

void    cleanup_extracted(CLEANUP_STATE *state, int type,
			          const char *buf, int len)
{
    const char *encoding;

    /*
     * Start the extracted segment.
     */
    cleanup_out_string(state, REC_TYPE_XTRA, "");

    /*
     * Put the optional content filter before the mandatory Return-Receipt-To
     * and Errors-To so that the queue manager will pick up the filter name
     * before starting deliveries.
     */
    if (state->filter != 0)
	cleanup_out_string(state, REC_TYPE_FILT, state->filter);

    /*
     * Output the optional redirect target address before the mandatory
     * Return-Receipt-To and Errors-To queue file records so that the queue
     * manager will pick up the address before starting deliveries.
     */
    if (state->redirect != 0)
	cleanup_out_string(state, REC_TYPE_RDR, state->redirect);

    /*
     * Older Postfix versions didn't emit encoding information, so this
     * record can only be optional. Putting this before the mandatory
     * Return-Receipt-To and Errors-To ensures that the queue manager will
     * pick up the content encoding before starting deliveries.
     */
    if ((encoding = nvtable_find(state->attr, MAIL_ATTR_ENCODING)) != 0)
	cleanup_out_format(state, REC_TYPE_ATTR, "%s=%s",
			   MAIL_ATTR_ENCODING, encoding);

    /*
     * Always emit Return-Receipt-To and Errors-To records, and always emit
     * them ahead of extracted recipients, so that the queue manager does not
     * waste lots of time searching through large numbers of recipient
     * addresses.
     */
    cleanup_out_string(state, REC_TYPE_RRTO, state->return_receipt ?
		       state->return_receipt : "");

    cleanup_out_string(state, REC_TYPE_ERTO, state->errors_to ?
		       state->errors_to : state->sender);

    /*
     * Pass control to the routine that processes the extracted segment.
     */
    state->action = cleanup_extracted_process;
    cleanup_extracted_process(state, type, buf, len);
}

/* cleanup_extracted_process - process extracted segment */

static void cleanup_extracted_process(CLEANUP_STATE *state, int type,
				              const char *buf, int len)
{
    char   *myname = "cleanup_extracted_process";

    /*
     * Weird condition for consistency with cleanup_envelope.c
     */
    if (type != REC_TYPE_RCPT) {
	if (state->orig_rcpt != 0) {
	    if (type != REC_TYPE_DONE)
		msg_warn("%s: out-of-order original recipient record <%.200s>",
			 state->queue_id, buf);
	    myfree(state->orig_rcpt);
	    state->orig_rcpt = 0;
	}
    }
    if (type == REC_TYPE_RCPT) {
	if (state->orig_rcpt == 0)
	    state->orig_rcpt = mystrdup(buf);
	cleanup_addr_recipient(state, buf);
	myfree(state->orig_rcpt);
	state->orig_rcpt = 0;
	return;
    } else if (type == REC_TYPE_ORCP) {
	state->orig_rcpt = mystrdup(buf);
    }
    if (type != REC_TYPE_END) {
	cleanup_out(state, type, buf, len);
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
     * values.  For forward compatibility, we put the info into one record
     * (so that it is possible to switch back to an older Postfix version).
     */
    if (vstream_fseek(state->dst, 0L, SEEK_SET) < 0)
	msg_fatal("%s: vstream_fseek %s: %m", myname, cleanup_path);
    cleanup_out_format(state, REC_TYPE_SIZE, REC_TYPE_SIZE_FORMAT,
	    (REC_TYPE_SIZE_CAST1) (state->xtra_offset - state->data_offset),
		       (REC_TYPE_SIZE_CAST2) state->data_offset,
		       (REC_TYPE_SIZE_CAST3) state->rcpt_count);

    /*
     * Update the preliminary start-of-content marker with the actual value.
     * For forward compatibility, we keep this information until the end of
     * the year 2002 (so that it is possible to switch back to an older
     * Postfix version).
     */
    if (vstream_fseek(state->dst, state->mesg_offset, SEEK_SET) < 0)
	msg_fatal("%s: vstream_fseek %s: %m", myname, cleanup_path);
    cleanup_out_format(state, REC_TYPE_MESG, REC_TYPE_MESG_FORMAT,
		       (REC_TYPE_MESG_CAST) state->xtra_offset);
}
