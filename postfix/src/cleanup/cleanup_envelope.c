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
	msg_warn("%s: unexpected record type %d in envelope: message rejected",
		 state->queue_id, type);
	state->errs |= CLEANUP_STAT_BAD;
	return;
    }
    if (type == REC_TYPE_RCPT) {
	if (state->sender == 0) {		/* protect showq */
	    msg_warn("%s: envelope recipient precedes sender: message rejected",
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
	if (state->orig_rcpt != 0) {
	    myfree(state->orig_rcpt);
	    state->orig_rcpt = 0;
	}
	return;
    }
    if (state->orig_rcpt != 0) {
	/* REC_TYPE_ORCP must be followed by REC_TYPE_RCPT or REC_TYPE DONE. */
	msg_warn("%s: out-of-order original recipient record <%.200s>",
		 state->queue_id, state->orig_rcpt);
	myfree(state->orig_rcpt);
	state->orig_rcpt = 0;
    }
    if (type == REC_TYPE_ORCP) {
	state->orig_rcpt = mystrdup(buf);
	return;
    }
    if (type == REC_TYPE_TIME) {
	/* First definition wins. */
	if (state->time == 0) {
	    state->time = atol(buf);
	    cleanup_out(state, type, buf, len);
	}
	return;
    }
    if (type == REC_TYPE_FULL) {
	/* First definition wins. */
	if (state->fullname == 0) {
	    state->fullname = mystrdup(buf);
	    cleanup_out(state, type, buf, len);
	}
	return;
    }
    if (type == REC_TYPE_FROM) {
	/* Allow only one instance. */
	if (state->sender != 0) {
	    msg_warn("%s: too many envelope sender records: message rejected",
		     state->queue_id);
	    state->errs |= CLEANUP_STAT_BAD;
	    return;
	}
	cleanup_addr_sender(state, buf);
	return;
    }
    if (type == REC_TYPE_WARN) {
	/* First definition wins. */
	if (state->warn_time == 0) {
	    if ((state->warn_time = atol(buf)) < 0) {
		msg_warn("%s: bad arrival time record: %s: message rejected",
			 state->queue_id, buf);
		state->errs |= CLEANUP_STAT_BAD;
	    }
	}
	return;
    }
    if (type == REC_TYPE_VERP) {
	/* First definition wins. */
	if (state->verp_seen == 0) {
	    if ((error_text = verp_delims_verify(buf)) != 0) {
		msg_warn("%s: %s: \"%s\": message rejected",
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
	/* Pass through. Last definition wins. */
	char   *sbuf;

	if (state->attr->used >= var_qattr_count_limit) {
	    msg_warn("%s: queue file attribute count exceeds safety limit %d"
		     ": message rejected",
		     state->queue_id, var_qattr_count_limit);
	    state->errs |= CLEANUP_STAT_BAD;
	    return;
	}
	sbuf = mystrdup(buf);
	if ((error_text = split_nameval(sbuf, &attr_name, &attr_value)) != 0) {
	    msg_warn("%s: malformed attribute: %s: %.100s: message rejected",
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
    if (type == REC_TYPE_SIZE)
	/* Use our own SIZE record instead. */
	return;
    if (type != REC_TYPE_MESG) {
	/* Anything else. Pass through. */
	cleanup_out(state, type, buf, len);
	return;
    }

    /*
     * On the transition from envelope segment to content segment, do some
     * sanity checks.
     * 
     * If senders can be specified in the extracted envelope segment, then we
     * need to move the VERP test there, too.
     */
    if (state->sender == 0 || state->time == 0) {
	msg_warn("%s: missing sender or time envelope record: message rejected",
		 state->queue_id);
	state->errs |= CLEANUP_STAT_BAD;
	return;
    }
    if (state->verp_seen && (state->sender == 0 || *state->sender == 0)) {
	msg_warn("%s: VERP request with no or null sender: message rejected",
		 state->queue_id);
	state->errs |= CLEANUP_STAT_BAD;
	return;
    }

    /*
     * Emit records for information that we collected from the envelope
     * segment.
     */
    if (state->warn_time == 0 && var_delay_warn_time > 0)
	state->warn_time = state->time + var_delay_warn_time;
    if (state->warn_time)
	cleanup_out_format(state, REC_TYPE_WARN, REC_TYPE_WARN_FORMAT,
			   state->warn_time);

    state->action = cleanup_message;
}
