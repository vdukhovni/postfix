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

static void cleanup_extracted_process(CLEANUP_STATE *, int, const char *, int);

 /*
  * The following queue file records are generated from message header or
  * message body content. We may encounter them in extracted envelope
  * segments after mail is re-injected with "postsuper -r" and we should
  * ignore them. It might be infinitesimally faster to move this test to the
  * pickup daemon, but that would make program maintenance more difficult.
  */
static char cleanup_extracted_generated[] = {
    REC_TYPE_RRTO,			/* return-receipt-to */
    REC_TYPE_ERTO,			/* errors-to */
    REC_TYPE_FILT,			/* content filter */
    REC_TYPE_INSP,			/* content inspector */
    REC_TYPE_RDR,			/* redirect address */
    REC_TYPE_ATTR,			/* some header attribute */
    0,
};

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
     * Postfix keeps all information related to an email message is in a
     * write-once file, including the envelope sender and recipients, and the
     * message content. This design maximizes robustness: one file is easier
     * to keep track of than multiple files, and write-once means that no
     * operation ever needs to be undone. This design also minimizes file
     * system overhead, because creating and removing files is relatively
     * expensive compared to writing files. Separate files are used for
     * logging the causes of deferral or failed delivery.
     * 
     * A Postfix queue file consists of three segments.
     * 
     * 1) The initial envelope segment with the arrival time, sender address,
     * recipients, and some other stuff that can be recorded before the
     * message content is received, including non-recipient information that
     * results from actions in Postfix SMTP server access tables. In this
     * segment, recipient records may be preceded or followed by
     * non-recipient records.
     * 
     * 2) The message content segment with the message headers and body. The
     * message body includes all the MIME segments, if there are any.
     * 
     * 3) The extracted envelope segment with information that was extracted
     * from message headers or from the message body, including recipient
     * addresses that were extracted from message headers, and non-recipient
     * information that results from actions in header/body_checks patterns.
     * In this segment, all non-recipient records precede the recipient
     * records.
     * 
     * There are two queue file layouts.
     * 
     * A) All recipient records are in the initial envelope segment, except for
     * the optional always_bcc recipient which is always stored in the
     * extracted envelope segment. The queue manager reads as many recipients
     * as it can from the initial envelope segment, and then examines all
     * remaining initial envelope records and all extracted envelope records,
     * picking up non-recipient information. This organization favors
     * messages with fewer than $qmgr_active_recipient_limit recipients.
     * 
     * B) All recipient records are stored in the extracted envelope segment,
     * after all non-recipient records. The queue manager is guaranteed to
     * have read all the non-recipient records before it sees the first
     * recipient record. This organization can handle messages with very
     * large numbers of recipients.
     * 
     * All this is the result of an evolutionary process, where compatibility
     * between Postfix versions was a major goal as new features were added.
     * Therefore the file organization is not optimal from a performance
     * point of view. In hindsight, the non-recipient information that
     * follows recipients in the initial envelope segment could be moved to
     * the extracted envelope segment. This would improve file organization
     * A)'s performance with very large numbers of recipients, by eliminating
     * the need to examine all initial envelope records before starting
     * deliveries.
     */
    if (state->filter != 0)
	cleanup_out_string(state, REC_TYPE_FILT, state->filter);

    /*
     * The optional redirect target address from header/body_checks actions.
     */
    if (state->redirect != 0)
	cleanup_out_string(state, REC_TYPE_RDR, state->redirect);

    /*
     * Older Postfix versions didn't MIME emit encoding information, so this
     * record can only be optional.
     */
    if ((encoding = nvtable_find(state->attr, MAIL_ATTR_ENCODING)) != 0)
	cleanup_out_format(state, REC_TYPE_ATTR, "%s=%s",
			   MAIL_ATTR_ENCODING, encoding);

    /*
     * Return-Receipt-To and Errors-To records are now optional.
     */
    if (state->return_receipt)
	cleanup_out_string(state, REC_TYPE_RRTO, state->return_receipt);
    if (state->errors_to)
	cleanup_out_string(state, REC_TYPE_ERTO, state->errors_to);

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

    if (msg_verbose)
	msg_info("extracted envelope %c %.*s", type, len, buf);

    if (strchr(REC_TYPE_EXTRACT, type) == 0) {
	msg_warn("%s: unexpected record type %d in extracted envelope"
		 ": message rejected", state->queue_id, type);
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
    if (strchr(cleanup_extracted_generated, type) != 0)
	/* Use our own message header extracted information instead. */
	return;
    if (type != REC_TYPE_END) {
	msg_warn("unexpected non-recipient record: %s", rec_type_name(type));
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
     * values.
     */
    if (vstream_fseek(state->dst, 0L, SEEK_SET) < 0)
	msg_fatal("%s: vstream_fseek %s: %m", myname, cleanup_path);
    cleanup_out_format(state, REC_TYPE_SIZE, REC_TYPE_SIZE_FORMAT,
	    (REC_TYPE_SIZE_CAST1) (state->xtra_offset - state->data_offset),
		       (REC_TYPE_SIZE_CAST2) state->data_offset,
		       (REC_TYPE_SIZE_CAST3) state->rcpt_count);
}
