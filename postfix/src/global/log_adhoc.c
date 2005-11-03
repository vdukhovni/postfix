/*++
/* NAME
/*	log_adhoc 3
/* SUMMARY
/*	ad-hoc delivery event logging
/* SYNOPSIS
/*	#include <log_adhoc.h>
/*
/*	void	log_adhoc(id, stats, recipient, relay, dsn, status)
/*	const char *id;
/*	MSG_STATS *stats;
/*	RECIPIENT *recipient;
/*	const char *relay;
/*	DSN *dsn;
/*	const char *status;
/* DESCRIPTION
/*	This module logs delivery events in an ad-hoc manner.
/*
/*	log_adhoc() appends a record to the mail logfile
/*
/*	Arguments:
/* .IP queue
/*	The message queue name of the original message file.
/* .IP id
/*	The queue id of the original message file.
/* .IP stats
/*	Time stamps from different message delivery stages
/*	and session reuse count.
/* .IP recipient
/*	Recipient information. See recipient_list(3).
/* .IP sender
/*	The sender envelope address.
/* .IP relay
/*	Host we could (not) talk to.
/* .IP status
/*	bounced, deferred, sent, and so on.
/* .IP dsn
/*	Delivery status information. See dsn(3).
/* BUGS
/*	Should be replaced by routines with an attribute-value based
/*	interface instead of an interface that uses a rigid argument list.
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

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

/* Utility library. */

#include <msg.h>
#include <vstring.h>

/* Global library. */

#include <log_adhoc.h>

/* log_adhoc - ad-hoc logging */

void    log_adhoc(const char *id, MSG_STATS *stats, RECIPIENT *recipient,
		          const char *relay, DSN *dsn,
		          const char *status)
{
    static VSTRING *buf;
    int     delay;
    int  pdelay;			/* time before queue manager */
    struct timeval adelay;		/* queue manager latency */
    struct timeval sdelay;		/* connection set-up latency */
    struct timeval xdelay;		/* transmission latency */
    struct timeval now;

    /*
     * Alas, we need an intermediate buffer for the pre-formatted result.
     * There are several optional fields, and we want to tweak some
     * formatting depending on delay values.
     */
    if (buf == 0)
	buf = vstring_alloc(100);

    /*
     * First, general information that identifies the transaction.
     */
    vstring_sprintf(buf, "%s: to=<%s>", id, recipient->address);
    if (recipient->orig_addr && *recipient->orig_addr
	&& strcasecmp(recipient->address, recipient->orig_addr) != 0)
	vstring_sprintf_append(buf, ", orig_to=<%s>", recipient->orig_addr);
    vstring_sprintf_append(buf, ", relay=%s", relay);

    /*
     * Next, performance statistics.
     * 
     * Use wall clock time to compute pdelay (before active queue latency) if
     * there is no time stamp for active queue entry. This happens when mail
     * is bounced by the cleanup server.
     * 
     * Otherwise, use wall clock time to compute adelay (active queue latency)
     * if there is no time stamp for hand-off to delivery agent. This happens
     * when mail was deferred or bounced by the queue manager.
     * 
     * Otherwise, use wall clock time to compute xdelay (message transfer
     * latency) if there is no time stamp for delivery completion. In the
     * case of multi-recipient deliveries the delivery agent may specify the
     * delivery completion time, so that multiple recipient records show the
     * same delay values.
     * 
     * Don't compute the sdelay (connection setup latency) if there is no time
     * stamp for connection setup completion.
     * 
     * Instead of floating point, use integer math where practical.
     */
#define DELTA(x, y, z) \
    do { \
	(x).tv_sec = (y).tv_sec - (z).tv_sec; \
	(x).tv_usec = (y).tv_usec - (z).tv_usec; \
	if ((x).tv_usec < 0) { \
	    (x).tv_usec += 1000000; \
	    (x).tv_sec -= 1; \
	} \
	if ((x).tv_sec < 0) \
	    (x).tv_sec = (x).tv_usec = 0; \
    } while (0)

    if (stats->deliver_done.tv_sec)
	now = stats->deliver_done;
    else
	GETTIMEOFDAY(&now);
    delay = now.tv_sec - stats->incoming_arrival;
    adelay.tv_sec = adelay.tv_usec =
	sdelay.tv_sec = sdelay.tv_usec =
	xdelay.tv_sec = xdelay.tv_usec = 0;
    if (stats->active_arrival.tv_sec) {
	pdelay = stats->active_arrival.tv_sec - stats->incoming_arrival;
	if (stats->agent_handoff.tv_sec) {
	    DELTA(adelay, stats->agent_handoff, stats->active_arrival);
	    if (stats->conn_setup_done.tv_sec) {
		DELTA(sdelay, stats->conn_setup_done, stats->agent_handoff);
		DELTA(xdelay, now, stats->conn_setup_done);
	    } else {
		/* Non-network delivery agent. */
		DELTA(xdelay, now, stats->agent_handoff);
	    }
	} else {
	    /* No delivery agent. */
	    DELTA(adelay, now, stats->active_arrival);
	}
    } else {
	/* No queue manager. */
	pdelay = now.tv_sec - stats->incoming_arrival;
    }
    if (stats->reuse_count > 0)
	vstring_sprintf_append(buf, ", conn_use=%d", stats->reuse_count + 1);

#define PRETTY_FORMAT(b, x) \
    do { \
	if ((x).tv_sec > 0 || (x).tv_usec < 10000) { \
	    vstring_sprintf_append((b), "/%ld", (long) (x).tv_sec); \
	} else { \
	    vstring_sprintf_append((b), "/%.1g", \
		    (x).tv_sec + (x).tv_usec / 1000000.0); \
	} \
    } while (0)

    vstring_sprintf_append(buf, ", delay=%d, delays=%d", delay, pdelay);
    PRETTY_FORMAT(buf, adelay);
    PRETTY_FORMAT(buf, sdelay);
    PRETTY_FORMAT(buf, xdelay);

    /*
     * Finally, the delivery status.
     */
    vstring_sprintf_append(buf, ", dsn=%s, status=%s (%s)",
			   dsn->status, status, dsn->reason);

    /*
     * Ship it off.
     */
    msg_info("%s", vstring_str(buf));
}
