/*++
/* NAME
/*	postscreen_dict 3
/* SUMMARY
/*	postscreen table access wrappers
/* SYNOPSIS
/*	#include <postscreen.h>
/*
/*	int	psc_addr_match_list_match(match_list, client_addr)
/*	ADDR_MATCH_LIST *match_list;
/*	const char *client_addr;
/*
/*	const char *psc_cache_lookup(DICT_CACHE *cache, const char *key)
/*	DICT_CACHE *cache;
/*	const char *key;
/*
/*	void	psc_cache_update(cache, key, value)
/*	DICT_CACHE *cache;
/*	const char *key;
/*	const char *value;
/* DESCRIPTION
/*	This module implements wrappers around time-critical table
/*	access functions.  The functions log a warning when table
/*	access takes a non-trivial amount of time.
/*
/*	psc_addr_match_list_match() is a wrapper around
/*	addr_match_list_match().
/*
/*	psc_cache_lookup() and psc_cache_update() are wrappers around
/*	the corresponding dict_cache() methods.
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

/* Application-specific. */

#include <postscreen.h>

 /*
  * Monitor time-critical operations.
  */
#define PSC_GET_TIME_BEFORE_LOOKUP \
    struct timeval _before, _after; \
    DELTA_TIME _delta; \
    GETTIMEOFDAY(&_before);

#define PSC_DELTA_MS(d) ((d).dt_sec * 1000 + (d).dt_usec / 1000)

#define PSC_CHECK_TIME_AFTER_LOOKUP(table, action) \
    GETTIMEOFDAY(&_after); \
    PSC_CALC_DELTA(_delta, _after, _before); \
    if (_delta.dt_sec > 1 || _delta.dt_usec > 100000) \
        msg_warn("%s: %s %s took %d ms", \
                 myname, (table), (action), PSC_DELTA_MS(_delta));

/* psc_addr_match_list_match - time-critical address list lookup */

int     psc_addr_match_list_match(ADDR_MATCH_LIST *addr_list,
				          const char *addr_str)
{
    const char *myname = "psc_addr_match_list_match";
    int     result;

    PSC_GET_TIME_BEFORE_LOOKUP;
    result = addr_match_list_match(addr_list, addr_str);
    PSC_CHECK_TIME_AFTER_LOOKUP("address list", "lookup");
    return (result);
}

/* psc_cache_lookup - time-critical cache lookup */

const char *psc_cache_lookup(DICT_CACHE *cache, const char *key)
{
    const char *myname = "psc_cache_lookup";
    const char *result;

    PSC_GET_TIME_BEFORE_LOOKUP;
    result = dict_cache_lookup(cache, key);
    PSC_CHECK_TIME_AFTER_LOOKUP(dict_cache_name(cache), "lookup");
    return (result);
}

/* psc_cache_update - time-critical cache update */

void    psc_cache_update(DICT_CACHE *cache, const char *key, const char *value)
{
    const char *myname = "psc_cache_update";

    PSC_GET_TIME_BEFORE_LOOKUP;
    dict_cache_update(cache, key, value);
    PSC_CHECK_TIME_AFTER_LOOKUP(dict_cache_name(cache), "update");
}
