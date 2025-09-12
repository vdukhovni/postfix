#ifndef _TLS_STATS_H_INCLUDED_
#define _TLS_STATS_H_INCLUDED_

/*++
/* NAME
/*	tls_stats 3h
/* SUMMARY
/*	manage TLS per-feature policy compliance status
/* SYNOPSIS
/*	#include <tls_stats.h>
/* DESCRIPTION
/* .nf

 /*
  * TODO(wietse) adopt C99 bool.
  */
#include <mail_params.h>

 /*
  * External interface.
  */
typedef struct TLS_STAT {
    const char *target_name;		/* Human-readable feature name */
    const char *final_name;		/* Human-readable feature name */
    int     status;			/* See below */
    bool    enforce;			/* See below */
} TLS_STAT;

#define TLS_STAT_INACTIVE	0	/* No data */
#define TLS_STAT_UNDECIDED	1	/* Pending decision */
#define TLS_STAT_VIOLATION	2	/* Definitely did not meet policy */
#define TLS_STAT_COMPLIANT	3	/* Definitely did meet policy */
#define TLS_STAT_ENF_FULL	1	/* Full enforcement */
#define TLS_STAT_ENF_RELAXED	0	/* Relaxed enforcement */

 /*
  * Wrap it in a structure for sanity-checked access.
  */
#define TLS_STATS_SIZE	2		/* TLS level and REQUIRETLS */

typedef struct TLS_STATS {
    int     used;
    TLS_STAT st[TLS_STATS_SIZE];
} TLS_STATS;

#ifdef USE_TLS

extern TLS_STATS *tls_stats_create(void);
extern void tls_stats_revert(TLS_STATS *);
extern void tls_stats_free(TLS_STATS *);

#define tls_stats_used(t) ((t)->used)
extern void tls_stat_activate(TLS_STATS *, int, const char *, bool);
extern void tls_stat_decide(TLS_STATS *, int, const char *, int);
extern const TLS_STAT *tls_stat_access(const TLS_STATS *, int);

#define NO_TLS_STATS	((TLS_STATS *) 0)

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

#endif					/* USE_TLS */
#endif					/* _TLS_STATS_H_INCLUDED_ */
