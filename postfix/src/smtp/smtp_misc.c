/*++
/* NAME
/*	smtp_misc 3
/* SUMMARY
/*	assorted routines
/* SYNOPSIS
/*	#include <smtp.h>
/*
/*	void	smtp_rcpt_done(state, reply, rcpt)
/*	SMTP_STATE *state;
/*	const char *reply;
/*	RECIPIENT *rcpt;
/*
/*	int	smtp_rcpt_mark_finish(SMTP_STATE *state)
/*	SMTP_STATE *state;
/* DESCRIPTION
/*	smtp_rcpt_done() logs that a recipient is completed and upon
/*	success it marks the recipient as done in the queue file.
/*	Finally, it marks the in-memory recipient as DROP.
/*
/*	smtp_rcpt_mark_finish() cleans up the in-memory recipient list.
/*	It deletes recipients marked DROP, and unmarks recipients marked KEEP.
/*	It enforces the requirement that all recipients are marked one way
/*	or the other. The result value is the number of left-over recipients.
/* DIAGNOSTICS
/*	Panic: interface violation.
/*
/*	When a recipient can't be logged as completed, the recipient is
/*	logged as deferred instead.
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

/* System  library. */

#include <sys_defs.h>
#include <stdlib.h>			/* smtp_rcpt_mark_finish  */

/* Utility  library. */

#include <msg.h>

/* Global library. */

#include <deliver_request.h>		/* smtp_rcpt_done */
#include <deliver_completed.h>		/* smtp_rcpt_done */
#include <sent.h>			/* smtp_rcpt_done */

/* Application-specific. */

#include <smtp.h>

/* smtp_rcpt_done - mark recipient as done or else */

void    smtp_rcpt_done(SMTP_STATE *state, const char *reply, RECIPIENT *rcpt)
{
    DELIVER_REQUEST *request = state->request;
    SMTP_SESSION *session = state->session;
    int     status;

    /*
     * Report success and delete the recipient from the delivery request.
     * Defer if the success can't be reported.
     */
    status = sent(DEL_REQ_TRACE_FLAGS(request->flags),
		  request->queue_id, rcpt->orig_addr,
		  rcpt->address, rcpt->offset,
		  session->namaddr,
		  request->arrival_time,
		  "%s", reply);
    if (status == 0)
	if (request->flags & DEL_REQ_FLAG_SUCCESS)
	    deliver_completed(state->src, rcpt->offset);
    SMTP_RCPT_MARK_DROP(state, rcpt);
    state->status |= status;
}

/* smtp_rcpt_mark_finish_callback - qsort callback */

static int smtp_rcpt_mark_finish_callback(const void *a, const void *b)
{
    return (((RECIPIENT *) a)->status - ((RECIPIENT *) b)->status);
}

/* smtp_rcpt_mark_finish - purge completed recipients from request */

int     smtp_rcpt_mark_finish(SMTP_STATE *state)
{
    RECIPIENT_LIST *rcpt_list = &state->request->rcpt_list;
    RECIPIENT *rcpt;

    /*
     * Sanity checks.
     */
    if (state->drop_count + state->keep_count != rcpt_list->len)
	msg_panic("smtp_rcpt_mark_finish: recipient count mismatch: %d+%d!=%d",
		  state->drop_count, state->keep_count, rcpt_list->len);

    /*
     * Recipients marked KEEP sort before recipients marked DROP. Skip the
     * sorting in the common case that all recipients are marked the same.
     */
    if (state->drop_count > 0 && state->keep_count > 0)
	qsort((void *) rcpt_list->info, rcpt_list->len,
	      sizeof(rcpt_list->info), smtp_rcpt_mark_finish_callback);

    /*
     * Truncate the recipient list and unmark the left-over recipients so
     * that the result looks like a brand-new recipient list.
     */
    if (state->keep_count < rcpt_list->len)
	recipient_list_truncate(rcpt_list, state->keep_count);
    for (rcpt = rcpt_list->info; rcpt < rcpt_list->info + rcpt_list->len; rcpt++)
	rcpt->status = 0;

    return (rcpt_list->len);
}
