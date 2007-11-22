/*++
/* NAME
/*	qmgr_queue 3
/* SUMMARY
/*	per-destination queues
/* SYNOPSIS
/*	#include "qmgr.h"
/*
/*	int	qmgr_queue_count;
/*
/*	QMGR_QUEUE *qmgr_queue_create(transport, name, nexthop)
/*	QMGR_TRANSPORT *transport;
/*	const char *name;
/*	const char *nexthop;
/*
/*	void	qmgr_queue_done(queue)
/*	QMGR_QUEUE *queue;
/*
/*	QMGR_QUEUE *qmgr_queue_find(transport, name)
/*	QMGR_TRANSPORT *transport;
/*	const char *name;
/*
/*	void	qmgr_queue_throttle(queue, dsn)
/*	QMGR_QUEUE *queue;
/*	DSN	*dsn;
/*
/*	void	qmgr_queue_unthrottle(queue)
/*	QMGR_QUEUE *queue;
/* DESCRIPTION
/*	These routines add/delete/manipulate per-destination queues.
/*	Each queue corresponds to a specific transport and destination.
/*	Each queue has a `todo' list of delivery requests for that
/*	destination, and a `busy' list of delivery requests in progress.
/*
/*	qmgr_queue_count is a global counter for the total number
/*	of in-core queue structures.
/*
/*	qmgr_queue_create() creates an empty named queue for the named
/*	transport and destination. The queue is given an initial
/*	concurrency limit as specified with the
/*	\fIinitial_destination_concurrency\fR configuration parameter,
/*	provided that it does not exceed the transport-specific
/*	concurrency limit.
/*
/*	qmgr_queue_done() disposes of a per-destination queue after all
/*	its entries have been taken care of. It is an error to dispose
/*	of a dead queue.
/*
/*	qmgr_queue_find() looks up the named queue for the named
/*	transport. A null result means that the queue was not found.
/*
/*	qmgr_queue_throttle() handles a delivery error, and decrements the
/*	concurrency limit for the destination, with a lower bound of 1.
/*	When the cohort failure bound is reached, qmgr_queue_throttle()
/*	sets the concurrency limit to zero and starts a timer
/*	to re-enable delivery to the destination after a configurable delay.
/*
/*	qmgr_queue_unthrottle() undoes qmgr_queue_throttle()'s effects.
/*	The concurrency limit for the destination is incremented,
/*	provided that it does not exceed the destination concurrency
/*	limit specified for the transport. This routine implements
/*	"slow open" mode, and eliminates the "thundering herd" problem.
/* DIAGNOSTICS
/*	Panic: consistency check failure.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*
/*	Pre-emptive scheduler enhancements:
/*	Patrik Rak
/*	Modra 6
/*	155 00, Prague, Czech Republic
/*--*/

/* System library. */

#include <sys_defs.h>
#include <time.h>
#include <math.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <events.h>
#include <htable.h>
#include <name_code.h>

/* Global library. */

#include <mail_params.h>
#include <recipient_list.h>
#include <mail_proto.h>			/* QMGR_LOG_WINDOW */

/* Application-specific. */

#include "qmgr.h"

int     qmgr_queue_count;

 /*
  * Lookup tables for main.cf feedback method names.
  */
#define QMGR_FDBACK_CODE_BAD		0
#define QMGR_FDBACK_CODE_FIXED_1	1
#define QMGR_FDBACK_CODE_INVERSE_WIN	2
#define QMGR_FDBACK_CODE_INVERSE_1	QMGR_FDBACK_CODE_INVERSE_WIN
#define QMGR_FDBACK_CODE_INV_SQRT_WIN	3
#define QMGR_FDBACK_CODE_INV_SQRT	QMGR_FDBACK_CODE_INV_SQRT_WIN

NAME_CODE qmgr_feedback_map[] = {
    QMGR_FDBACK_NAME_FIXED_1, QMGR_FDBACK_CODE_FIXED_1,
    QMGR_FDBACK_NAME_INVERSE_WIN, QMGR_FDBACK_CODE_INVERSE_WIN,
    QMGR_FDBACK_NAME_INVERSE_1, QMGR_FDBACK_CODE_INVERSE_1,
    QMGR_FDBACK_NAME_INV_SQRT_WIN, QMGR_FDBACK_CODE_INV_SQRT_WIN,
    QMGR_FDBACK_NAME_INV_SQRT, QMGR_FDBACK_CODE_INV_SQRT,
    0, QMGR_FDBACK_CODE_BAD,
};
static int qmgr_pos_feedback_idx;
static int qmgr_neg_feedback_idx;

 /*
  * Choosing the right feedback method at run-time.
  */
#define QMGR_FEEDBACK_VAL(idx, window) ( \
	(idx) == QMGR_FDBACK_CODE_INVERSE_1 ? (1.0 / (window)) : \
	(idx) == QMGR_FDBACK_CODE_FIXED_1 ? (1.0) : \
	(1.0 / sqrt(window)) \
    )

#define QMGR_ERROR_OR_RETRY_QUEUE(queue) \
	(strcmp(queue->transport->name, MAIL_SERVICE_RETRY) == 0 \
	    || strcmp(queue->transport->name, MAIL_SERVICE_ERROR) == 0)

#define QMGR_LOG_FEEDBACK(feedback) \
	if (var_qmgr_feedback_debug && !QMGR_ERROR_OR_RETRY_QUEUE(queue)) \
	    msg_info("%s: feedback %g", myname, feedback);

#define QMGR_LOG_WINDOW(queue) \
	if (var_qmgr_feedback_debug && !QMGR_ERROR_OR_RETRY_QUEUE(queue)) \
	    msg_info("%s: queue %s: limit %d window %d success %g failure %g fail_cohorts %g", \
		    myname, queue->name, queue->transport->dest_concurrency_limit, \
		    queue->window, queue->success, queue->failure, queue->fail_cohorts);

/* qmgr_queue_feedback_init - initialize feedback selection */

void    qmgr_queue_feedback_init(void)
{

    /*
     * Positive and negative feedback method indices.
     */
    qmgr_pos_feedback_idx = name_code(qmgr_feedback_map, NAME_CODE_FLAG_NONE,
				      var_qmgr_pos_feedback);
    if (qmgr_pos_feedback_idx == QMGR_FDBACK_CODE_BAD)
	msg_fatal("%s: bad feedback method: %s",
		  VAR_QMGR_POS_FDBACK, var_qmgr_pos_feedback);
    if (var_qmgr_feedback_debug)
	msg_info("positive feedback method %d, value at %d: %g",
		 qmgr_pos_feedback_idx, var_init_dest_concurrency,
		 QMGR_FEEDBACK_VAL(qmgr_pos_feedback_idx,
				   var_init_dest_concurrency));

    qmgr_neg_feedback_idx = name_code(qmgr_feedback_map, NAME_CODE_FLAG_NONE,
				      var_qmgr_neg_feedback);
    if (qmgr_neg_feedback_idx == QMGR_FDBACK_CODE_BAD)
	msg_fatal("%s: bad feedback method: %s",
		  VAR_QMGR_NEG_FDBACK, var_qmgr_neg_feedback);
    if (var_qmgr_feedback_debug)
	msg_info("negative feedback method %d, value at %d: %g",
		 qmgr_neg_feedback_idx, var_init_dest_concurrency,
		 QMGR_FEEDBACK_VAL(qmgr_neg_feedback_idx,
				   var_init_dest_concurrency));
}

/* qmgr_queue_unthrottle_wrapper - in case (char *) != (struct *) */

static void qmgr_queue_unthrottle_wrapper(int unused_event, char *context)
{
    QMGR_QUEUE *queue = (QMGR_QUEUE *) context;

    /*
     * This routine runs when a wakeup timer goes off; it does not run in the
     * context of some queue manipulation. Therefore, it is safe to discard
     * this in-core queue when it is empty and when this site is not dead.
     */
    qmgr_queue_unthrottle(queue);
    if (queue->window > 0 && queue->todo.next == 0 && queue->busy.next == 0)
	qmgr_queue_done(queue);
}

/* qmgr_queue_unthrottle - give this destination another chance */

void    qmgr_queue_unthrottle(QMGR_QUEUE *queue)
{
    const char *myname = "qmgr_queue_unthrottle";
    QMGR_TRANSPORT *transport = queue->transport;
    double  feedback;
    double  multiplier;

    if (msg_verbose)
	msg_info("%s: queue %s", myname, queue->name);

    /*
     * Don't restart the negative feedback hysteresis cycle with every
     * positive feedback. Restart it only when we make a positive concurrency
     * adjustment (i.e. at the end of a positive feedback hysteresis cycle).
     * Otherwise negative feedback would be too aggressive: negative feedback
     * takes effect immediately at the start of its hysteresis cycle.
     */
    queue->fail_cohorts = 0;

    /*
     * Special case when this site was dead.
     */
    if (queue->window == 0) {
	event_cancel_timer(qmgr_queue_unthrottle_wrapper, (char *) queue);
	if (queue->dsn == 0)
	    msg_panic("%s: queue %s: window 0 status 0", myname, queue->name);
	dsn_free(queue->dsn);
	queue->dsn = 0;
	/* Back from the almost grave, best concurrency is anyone's guess. */
	if (queue->busy_refcount > 0)
	    queue->window = queue->busy_refcount;
	else
	    queue->window = transport->init_dest_concurrency;
	queue->success = queue->failure = 0;
	QMGR_LOG_WINDOW(queue);
	return;
    }

    /*
     * Increase the destination's concurrency limit until we reach the
     * transport's concurrency limit. Allow for a margin the size of the
     * initial destination concurrency, so that we're not too gentle.
     * 
     * Why is the concurrency increment based on preferred concurrency and not
     * on the number of outstanding delivery requests? The latter fluctuates
     * wildly when deliveries complete in bursts (artificial benchmark
     * measurements), and does not account for cached connections.
     * 
     * Keep the window within reasonable distance from actual concurrency
     * otherwise negative feedback will be ineffective. This expression
     * assumes that busy_refcount changes gradually. This is invalid when
     * deliveries complete in bursts (artificial benchmark measurements).
     */
    if (transport->dest_concurrency_limit == 0
	|| transport->dest_concurrency_limit > queue->window)
	if (queue->window < queue->busy_refcount + transport->init_dest_concurrency) {
	    feedback = QMGR_FEEDBACK_VAL(qmgr_pos_feedback_idx, queue->window);
	    QMGR_LOG_FEEDBACK(feedback);
	    queue->success += feedback;
	    /* Prepare for overshoot (feedback > hysteresis, rounding error). */
	    while (queue->success >= var_qmgr_pos_hysteresis) {
		queue->window += var_qmgr_pos_hysteresis;
		queue->success -= var_qmgr_pos_hysteresis;
		queue->failure = 0;
	    }
	    /* Prepare for overshoot. */
	    if (transport->dest_concurrency_limit > 0
		&& queue->window > transport->dest_concurrency_limit)
		queue->window = transport->dest_concurrency_limit;
	}
    QMGR_LOG_WINDOW(queue);
}

/* qmgr_queue_throttle - handle destination delivery failure */

void    qmgr_queue_throttle(QMGR_QUEUE *queue, DSN *dsn)
{
    const char *myname = "qmgr_queue_throttle";
    double  feedback;

    /*
     * Sanity checks.
     */
    if (queue->dsn)
	msg_panic("%s: queue %s: spurious reason %s",
		  myname, queue->name, queue->dsn->reason);
    if (msg_verbose)
	msg_info("%s: queue %s: %s %s",
		 myname, queue->name, dsn->status, dsn->reason);

    /*
     * Don't restart the positive feedback hysteresis cycle with every
     * negative feedback. Restart it only when we make a negative concurrency
     * adjustment (i.e. at the start of a negative feedback hysteresis
     * cycle). Otherwise positive feedback would be too weak (positive
     * feedback does not take effect until the end of its hysteresis cycle).
     */

    /*
     * This queue is declared dead after a configurable number of
     * pseudo-cohort failures.
     */
    if (queue->window > 0) {
	queue->fail_cohorts += 1.0 / queue->window;
	if (queue->fail_cohorts >= var_qmgr_sac_cohorts)
	    queue->window = 0;
    }

    /*
     * Decrease the destination's concurrency limit until we reach 1. Base
     * adjustments on the concurrency limit itself, instead of using the
     * actual concurrency. The latter fluctuates wildly when deliveries
     * complete in bursts (artificial benchmark measurements).
     * 
     * Even after reaching 1, we maintain the negative hysteresis cycle so that
     * negative feedback can cancel out positive feedback.
     */
    if (queue->window > 0) {
	feedback = QMGR_FEEDBACK_VAL(qmgr_neg_feedback_idx, queue->window);
	QMGR_LOG_FEEDBACK(feedback);
	queue->failure -= feedback;
	/* Prepare for overshoot (feedback > hysteresis, rounding error). */
	while (queue->failure < 0) {
	    queue->window -= var_qmgr_neg_hysteresis;
	    queue->success = 0;
	    queue->failure += var_qmgr_neg_hysteresis;
	}
	/* Prepare for overshoot. */
	if (queue->window < 1)
	    queue->window = 1;
    }

    /*
     * Special case for a site that just was declared dead.
     */
    if (queue->window == 0) {
	queue->dsn = DSN_COPY(dsn);
	event_request_timer(qmgr_queue_unthrottle_wrapper,
			    (char *) queue, var_min_backoff_time);
	queue->dflags = 0;
    }
    QMGR_LOG_WINDOW(queue);
}

/* qmgr_queue_done - delete in-core queue for site */

void    qmgr_queue_done(QMGR_QUEUE *queue)
{
    const char *myname = "qmgr_queue_done";
    QMGR_TRANSPORT *transport = queue->transport;

    /*
     * Sanity checks. It is an error to delete an in-core queue with pending
     * messages or timers.
     */
    if (queue->busy_refcount != 0 || queue->todo_refcount != 0)
	msg_panic("%s: refcount: %d", myname,
		  queue->busy_refcount + queue->todo_refcount);
    if (queue->todo.next || queue->busy.next)
	msg_panic("%s: queue not empty: %s", myname, queue->name);
    if (queue->window <= 0)
	msg_panic("%s: window %d", myname, queue->window);
    if (queue->dsn)
	msg_panic("%s: queue %s: spurious reason %s",
		  myname, queue->name, queue->dsn->reason);

    /*
     * Clean up this in-core queue.
     */
    QMGR_LIST_UNLINK(transport->queue_list, QMGR_QUEUE *, queue, peers);
    htable_delete(transport->queue_byname, queue->name, (void (*) (char *)) 0);
    myfree(queue->name);
    myfree(queue->nexthop);
    qmgr_queue_count--;
    myfree((char *) queue);
}

/* qmgr_queue_create - create in-core queue for site */

QMGR_QUEUE *qmgr_queue_create(QMGR_TRANSPORT *transport, const char *name,
			              const char *nexthop)
{
    QMGR_QUEUE *queue;

    /*
     * If possible, choose an initial concurrency of > 1 so that one bad
     * message or one bad network won't slow us down unnecessarily.
     */

    queue = (QMGR_QUEUE *) mymalloc(sizeof(QMGR_QUEUE));
    qmgr_queue_count++;
    queue->dflags = 0;
    queue->last_done = 0;
    queue->name = mystrdup(name);
    queue->nexthop = mystrdup(nexthop);
    queue->todo_refcount = 0;
    queue->busy_refcount = 0;
    queue->transport = transport;
    queue->window = transport->init_dest_concurrency;
    queue->success = queue->failure = queue->fail_cohorts = 0;
    QMGR_LIST_INIT(queue->todo);
    QMGR_LIST_INIT(queue->busy);
    queue->dsn = 0;
    queue->clog_time_to_warn = 0;
    queue->blocker_tag = 0;
    QMGR_LIST_APPEND(transport->queue_list, queue, peers);
    htable_enter(transport->queue_byname, name, (char *) queue);
    return (queue);
}

/* qmgr_queue_find - find in-core named queue */

QMGR_QUEUE *qmgr_queue_find(QMGR_TRANSPORT *transport, const char *name)
{
    return ((QMGR_QUEUE *) htable_find(transport->queue_byname, name));
}
