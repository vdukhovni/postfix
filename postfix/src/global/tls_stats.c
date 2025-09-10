/*++
/* NAME
/*	tls_stats 3
/* SUMMARY
/*	manage TLS per-feature policy compliance status
/* SYNOPSIS
/*	#include <tls_stats.h>
/*
/*	TLS_STATS *tls_stats_create(void)
/*
/*	void	tls_stats_revert(TLS_STATS *tstats)
/*
/*	void	tls_stats_free(TLS_STATS *tstats)
/*
/*	void	tls_stat_activate(
/*	TLS_STATS *tstats,
/*	int	idx,
/*	const char *name,
/*	bool	enforce)
/*
/*	void	tls_stat_decide(
/*	TLS_STATS *tstats,
/*	int	idx,
/*	const char *name,
/*	int	status,
/*	bool	enforce)
/*
/*	const TLS_STAT *tls_stats_access(
/*	TLS_STATS *tstats,
/*	int	idx)
/*
/*	int	tls_stats_used(TLS_STATS *tstats)
/* DESCRIPTION
/*	This module maintains for each active TLS feature whether
/*	the current outbound SMTP connection satisfies the policy
/*	requirements for that feature. For example, whether a server
/*	certificate matches DANE or STS requirements.
/*
/*	tls_stats_create() creates one TLS_STATS instance with all status
/*	information set to TLS_STAT_INACTIVE.
/*
/*	tls_stats_revert() reverts changes after tls_stats_create().
/*
/*	tls_stats_free() recycles storage for a TLS_STATS instance.
/*
/*	Specific status information is accessed with an index. Valid
/*	indices are 0 or 1 (the caller decides their purpose).
/*
/*	The enforcement level of a feature is either TLS_STAT_ENF_FULL
/*	(full enforcement) or TLS_STAT_ENF_RELAXED (some requirements
/*	are relaxed).
/*
/*	tls_stat_activate() changes the status in tstats at index idx
/*	from TLS_STAT_INACTIVE to TLS_STAT_UNDECIDED, and updates the
/*	name (pointer copy) and 'enforce' level. Calls with an invalid
/*	index result in a panic(), and calls with an already active
/*	index result in a warning.
/*
/*	tls_stat_decide() updates the status in tstats at index idx from
/*	TLS_STAT_UNDECIDED to TLS_STAT_COMPLIANT, TLS_STAT_VIOLATION,
/*	or TLS_STAT_DISABLED, and updates its name and enforcement
/*	level. Calls with an invalid index or an unexpected decision
/*	status result in a panic(), and calls with an inactive or
/*	already decided index status result in a warning.
/*
/*	tls_stats_access() returns a const pointer to the status
/*	information in tstats at index idx.
/*
/*	tls_stats_used() returns the number of activated categories for
/*	its argument.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

#ifdef USE_TLS

 /*
  * System library.
  */
#include <sys_defs.h>

 /*
  * Utility library.
  */
#include <msg.h>
#include <mymalloc.h>

 /*
  * Global library.
  */
#include <tls_stats.h>

/* tls_stats_create - create an all-inactive TLS_STATS instance */

TLS_STATS *tls_stats_create(void)
{
    TLS_STATS *tstats;
    TLS_STAT *tp;

#define TLS_STAT_INIT(tp) do { \
	(tp)->name = 0; \
	(tp)->status = TLS_STAT_INACTIVE; \
	(tp)->enforce = 0; \
    } while (0);

    tstats = (TLS_STATS *) mymalloc(sizeof(*tstats));
    tstats->used = 0;
    for (tp = tstats->st; tp < tstats->st + TLS_STATS_SIZE; tp++)
	TLS_STAT_INIT(tp);
    return tstats;
}

/* tls_stats_revert - revert changes after tls_stats_create() */

void    tls_stats_revert(TLS_STATS *tstats)
{
    TLS_STAT *tp;

    tstats->used = 0;
    for (tp = tstats->st; tp < tstats->st + TLS_STATS_SIZE; tp++)
	if (tp->status != TLS_STAT_INACTIVE)
	    TLS_STAT_INIT(tp);
}

/* tls_stats_free - TLS_STATS destructor */

void    tls_stats_free(TLS_STATS *tstats)
{
    myfree(tstats);
}

/* tls_stat_activate - activate status at index */

void    tls_stat_activate(TLS_STATS *tstats, int idx, const char *name,
			          bool enforce)
{
    TLS_STAT *tls_stat;

    if (idx < 0 || idx >= TLS_STATS_SIZE)
	msg_panic("%s: bad index: %d", __func__, idx);
    tls_stat = tstats->st + idx;
    if (tls_stat->status != TLS_STAT_INACTIVE)
	msg_warn("%s: already active TLS_STAT at index %d", __func__, idx);
    tls_stat->name = name;
    tls_stat->status = TLS_STAT_UNDECIDED;
    tls_stat->enforce = enforce;
    tstats->used += 1;
}

/* tls_stat_decide - update undecided status at index */

extern void tls_stat_decide(TLS_STATS *tstats, int idx, const char *name,
			            int status, bool enforce)
{
    TLS_STAT *tls_stat;

    if (status != TLS_STAT_VIOLATION && status != TLS_STAT_COMPLIANT
	&& status != TLS_STAT_DISABLED)
	msg_panic("%s: bad new status: %d", __func__, status);
    if (idx < 0 || idx >= TLS_STATS_SIZE)
	msg_panic("%s: bad index: %d", __func__, idx);
    tls_stat = tstats->st + idx;
    if (tls_stat->status != TLS_STAT_UNDECIDED)
	msg_warn("%s: unexpected status %d at index %d",
		 __func__, tls_stat->status, idx);
    tls_stat->name = name;
    tls_stat->status = status;
    tls_stat->enforce = enforce;
}

/* tls_stat_access - peek at specific TLS_STAT instance. */

const TLS_STAT *tls_stat_access(const TLS_STATS *tstats, int idx)
{
    const TLS_STAT *tls_stat;

    if (idx < 0 || idx >= TLS_STATS_SIZE)
	msg_panic("%s: bad index: %d", __func__, idx);
    tls_stat = tstats->st + idx;
    return (tls_stat);
}

#endif					/* USE_TLS */
