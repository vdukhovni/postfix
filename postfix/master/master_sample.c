/*++
/* NAME
/*	master_sample 3
/* SUMMARY
/*	Postfix master - statistics sampling
/* SYNOPSIS
/*	#include "master.h"
/*
/*	void	master_sample_start()
/*
/*	void	master_sample_stop(serv)
/* DESCRIPTION
/*	This module samples statistics at one-minute intervals.
/*	Currently, it maintains the average process counts.
/*
/*	master_sample_start() resets the statistics and starts
/*	the statistics sampling process.
/*
/*	master_sample_start() stops the statistics sampling process.
/* DIAGNOSTICS
/* BUGS
/* SEE ALSO
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

/* System libraries. */

#include <sys_defs.h>

/* Utility library. */

#include <events.h>
#include <msg.h>

/* Application-specific. */

#include "master.h"

/* master_sample_action - take sample */

static void master_sample_action(int unused_event, char *unused_context)
{
    MASTER_SERV *serv;

#define TSAMPLE	60
#define NSAMPLE 5

    /*
     * Update the process limit for services that have different peak/average
     * concurrency limits. Gradually change from idle mode (allowing peak
     * concurrency) to stress mode (long-term average process limit).
     */
    for (serv = master_head; serv != 0; serv = serv->next) {
	if (serv->max_proc_pk == 0 || serv->max_proc_avg == 0
	    || serv->max_proc_pk == serv->max_proc_avg)
	    continue;
	serv->total_proc_avg +=
	    (serv->total_proc - serv->total_proc_avg) / NSAMPLE;
	if (msg_verbose)
	    msg_info("%s total/avg %d/%.1f",
		     serv->name, serv->total_proc, serv->total_proc_avg);
	if (serv->max_proc_pk < serv->max_proc_avg)
	    msg_panic("%s: process limit botch: %d < %d",
		      serv->name, serv->max_proc_pk, serv->max_proc_avg);
	if (serv->total_proc_avg >= serv->max_proc_avg)
	    serv->max_proc = serv->max_proc_avg;
	else
	    serv->max_proc = serv->max_proc_pk
		- serv->total_proc_avg * (serv->max_proc_pk - serv->max_proc_avg) / (double) serv->max_proc_avg;
    }
    event_request_timer(master_sample_action, (char *) 0, TSAMPLE);
}

/* master_sample_start - start sampling */

void    master_sample_start(void)
{
    MASTER_SERV *serv;

    for (serv = master_head; serv != 0; serv = serv->next)
	serv->total_proc_avg = 0;
    event_request_timer(master_sample_action, (char *) 0, TSAMPLE);
}

/* master_sample_stop - stop sampling */

void    master_sample_stop(void)
{
    event_cancel_timer(master_sample_action, (char *) 0);
}
