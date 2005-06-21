/*++
/* NAME
/*	cleanup_bounce 3
/* SUMMARY
/*	bounce all recipients
/* SYNOPSIS
/*	#include "cleanup.h"
/*
/*	void	cleanup_bounce(state)
/*	CLEANUP_STATE *state;
/* DESCRIPTION
/*	cleanup_bounce() updates the bounce log on request by client
/*	programs that cannot handle such problems themselves.
/*
/*	Upon successful completion, all error flags are reset.
/*	Otherwise, the CLEANUP_STAT_WRITE error flag is raised.
/*
/*	Arguments:
/* .IP state
/*	Queue file and message processing state. This state is
/*	updated as records are processed and as errors happen.
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

/* Utility library. */

#include <msg.h>
#include <stringops.h>
#include <stdlib.h>

/* Global library. */

#include <cleanup_user.h>
#include <mail_params.h>
#include <mail_proto.h>
#include <bounce.h>
#include <dsn_util.h>
#include <record.h>
#include <rec_type.h>
#include <dsn_mask.h>
#include <mail_queue.h>
#include <dsn_attr_map.h>
#include <deliver_completed.h>

/* Application-specific. */

#include "cleanup.h"

#define STR(x) vstring_str(x)

/* cleanup_bounce_append - update bounce logfile */

static void cleanup_bounce_append(CLEANUP_STATE *state, RECIPIENT *rcpt,
				          DSN *dsn)
{
    const char *myname = "cleanup_bounce_append";
    long    last_offset;

    if (cleanup_bounce_path == 0) {
	cleanup_bounce_path = vstring_alloc(10);
	(void) mail_queue_path(cleanup_bounce_path, MAIL_QUEUE_BOUNCE,
			       state->queue_id);
    }
    if (bounce_append(BOUNCE_FLAG_CLEAN, state->queue_id, state->time,
		      rcpt, "none", dsn) != 0) {
	msg_warn("%s: bounce logfile update error", state->queue_id);
	state->errs |= CLEANUP_STAT_WRITE;
    } else if (rcpt->offset > 0) {
	if ((last_offset = vstream_ftell(state->dst)) < 0)
	    msg_fatal("%s: vstream_ftell %s: %m", myname, cleanup_path);
	deliver_completed(state->dst, rcpt->offset);
	if (vstream_fseek(state->dst, last_offset, SEEK_SET) < 0)
	    msg_fatal("%s: seek %s: %m", myname, cleanup_path);
    }
}

/* cleanup_bounce - bounce all recipients */

int     cleanup_bounce(CLEANUP_STATE *state)
{
    const char *myname = "cleanup_bounce";
    VSTRING *buf = vstring_alloc(100);
    CLEANUP_STAT_DETAIL *detail;
    DSN_SPLIT dp;
    const char *dsn_status;
    const char *dsn_text;
    char   *rcpt = 0;
    RECIPIENT recipient;
    DSN     dsn;
    char   *attr_name;
    char   *attr_value;
    char   *dsn_orcpt = 0;
    int     dsn_notify = 0;
    char   *orig_rcpt = 0;
    char   *start;
    int     rec_type;
    int     junk;
    long    curr_offset;

    /*
     * Parse the failure reason if one was given, otherwise use a generic
     * mapping from cleanup-internal error code to (DSN + text).
     */
    if (state->reason) {
	dsn_split(&dp, "5.0.0", state->reason);
	dsn_status = DSN_STATUS(dp.dsn);
	dsn_text = dp.text;
    } else {
	detail = cleanup_stat_detail(state->errs);
	dsn_status = detail->dsn;
	dsn_text = detail->text;
    }

    /*
     * Create a bounce logfile with one entry for each final recipient.
     * Degrade gracefully in case of no recipients or no queue file.
     * 
     * We're NOT going to flush the bounce file from the cleanup server; if we
     * need to write trace logfile records, and the trace service fails, we
     * must be able to cancel the entire cleanup request including any trace
     * or bounce logfiles. The queue manager will flush the bounce (and
     * trace) logfile, possibly after it has generated its own success or
     * failure notification records.
     * 
     * Victor Duchovni observes that the number of recipients in the queue file
     * can potentially be very large due to virtual alias expansion. This can
     * expand the recipient count by virtual_alias_expansion_limit (default:
     * 1000) times.
     */
    if (vstream_fseek(state->dst, 0L, SEEK_SET) < 0)
	msg_fatal("%s: seek %s: %m", myname, cleanup_path);

    while ((state->errs & CLEANUP_STAT_WRITE) == 0) {
	if ((curr_offset = vstream_ftell(state->dst)) < 0)
	    msg_fatal("%s: vstream_ftell %s: %m", myname, cleanup_path);
	if ((rec_type = rec_get(state->dst, buf, 0)) <= 0
	    || rec_type == REC_TYPE_END)
	    break;
	start = STR(buf);
	if (rec_type == REC_TYPE_ATTR) {
	    if (split_nameval(STR(buf), &attr_name, &attr_value) != 0
		|| *attr_value == 0)
		continue;
	    /* Map DSN attribute names to pseudo record type. */
	    if ((junk = dsn_attr_map(attr_name)) != 0) {
		start = attr_value;
		rec_type = junk;
	    }
	}
	switch (rec_type) {
	case REC_TYPE_DSN_ORCPT:		/* RCPT TO ORCPT parameter */
	    if (dsn_orcpt != 0)			/* can't happen */
		myfree(dsn_orcpt);
	    dsn_orcpt = mystrdup(start);
	    break;
	case REC_TYPE_DSN_NOTIFY:		/* RCPT TO NOTIFY parameter */
	    if (alldig(start) && (junk = atoi(start)) > 0
		&& DSN_NOTIFY_OK(junk))
		dsn_notify = junk;
	    else
		dsn_notify = 0;
	    break;
	case REC_TYPE_ORCP:			/* unmodified RCPT TO address */
	    if (orig_rcpt != 0)			/* can't happen */
		myfree(orig_rcpt);
	    orig_rcpt = mystrdup(start);
	    break;
	case REC_TYPE_RCPT:			/* rewritten RCPT TO address */
	    rcpt = start;
	    RECIPIENT_ASSIGN(&recipient, curr_offset,
			     dsn_orcpt ? dsn_orcpt : "", dsn_notify,
			     orig_rcpt ? orig_rcpt : rcpt, rcpt);
	    (void) DSN_SIMPLE(&dsn, dsn_status, dsn_text);
	    cleanup_bounce_append(state, &recipient, &dsn);
	    /* FALLTHROUGH */
	case REC_TYPE_DONE:			/* can't happen */
	    if (orig_rcpt != 0) {
		myfree(orig_rcpt);
		orig_rcpt = 0;
	    }
	    if (dsn_orcpt != 0) {
		myfree(dsn_orcpt);
		dsn_orcpt = 0;
	    }
	    dsn_notify = 0;
	    break;
	}
    }
    if (orig_rcpt != 0)				/* can't happen */
	myfree(orig_rcpt);
    if (dsn_orcpt != 0)				/* can't happen */
	myfree(dsn_orcpt);

    /*
     * No recipients. Yes, this can happen.
     */
    if (rcpt == 0) {
	RECIPIENT_ASSIGN(&recipient, 0, "", 0, "", "unknown");
	(void) DSN_SIMPLE(&dsn, dsn_status, dsn_text);
	cleanup_bounce_append(state, &recipient, &dsn);
    }
    vstring_free(buf);

    return (state->errs &= CLEANUP_STAT_WRITE);
}
