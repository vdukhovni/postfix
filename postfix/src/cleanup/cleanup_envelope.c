/*++
/* NAME
/*	cleanup_envelope 3
/* SUMMARY
/*	process envelope segment
/* SYNOPSIS
/*	#include <cleanup.h>
/*
/*	void	cleanup_envelope(state, type, buf, len)
/*	CLEANUP_STATE *state;
/*	int	type;
/*	const char *buf;
/*	int	len;
/* DESCRIPTION
/*	This module processes envelope records and writes the result
/*	to the queue file.  It validates the message structure, rewrites
/*	sender/recipient addresses to canonical form, and expands recipients
/*	according to entries in the virtual table. This routine absorbs but
/*	does not emit the envelope to content boundary record.
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
#include <string.h>
#include <stdlib.h>

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

/* Utility library. */

#include <msg.h>
#include <vstring.h>
#include <vstream.h>
#include <mymalloc.h>
#include <stringops.h>
#include <nvtable.h>

/* Global library. */

#include <record.h>
#include <rec_type.h>
#include <cleanup_user.h>
#include <tok822.h>
#include <mail_params.h>
#include <ext_prop.h>
#include <mail_addr.h>
#include <canon_addr.h>
#include <verp_sender.h>

/* Application-specific. */

#include "cleanup.h"

#define STR	vstring_str

static void cleanup_envelope_process(CLEANUP_STATE *, int, const char *, int);

/* cleanup_envelope - initialize message envelope */

void    cleanup_envelope(CLEANUP_STATE *state, int type,
			         const char *str, int len)
{

    /*
     * The message size and count record goes first, so it can easily be
     * updated in place. This information takes precedence over any size
     * estimate provided by the client. It's all in one record, data size
     * first, for backwards compatibility reasons.
     */
    cleanup_out_format(state, REC_TYPE_SIZE, REC_TYPE_SIZE_FORMAT,
		       (REC_TYPE_SIZE_CAST1) 0,	/* content size */
		       (REC_TYPE_SIZE_CAST2) 0,	/* content offset */
		       (REC_TYPE_SIZE_CAST3) 0);	/* recipient count */

    /*
     * Pass control to the actual envelope processing routine.
     */
    state->action = cleanup_envelope_process;
    cleanup_envelope_process(state, type, str, len);
}

/* cleanup_envelope_process - process one envelope record */

static void cleanup_envelope_process(CLEANUP_STATE *state, int type,
				             const char *buf, int len)
{
    char   *attr_name;
    char   *attr_value;
    const char *error_text;
    int     extra_flags;

    if (msg_verbose)
	msg_info("initial envelope %c %.*s", type, len, buf);

    if (type == REC_TYPE_FLGS) {
	/* Not part of queue file format. */
	extra_flags = atol(buf);
	if (extra_flags & ~CLEANUP_FLAG_MASK_EXTRA)
	    msg_warn("%s: ignoring bad extra flags: 0x%x",
		     state->queue_id, extra_flags);
	else
	    state->flags |= extra_flags;
	return;
    }
    if (strchr(REC_TYPE_ENVELOPE, type) == 0) {
	msg_warn("%s: message rejected: unexpected record type %d in envelope",
		 state->queue_id, type);
	state->errs |= CLEANUP_STAT_BAD;
	return;
    }

    /*
     * The code for processing recipient records is first, because there can
     * be lots of them. However, recipient records appear at the end of the
     * initial or extracted envelope, so that the queue manager does not have
     * to read the whole envelope before it can start deliveries.
     */
    if (type == REC_TYPE_RCPT) {
	state->flags |= CLEANUP_FLAG_INRCPT;
	if (state->sender == 0) {		/* protect showq */
	    msg_warn("%s: message rejected: envelope recipient precedes sender",
		     state->queue_id);
	    state->errs |= CLEANUP_STAT_BAD;
	    return;
	}
	if (state->orig_rcpt == 0)
	    state->orig_rcpt = mystrdup(buf);
	cleanup_addr_recipient(state, buf);
	myfree(state->orig_rcpt);
	state->orig_rcpt = 0;
	return;
    }
    if (type == REC_TYPE_DONE) {
	state->flags |= CLEANUP_FLAG_INRCPT;
	if (state->orig_rcpt != 0) {
	    myfree(state->orig_rcpt);
	    state->orig_rcpt = 0;
	}
	return;
    }
    if (state->orig_rcpt != 0) {
	/* REC_TYPE_ORCP must be followed by REC_TYPE_RCPT or REC_TYPE DONE. */
	msg_warn("%s: ignoring out-of-order original recipient record <%.200s>",
		 state->queue_id, state->orig_rcpt);
	myfree(state->orig_rcpt);
	state->orig_rcpt = 0;
    }
    if (type == REC_TYPE_ORCP) {
	state->flags |= CLEANUP_FLAG_INRCPT;
	state->orig_rcpt = mystrdup(buf);
	return;
    }

    /*
     * These non-recipient records may appear before or after recipient
     * records. In order to keep recipient records pure, We take away these
     * non-recipient records from the input, and output them at the start of
     * the extracted envelope segment.
     */
    if (type == REC_TYPE_FILT) {
	/* Last instance wins. */
	if (strchr(buf, ':') == 0) {
	    msg_warn("%s: ignoring invalid content filter: %.100s",
		     state->queue_id, buf);
	    return;
	}
	if (state->filter)
	    myfree(state->filter);
	state->filter = mystrdup(buf);
	return;
    }
    if (type == REC_TYPE_RDR) {
	/* Last instance wins. */
	if (strchr(buf, '@') == 0) {
	    msg_warn("%s: ignoring invalid redirect address: %.100s",
		     state->queue_id, buf);
	    return;
	}
	if (state->redirect)
	    myfree(state->redirect);
	state->redirect = mystrdup(buf);
	return;
    }

    /*
     * The following records must not appear after recipient records. We
     * force the warning record before the sender record so we know when
     * (not) to emit a warning record. A warning or size record may already
     * be present when mail is requeued with "postsuper -r".
     */
    if (type != REC_TYPE_MESG && (state->flags & CLEANUP_FLAG_INRCPT) != 0) {
	msg_warn("%s: ignoring %s record after initial envelope recipients",
		 state->queue_id, rec_type_name(type));
	return;
    }
    if (type == REC_TYPE_SIZE)
	/* Use our own SIZE record instead. */
	return;
    if (type == REC_TYPE_TIME) {
	/* First instance wins. */
	if (state->time == 0) {
	    state->time = atol(buf);
	    cleanup_out(state, type, buf, len);
	}
	return;
    }
    if (type == REC_TYPE_FULL) {
	/* First instance wins. */
	if (state->fullname == 0) {
	    state->fullname = mystrdup(buf);
	    cleanup_out(state, type, buf, len);
	}
	return;
    }
    if (type == REC_TYPE_FROM) {
	/* Allow only one instance. */
	if (state->sender != 0) {
	    msg_warn("%s: message rejected: multiple envelope sender records",
		     state->queue_id);
	    state->errs |= CLEANUP_STAT_BAD;
	    return;
	}
	/* Kluge to force REC_TYPE_WARN before recipients. */
	if (state->warn_seen == 0 && var_delay_warn_time > 0) {
	    cleanup_out_format(state, REC_TYPE_WARN, REC_TYPE_WARN_FORMAT,
			       (long) var_delay_warn_time);
	    state->warn_seen = 1;
	}
	cleanup_addr_sender(state, buf);
	return;
    }
    if (type == REC_TYPE_WARN) {
	/* First instance wins. */
	if (state->warn_seen == 0) {
	    if (atoi(buf) < 0) {
		msg_warn("%s: message rejected: bad warning time: %.100s",
			 state->queue_id, buf);
		state->errs |= CLEANUP_STAT_BAD;
		return;
	    }
	    state->warn_seen = 1;
	    cleanup_out(state, type, buf, len);
	}
	return;
    }
    if (type == REC_TYPE_VERP) {
	/* First instance wins. */
	if (state->verp_seen == 0) {
	    if ((error_text = verp_delims_verify(buf)) != 0) {
		msg_warn("%s: message rejected: %s: %.100s",
			 state->queue_id, error_text, buf);
		state->errs |= CLEANUP_STAT_BAD;
		return;
	    }
	    state->verp_seen = 1;
	    cleanup_out(state, type, buf, len);
	}
	return;
    }
    if (type == REC_TYPE_ATTR) {
	/* Pass through. Last instance wins. */
	char   *sbuf;

	if (state->attr->used >= var_qattr_count_limit) {
	    msg_warn("%s: message rejected: attribute count exceeds limit %d",
		     state->queue_id, var_qattr_count_limit);
	    state->errs |= CLEANUP_STAT_BAD;
	    return;
	}
	sbuf = mystrdup(buf);
	if ((error_text = split_nameval(sbuf, &attr_name, &attr_value)) != 0) {
	    msg_warn("%s: message rejected: malformed attribute: %s: %.100s",
		     state->queue_id, error_text, buf);
	    state->errs |= CLEANUP_STAT_BAD;
	    myfree(sbuf);
	    return;
	}
	nvtable_update(state->attr, attr_name, attr_value);
	myfree(sbuf);
	cleanup_out(state, type, buf, len);
	return;
    }
    if (type != REC_TYPE_MESG) {
	/* Any other allowed record type. Pass through. */
	cleanup_out(state, type, buf, len);
	return;
    }

    /*
     * On the transition from initial envelope segment to content segment, do
     * some sanity checks.
     * 
     * XXX If senders can be specified in the extracted envelope segment (this
     * could reduce qmqpd's memory requirements), then we need to move the
     * VERP test there, too.
     */
    if (state->sender == 0 || state->time == 0) {
	msg_warn("%s: message rejected: missing sender or time envelope record",
		 state->queue_id);
	state->errs |= CLEANUP_STAT_BAD;
	return;
    }
    if (state->verp_seen && (state->sender == 0 || *state->sender == 0)) {
	msg_warn("%s: message rejected: VERP request with no or null sender",
		 state->queue_id);
	state->errs |= CLEANUP_STAT_BAD;
	return;
    }
    state->flags &= ~CLEANUP_FLAG_INRCPT;
    state->action = cleanup_message;
}
