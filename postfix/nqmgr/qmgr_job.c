/*++
/* NAME
/*	qmgr_job 3
/* SUMMARY
/*	per-transport jobs
/* SYNOPSIS
/*	#include "qmgr.h"
/*
/*	QMGR_JOB *qmgr_job_obtain(message, transport)
/*	QMGR_MESSAGE *message;
/*	QMGR_TRANSPORT *transport;
/*
/*	void qmgr_job_free(job)
/*	QMGR_JOB *job;
/*
/*	void qmgr_job_move_limits(job)
/*	QMGR_JOB *job;
/*
/*	QMGR_ENTRY *qmgr_job_entry_select(transport)
/*	QMGR_TRANSPORT *transport;
/* DESCRIPTION
/*	These routines add/delete/manipulate per-transport jobs.
/*	Each job corresponds to a specific transport and message.
/*	Each job has a peer list containing all pending delivery
/*	requests for that message.
/*
/*	qmgr_job_obtain() finds an existing job for named message and
/*	transport combination. New empty job is created if no existing can
/*	be found. In either case, the job is prepared for assignement of
/*	(more) message recipients.
/*
/*	qmgr_job_free() disposes of a per-transport job after all
/*	its entries have been taken care of. It is an error to dispose
/*	of a job that is still in use.
/*
/*	qmgr_job_entry_select() attempts to find the next entry suitable
/*	for delivery. The job preempting algorithm is also exercised.
/*	If necessary, an attempt to read more recipients into core is made.
/*	This can result in creation of more job, queue and entry structures.
/*
/*	qmgr_job_move_limits() takes care of proper distribution of the
/*	per-transport recipients limit among the per-transport jobs.
/*	Should be called whenever a job's recipient slot becomes available.
/* DIAGNOSTICS
/*	Panic: consistency check failure.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Patrik Rak
/*	patrik@raxoft.cz
/*--*/

/* System library. */

#include <sys_defs.h>

/* Utility library. */

#include <msg.h>
#include <events.h>
#include <htable.h>
#include <mymalloc.h>

/* Application-specific. */

#include "qmgr.h"

/* Forward declarations */

static void qmgr_job_pop(QMGR_JOB *);

/* Helper macros */

#define HAS_ENTRIES(job) ((job)->selected_entries < (job)->read_entries)

/*
 * The MIN_ENTRIES macro may underestimate a lot but we can't use message->rcpt_unread
 * because we don't know if all those unread recipients go to our transport yet.
 */

#define MIN_ENTRIES(job) ((job)->read_entries)
#define MAX_ENTRIES(job) ((job)->read_entries + (job)->message->rcpt_unread)

#define RESET_CANDIDATE_CACHE(transport) do { \
                                            (transport)->candidate_cache_time = (time_t) 0; \
                                            (transport)->candidate_cache = 0; \
                                        } while(0)

/* qmgr_job_create - create and initialize message job structure */

static QMGR_JOB *qmgr_job_create(QMGR_MESSAGE *message, QMGR_TRANSPORT *transport)
{
    QMGR_JOB *job;

    job = (QMGR_JOB *) mymalloc(sizeof(QMGR_JOB));
    job->message = message;
    QMGR_LIST_APPEND(message->job_list, job, message_peers);
    htable_enter(transport->job_byname, message->queue_id, (char *) job);
    job->transport = transport;
    QMGR_LIST_INIT(job->transport_peers);
    job->peer_byname = htable_create(0);
    QMGR_LIST_INIT(job->peer_list);
    job->stack_level = 0;
    job->slots_used = 0;
    job->slots_available = 0;
    job->selected_entries = 0;
    job->read_entries = 0;
    job->rcpt_count = 0;
    job->rcpt_limit = 0;
    return (job);
}

/* qmgr_job_link - append the job to the job list, according to the time it was queued */

static void qmgr_job_link(QMGR_JOB * job)
{
    QMGR_TRANSPORT *transport = job->transport;
    QMGR_MESSAGE *message = job->message;
    QMGR_JOB *prev,
           *next,
           *unread;
    int     delay;

    unread = transport->job_next_unread;

    /*
     * This may look inefficient but under normal operation it is expected
     * that the loop will stop right away, resulting in normal list append
     * below. However, this code is necessary for reviving retired jobs and
     * for jobs which are created long after the first chunk of recipients
     * was read in-core (either of these can happen only for multi-transport
     * messages).
     * 
     * In case this is found unsatisfactory one day, it's possible to deploy
     * some smarter technique (using some form of lookup trees perhaps).
     */
    for (next = 0, prev = transport->job_list.prev; prev;
	 next = prev, prev = prev->transport_peers.prev) {
	delay = message->queued_time - prev->message->queued_time;
	if (delay >= 0)
	    break;
	if (unread == prev)
	    unread = 0;
    }

    /*
     * Don't link the new job in front of the first job on the job list if
     * that job was already used for the regular delivery. This seems like a
     * subtle difference but it helps many invariants used at various other
     * places to remain true.
     */
    if (prev == 0 && next != 0 && next->slots_used != 0) {
	prev = next;
	next = next->transport_peers.next;

	/*
	 * The following is not currently necessary but is done anyway for
	 * the sake of consistency.
	 */
	if (prev == transport->job_next_unread)
	    unread = prev;
    }

    /*
     * Link the job into the proper place on the job list.
     */
    job->transport_peers.prev = prev;
    job->transport_peers.next = next;
    if (prev != 0)
	prev->transport_peers.next = job;
    else
	transport->job_list.next = job;
    if (next != 0)
	next->transport_peers.prev = job;
    else
	transport->job_list.prev = job;

    /*
     * Update the pointer to the first unread job on the job list and steal
     * the unused recipient slots from the old one.
     */
    if (unread == 0) {
	unread = transport->job_next_unread;
	transport->job_next_unread = job;
	if (unread != 0)
	    qmgr_job_move_limits(unread);
    }

    /*
     * Get as much recipient slots as possible. The excess will be returned
     * to the transport pool as soon as the exact amount required is known
     * (which is usually after all recipients have been read in core).
     */
    if (transport->rcpt_unused > 0) {
	job->rcpt_limit += transport->rcpt_unused;
	message->rcpt_limit += transport->rcpt_unused;
	transport->rcpt_unused = 0;
    }
}

/* qmgr_job_find - lookup job associated with named message and transport */

static QMGR_JOB *qmgr_job_find(QMGR_MESSAGE *message, QMGR_TRANSPORT *transport)
{

    /*
     * Instead of traversing the message job list, we use single per
     * transport hash table. This is better (at least with respect to memory
     * usage) than having single hash table (usually almost empty) for each
     * message.
     */
    return ((QMGR_JOB *) htable_find(transport->job_byname, message->queue_id));
}

/* qmgr_job_obtain - find/create the appropriate job and make it ready for new recipients */

QMGR_JOB *qmgr_job_obtain(QMGR_MESSAGE *message, QMGR_TRANSPORT *transport)
{
    QMGR_JOB *job;

    /*
     * Try finding an existing job and revive it if it was already retired.
     * Create a new job for this transport/message combination otherwise.
     */
    if ((job = qmgr_job_find(message, transport)) != 0) {
	if (job->stack_level < 0) {
	    job->stack_level = 0;
	    qmgr_job_link(job);
	}
    } else {
	job = qmgr_job_create(message, transport);
	qmgr_job_link(job);
    }

    /*
     * Reset the candidate cache because of the new expected recipients.
     */
    RESET_CANDIDATE_CACHE(transport);

    return (job);
}

/* qmgr_job_move_limits - move unused recipient slots to the next job */

void    qmgr_job_move_limits(QMGR_JOB * job)
{
    QMGR_TRANSPORT *transport = job->transport;
    QMGR_MESSAGE *message = job->message;
    QMGR_JOB *next = transport->job_next_unread;
    int     rcpt_unused,
            msg_rcpt_unused;

    /*
     * Find next unread job on the job list if necessary. Cache it for later.
     * This makes the amortized efficiency of this routine O(1) per job.
     */
    if (job == next) {
	for (next = next->transport_peers.next; next; next = next->transport_peers.next)
	    if (next->message->rcpt_offset != 0)
		break;
	transport->job_next_unread = next;
    }

    /*
     * Calculate the number of available unused slots.
     */
    rcpt_unused = job->rcpt_limit - job->rcpt_count;
    msg_rcpt_unused = message->rcpt_limit - message->rcpt_count;
    if (msg_rcpt_unused < rcpt_unused)
	rcpt_unused = msg_rcpt_unused;

    /*
     * Transfer the unused recipient slots back to the transport pool and to
     * the next not-fully-read job. Job's message limits are adjusted
     * accordingly.
     */
    if (rcpt_unused > 0) {
	job->rcpt_limit -= rcpt_unused;
	message->rcpt_limit -= rcpt_unused;
	transport->rcpt_unused += rcpt_unused;
	if (next != 0 && (rcpt_unused = transport->rcpt_unused) > 0) {
	    next->rcpt_limit += rcpt_unused;
	    next->message->rcpt_limit += rcpt_unused;
	    transport->rcpt_unused = 0;
	}
    }
}

/* qmgr_job_retire - remove the job from the job list while waiting for recipients to deliver */

static void qmgr_job_retire(QMGR_JOB * job)
{
    char   *myname = "qmgr_job_retire";
    QMGR_TRANSPORT *transport = job->transport;

    if (msg_verbose)
	msg_info("%s: %s", myname, job->message->queue_id);

    /*
     * Sanity checks.
     */
    if (job->stack_level != 0)
	msg_panic("%s: non-zero stack level (%d)", myname, job->stack_level);

    /*
     * Make sure this job is not cached as the next unread job for this
     * transport. The qmgr_entry_done() will make sure that the slots donated
     * by this job are moved back to the transport pool as soon as possible.
     */
    qmgr_job_move_limits(job);

    /*
     * Invalidate the candidate selection cache if necessary.
     */
    if (job == transport->candidate_cache
     || (transport->job_stack.next == 0 && job == transport->job_list.next))
	RESET_CANDIDATE_CACHE(transport);

    /*
     * Remove the job from the job list and mark it as retired.
     */
    QMGR_LIST_UNLINK(transport->job_list, QMGR_JOB *, job, transport_peers);
    job->stack_level = -1;
}

/* qmgr_job_free - release the job structure */

void    qmgr_job_free(QMGR_JOB * job)
{
    char   *myname = "qmgr_job_free";
    QMGR_MESSAGE *message = job->message;
    QMGR_TRANSPORT *transport = job->transport;

    if (msg_verbose)
	msg_info("%s: %s %s", myname, message->queue_id, transport->name);

    /*
     * Sanity checks.
     */
    if (job->rcpt_count)
	msg_panic("%s: non-zero recipient count (%d)", myname, job->rcpt_count);

    /*
     * Remove the job from the job stack if necessary.
     */
    if (job->stack_level > 0)
	qmgr_job_pop(job);

    /*
     * Return any remaining recipient slots back to the recipient slots pool.
     */
    qmgr_job_move_limits(job);
    if (job->rcpt_limit)
	msg_panic("%s: recipient slots leak (%d)", myname, job->rcpt_limit);

    /*
     * Invalidate the candidate selection cache if necessary.
     */
    if (job == transport->candidate_cache
     || (transport->job_stack.next == 0 && job == transport->job_list.next))
	RESET_CANDIDATE_CACHE(transport);

    /*
     * Unlink and discard the structure. Check if the job is still on the
     * transport job list or if it was already retired before unlinking it.
     */
    if (job->stack_level >= 0)
	QMGR_LIST_UNLINK(transport->job_list, QMGR_JOB *, job, transport_peers);
    QMGR_LIST_UNLINK(message->job_list, QMGR_JOB *, job, message_peers);
    htable_delete(transport->job_byname, message->queue_id, (void (*) (char *)) 0);
    htable_free(job->peer_byname, (void (*) (char *)) 0);
    myfree((char *) job);
}

/* qmgr_job_count_slots - maintain the delivery slot counters */

static void qmgr_job_count_slots(QMGR_JOB * current, QMGR_JOB * job)
{

    /*
     * Count the number of delivery slots used during the delivery of the
     * selected job. Also count the number of delivery slots available for
     * preemption.
     * 
     * However, suppress any slot counting if we didn't start regular delivery
     * of the selected job yet.
     */
    if (job == current || job->slots_used > 0) {
	job->slots_used++;
	job->slots_available++;
    }

    /*
     * If the selected job is not the current job, its chance to be chosen by
     * qmgr_job_candidate() has slightly changed. If we would like to make
     * the candidate cache completely transparent, we should invalidate it
     * now.
     * 
     * However, this case should usually happen only at "end of current job"
     * phase, when it's unlikely that the current job can be preempted
     * anyway.  And because it's likely to happen quite often then, we
     * intentionally don't reset the cache, to safe some cycles. Furthermore,
     * the cache times out every second anyway.
     */
#if 0
    if (job != current)
	RESET_CANDIDATE_CACHE(job->transport);
#endif
}

/* qmgr_job_candidate - find best job candidate for preempting given job */

static QMGR_JOB *qmgr_job_candidate(QMGR_JOB * current)
{
    QMGR_TRANSPORT *transport = current->transport;
    QMGR_JOB *job,
           *best_job = 0;
    float   score,
            best_score = 0.0;
    int     max_slots,
            max_needed_entries,
            max_total_entries;
    int     delay;
    time_t  now = event_time();

    /*
     * Fetch the result directly from the cache if the cache is still valid.
     * 
     * Note that we cache negative results too, so the cache must be invalidated
     * by resetting the cache time, not the candidate pointer itself.
     */
    if (transport->candidate_cache_time == now)
	return (transport->candidate_cache);

    /*
     * Estimate the minimum amount of delivery slots that can ever be
     * accumulated for the given job. All jobs that won't fit into these
     * slots are excluded from the candidate selection.
     */
    max_slots = (MIN_ENTRIES(current) - current->selected_entries
		 + current->slots_available) / transport->slot_cost;

    /*
     * Select the candidate with best time_since_queued/total_recipients
     * score. In addition to jobs which don't meet the max_slots limit, skip
     * also jobs which don't have any selectable entries at the moment.
     * 
     * By the way, the selection is reasonably resistant to OS time warping,
     * too.
     * 
     * However, don't bother searching if we can't find anything suitable
     * anyway.
     */
    if (max_slots > 0) {
	for (job = transport->job_list.next; job; job = job->transport_peers.next) {
	    if (job->stack_level != 0 || job == current)
		continue;
	    max_total_entries = MAX_ENTRIES(job);
	    max_needed_entries = max_total_entries - job->selected_entries;
	    delay = now - job->message->queued_time + 1;
	    if (max_needed_entries > 0 && max_needed_entries <= max_slots) {
		score = (float) delay / max_total_entries;
		if (score > best_score) {
		    best_score = score;
		    best_job = job;
		}
	    }

	    /*
	     * Stop early if the best score is as good as it can get.
	     */
	    if (delay <= best_score)
		break;
	}
    }

    /*
     * Cache the result for later use.
     */
    transport->candidate_cache = best_job;
    transport->candidate_cache_time = now;

    return (best_job);
}

/* qmgr_job_preempt - preempt large message with smaller one */

static QMGR_JOB *qmgr_job_preempt(QMGR_JOB * current)
{
    char   *myname = "qmgr_job_preempt";
    QMGR_TRANSPORT *transport = current->transport;
    QMGR_JOB *job;
    int     rcpt_slots;

    /*
     * Suppress preempting completely if the current job is not big enough to
     * accumulate even the mimimal number of slots required.
     * 
     * Also, don't look for better job candidate if there are no available slots
     * yet (the count can get negative due to the slot loans below).
     */
    if (current->slots_available <= 0
      || MAX_ENTRIES(current) < transport->min_slots * transport->slot_cost)
	return (current);

    /*
     * Find best candidate for preempting the current job.
     * 
     * Note that the function also takes care that the candidate fits within the
     * number of delivery slots which the current job is still able to
     * accumulate.
     */
    if ((job = qmgr_job_candidate(current)) == 0)
	return (current);

    /*
     * Sanity checks.
     */
    if (job == current)
	msg_panic("%s: attempt to preempt itself", myname);
    if (job->stack_level != 0)
	msg_panic("%s: already on the job stack (%d)", myname, job->stack_level);

    /*
     * Check if there is enough available delivery slots accumulated to
     * preempt the current job.
     * 
     * The slot loaning scheme improves the average message response time. Note
     * that the loan only allows the preemption happen earlier, though. It
     * doesn't affect how many slots have to be "paid" - the full number of
     * slots required has to be accumulated later before next preemption on
     * the same stack level can happen in either case.
     */
    if (current->slots_available / transport->slot_cost
	+ transport->slot_loan
	< (MAX_ENTRIES(job) - job->selected_entries)
	* transport->slot_loan_factor / 100.0)
	return (current);

    /*
     * Preempt the current job.
     */
    QMGR_LIST_PREPEND(transport->job_stack, job, stack_peers);
    job->stack_level = current->stack_level + 1;

    /*
     * Add part of extra recipient slots reserved for preempting jobs to the
     * new current job if necessary.
     * 
     * Note that transport->rcpt_unused is within <-rcpt_per_stack,0> in such
     * case.
     */
    if (job->message->rcpt_offset != 0) {
	rcpt_slots = (transport->rcpt_per_stack + transport->rcpt_unused + 1) / 2;
	job->rcpt_limit += rcpt_slots;
	job->message->rcpt_limit += rcpt_slots;
	transport->rcpt_unused -= rcpt_slots;
    }

    /*
     * Candidate cache must be reset because the current job has changed
     * completely.
     */
    RESET_CANDIDATE_CACHE(transport);

    if (msg_verbose)
	msg_info("%s: %s by %s", myname, current->message->queue_id,
		 job->message->queue_id);

    return (job);
}

/* qmgr_job_pop - remove the job from the job preemption stack */

static void qmgr_job_pop(QMGR_JOB * job)
{
    QMGR_TRANSPORT *transport = job->transport;
    QMGR_JOB *parent;

    if (msg_verbose)
	msg_info("qmgr_job_pop: %s", job->message->queue_id);

    /*
     * Adjust the number of delivery slots available to preempt job's parent.
     * 
     * Note that we intentionally do not adjust slots_used of the parent. Doing
     * so would decrease the maximum per message inflation factor if the
     * preemption appeared near the end of parent delivery.
     * 
     * For the same reason we do not adjust parent's slots_available if the
     * parent is not the original parent preempted by the selected job (i.e.,
     * the original parent job has already completed).
     * 
     * The special case when the head of the job list was preempted and then
     * delivered before the preempting job itself is taken care of too.
     * Otherwise we would decrease available slot counter of some job that
     * was not in fact preempted yet.
     */
    if (((parent = job->stack_peers.next) != 0
    || ((parent = transport->job_list.next) != 0 && parent->slots_used > 0))
	&& job->stack_level == parent->stack_level + 1)
	parent->slots_available -= job->slots_used * transport->slot_cost;

    /*
     * Invalidate the candidate selection cache if necessary.
     */
    if (job == transport->job_stack.next)
	RESET_CANDIDATE_CACHE(transport);

    /*
     * Remove the job from the job stack and reinitialize the slot counters.
     */
    QMGR_LIST_UNLINK(transport->job_stack, QMGR_JOB *, job, stack_peers);
    job->stack_level = 0;
    job->slots_used = 0;
    job->slots_available = 0;
}

/* qmgr_job_peer_select - select next peer suitable for delivery */

static QMGR_PEER *qmgr_job_peer_select(QMGR_JOB * job)
{
    QMGR_PEER *peer;
    QMGR_MESSAGE *message = job->message;

    if (HAS_ENTRIES(job) && (peer = qmgr_peer_select(job)) != 0)
	return (peer);

    /*
     * Try reading in more recipients. Note that we do not try to read them
     * as soon as possible as that would decrease the chance of per-site
     * recipient grouping. We waited until reading more is really necessary.
     */
    if (message->rcpt_offset != 0 && message->rcpt_limit > message->rcpt_count) {
	qmgr_message_realloc(message);
	if (HAS_ENTRIES(job))
	    return (qmgr_peer_select(job));
    }
    return (0);
}

/* qmgr_job_entry_select - select next entry suitable for delivery */

QMGR_ENTRY *qmgr_job_entry_select(QMGR_TRANSPORT *transport)
{
    QMGR_JOB *job,
           *current,
           *next;
    QMGR_PEER *peer;
    QMGR_ENTRY *entry;

    /*
     * Select the "current" job.
     */
    if ((current = transport->job_stack.next) == 0
	&& (current = transport->job_list.next) == 0)
	return (0);

    /*
     * Exercise the preempting algorithm if enabled.
     * 
     * The slot_cost equal to 1 causes the algorithm to degenerate and is
     * therefore disabled too.
     */
    if (transport->slot_cost >= 2)
	current = qmgr_job_preempt(current);

    /*
     * Select next entry suitable for delivery. First check the stack of
     * preempting jobs, then the list of all remaining jobs in FIFO order.
     * 
     * Note that although the loops may look inefficient, they only serve as a
     * recovery mechanism when an entry of the current job itself can't be
     * selected due peer concurrency restrictions. In most cases some entry
     * of the current job itself is selected.
     * 
     * Note that both loops also take care of getting the "stall" current job
     * (job with no entries currently available) out of the way if necessary.
     * Stall jobs can appear in case of multi-transport messages whose
     * recipients don't fit in-core at once. Some jobs created by such
     * message may have only few recipients and would block the job queue
     * until all other jobs of the message are delivered. Trying to read in
     * more recipients of such jobs each selection would also break the per
     * peer recipient grouping of the other jobs. That's why we retire such
     * jobs below.
     */
    for (job = transport->job_stack.next; job; job = next) {
	next = job->stack_peers.next;
	if ((peer = qmgr_job_peer_select(job)) != 0) {
	    entry = qmgr_entry_select(peer);
	    qmgr_job_count_slots(current, job);

	    /*
	     * In case we selected the very last job entry, remove the job
	     * from the job stack and the job list right now.
	     * 
	     * This action uses the assumption that once the job entry has been
	     * selected, it can be unselected only before the message ifself
	     * is deferred. Thus the job with all entries selected can't
	     * re-appear with more entries available for selection again
	     * (without reading in more entries from the queue file, which in
	     * turn invokes qmgr_job_obtain() which re-links the job back on
	     * the list if necessary).
	     * 
	     * Note that qmgr_job_move_limits() transfers the recipients slots
	     * correctly even if the job is unlinked from the job list thanks
	     * to the job_next_unread caching.
	     */
	    if (!HAS_ENTRIES(job) && job->message->rcpt_offset == 0) {
		qmgr_job_pop(job);
		qmgr_job_retire(job);
	    }
	    return (entry);
	} else if (job == current && !HAS_ENTRIES(job)) {
	    qmgr_job_pop(job);
	    qmgr_job_retire(job);
	    current = next ? next : transport->job_list.next;
	}
    }

    /*
     * Try the regular job list if there is nothing (suitable) on the job
     * stack.
     */
    for (job = transport->job_list.next; job; job = next) {
	next = job->transport_peers.next;
	if (job->stack_level != 0)
	    continue;
	if ((peer = qmgr_job_peer_select(job)) != 0) {
	    entry = qmgr_entry_select(peer);
	    qmgr_job_count_slots(current, job);

	    /*
	     * In case we selected the very last job entry, remove the job
	     * from the job list right away.
	     */
	    if (!HAS_ENTRIES(job) && job->message->rcpt_offset == 0)
		qmgr_job_retire(job);
	    return (entry);
	} else if (job == current && !HAS_ENTRIES(job)) {
	    qmgr_job_retire(job);
	    current = next;
	}
    }
    return (0);
}
