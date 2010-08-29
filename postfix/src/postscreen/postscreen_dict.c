/*++
/* NAME
/*	postscreen_dict 3
/* SUMMARY
/*	postscreen table access wrappers
/* SYNOPSIS
/*	#include <postscreen.h>
/*
/*	int	ps_addr_match_list_match(match_list, client_addr)
/*	ADDR_MATCH_LIST *match_list;
/*	const char *client_addr;
/*
/*	const char *ps_cache_lookup(DICT_CACHE *cache, const char *key)
/*	DICT_CACHE *cache;
/*	const char *key;
/*
/*	void	ps_cache_update(cache, key, value)
/*	DICT_CACHE *cache;
/*	const char *key;
/*	const char *value;
/* DESCRIPTION
/*	This module implements wrappers around time-critical table
/*	access functions.  The functions log a warning when table
/*	access takes a non-trivial amount of time.
/*
/*	ps_addr_match_list_match() is a wrapper around
/*	addr_match_list_match().
/*
/*	ps_cache_lookup() and ps_cache_update() are wrappers around
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
#define PS_GET_TIME_BEFORE_LOOKUP \
    struct timeval _before, _after; \
    DELTA_TIME _delta; \
    GETTIMEOFDAY(&_before);

#define PS_DELTA_MS(d) ((d).dt_sec * 1000 + (d).dt_usec / 1000)

#define PS_CHECK_TIME_AFTER_LOOKUP(table, action) \
    GETTIMEOFDAY(&_after); \
    PS_CALC_DELTA(_delta, _after, _before); \
    if (_delta.dt_sec > 1 || _delta.dt_usec > 100000) \
        msg_warn("%s: %s %s took %d ms", \
                 myname, (table), (action), PS_DELTA_MS(_delta));

/* ps_addr_match_list_match - time-critical address list lookup */

int     ps_addr_match_list_match(ADDR_MATCH_LIST *addr_list,
				         const char *addr_str)
{
    const char *myname = "ps_addr_match_list_match";
    int     result;

    PS_GET_TIME_BEFORE_LOOKUP;
    result = addr_match_list_match(addr_list, addr_str);
    PS_CHECK_TIME_AFTER_LOOKUP("address list", "lookup");
    return (result);
}

/* ps_cache_lookup - time-critical cache lookup */

const char *ps_cache_lookup(DICT_CACHE *cache, const char *key)
{
    const char *myname = "ps_cache_lookup";
    const char *result;

    PS_GET_TIME_BEFORE_LOOKUP;
    result = dict_cache_lookup(cache, key);
    PS_CHECK_TIME_AFTER_LOOKUP(dict_cache_name(cache), "lookup");
    return (result);
}

/* ps_cache_update - time-critical cache update */

void    ps_cache_update(DICT_CACHE *cache, const char *key, const char *value)
{
    const char *myname = "ps_cache_update";

    PS_GET_TIME_BEFORE_LOOKUP;
    dict_cache_update(cache, key, value);
    PS_CHECK_TIME_AFTER_LOOKUP(dict_cache_name(cache), "update");
}
