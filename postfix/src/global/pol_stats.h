#ifndef _POL_STATS_H_INCLUDED_
#define _POL_STATS_H_INCLUDED_

/*++
/* NAME
/*	tls_stats 3h
/* SUMMARY
/*	manage TLS per-feature policy compliance status
/* SYNOPSIS
/*	#include <pol_stats.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <vstring.h>

 /*
  * TODO(wietse) adopt C99 bool.
  */
#include <mail_params.h>

 /*
  * External interface.
  */
typedef struct POL_STAT {
    const char *init_name;		/* Human-readable feature name */
    const char *final_name;		/* Human-readable feature name */
    int     status;			/* See below */
    bool    enforce;			/* See below */
} POL_STAT;

#define POL_STAT_INACTIVE	0	/* No data */
#define POL_STAT_UNDECIDED	1	/* Pending decision */
#define POL_STAT_VIOLATION	2	/* Definitely did not meet policy */
#define POL_STAT_COMPLIANT	3	/* Definitely did meet policy */
#define POL_STAT_ENF_FULL	1	/* Full enforcement */
#define POL_STAT_ENF_RELAXED	0	/* Relaxed enforcement */

 /*
  * Wrap it in a structure for sanity-checked access.
  */
#define POL_STATS_SIZE	2		/* TLS level and REQUIRETLS */

typedef struct POL_STATS {
    int     used;
    POL_STAT st[POL_STATS_SIZE];
} POL_STATS;

#ifdef USE_TLS

extern POL_STATS *pol_stats_create(void);
extern void pol_stats_revert(POL_STATS *);
extern void pol_stats_free(POL_STATS *);

#define pol_stats_used(t) ((t)->used)
extern void pol_stat_activate(POL_STATS *, int, const char *, bool);
extern void pol_stat_decide(POL_STATS *, int, const char *, int);
extern void pol_stats_format(VSTRING *, const POL_STATS *);

#define NO_TLS_STATS	((POL_STATS *) 0)

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

#endif					/* USE_TLS */
#endif					/* _POL_STATS_H_INCLUDED_ */
