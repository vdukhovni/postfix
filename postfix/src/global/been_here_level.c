/*++
/* NAME
/*	been_here_level 3
/* SUMMARY
/*	detect repeated occurrence of string
/* SYNOPSIS
/*	#include <been_here.h>
/*
/*	int	been_here_level_fixed(dup_filter, level, string)
/*	BH_TABLE *dup_filter;
/*	int	level;
/*	char	*string;
/*
/*	int	been_here_level(dup_filter, level, format, ...)
/*	BH_TABLE *dup_filter;
/*	int	level;
/*	char	*format;
/*
/*	int	been_here_level_check_fixed(dup_filter, level, string)
/*	BH_TABLE *dup_filter;
/*	int	level;
/*	char	*string;
/*
/*	int	been_here_level_check(dup_filter, level, format, ...)
/*	BH_TABLE *dup_filter;
/*	int	level;
/*	char	*format;
/* DESCRIPTION
/*	This module implements a simple filter to detect repeated
/*	occurrences of character strings. Each string has associated with
/*	an integer level, the meaning of which is left up to the application.
/*	Otherwise the code is like been_here(3).
/*
/*	been_here_level_fixed() looks up a fixed string in the given table, and
/*	makes an entry in the table if the string was not found. The result
/*	is >= 0 if the string was found, -1 otherwise.
/*
/*	been_here_level() formats its arguments, looks up the result in the
/*	given table, and makes an entry in the table if the string was
/*	not found. The result is >= 0 if the formatted result was
/*	found, -1 otherwise.
/*
/*	been_here_level_check_fixed() and been_here_level_check() are similar
/*	but do not update the duplicate filter.
/*
/*	Arguments:
/* .IP size
/*	Upper bound on the table size; at most \fIsize\fR strings will
/*	be remembered.  Specify a value <= 0 to disable the upper bound.
/* .IP flags
/*	Requests for special processing. Specify the bitwise OR of zero
/*	or more flags:
/* .RS
/* .IP BH_FLAG_FOLD
/*	Enable case-insensitive lookup.
/* .IP BH_FLAG_NONE
/*	A manifest constant that requests no special processing.
/* .RE
/* .IP dup_filter
/*	The table with remembered names
/* .IP level
/*	The value that will be returned when the string is found by either
/*	been_here_level() or been_here_check_level(). This value must be
/*	>= 0.
/* .IP string
/*	Fixed search string.
/* .IP format
/*	Format for building the search string.
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

#include "sys_defs.h"
#include <stdlib.h>			/* 44BSD stdarg.h uses abort() */
#include <stdarg.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <htable.h>
#include <vstring.h>
#include <stringops.h>

/* Global library. */

#include "been_here.h"

/* been_here_level - duplicate detector with finer control */

int     been_here_level(BH_TABLE *dup_filter, int level, const char *fmt,...)
{
    VSTRING *buf = vstring_alloc(100);
    int     status;
    va_list ap;

    /*
     * Construct the string to be checked.
     */
    va_start(ap, fmt);
    vstring_vsprintf(buf, fmt, ap);
    va_end(ap);

    /*
     * Do the duplicate check.
     */
    status = been_here_level_fixed(dup_filter, level, vstring_str(buf));

    /*
     * Cleanup.
     */
    vstring_free(buf);
    return (status);
}

/* been_here_level_fixed - duplicate detector */

int     been_here_level_fixed(BH_TABLE *dup_filter, int level, const char *string)
{
    char   *folded_string;
    const char *lookup_key;
    int     status;
    HTABLE_INFO *info;

    /*
     * Sanity check.
     */
    if (level < 0)
	msg_panic("been_here_level_fixed: bad level %d", level);

    /*
     * Special processing: case insensitive lookup.
     */
    if (dup_filter->flags & BH_FLAG_FOLD) {
	folded_string = mystrdup(string);
	lookup_key = lowercase(folded_string);
    } else {
	folded_string = 0;
	lookup_key = string;
    }

    /*
     * Do the duplicate check.
     */
    if ((info = htable_locate(dup_filter->table, lookup_key)) != 0) {
	status = CAST_CHAR_PTR_TO_INT(info->value);
    } else {
	if (dup_filter->limit <= 0
	    || dup_filter->limit > dup_filter->table->used)
	    htable_enter(dup_filter->table, lookup_key,
			 CAST_INT_TO_CHAR_PTR(level));
	status = -1;
    }
    if (msg_verbose)
	msg_info("been_here_level: %s: %d", string, status);

    /*
     * Cleanup.
     */
    if (folded_string)
	myfree(folded_string);

    return (status);
}

/* been_here_level_check - query duplicate detector with finer control */

int     been_here_level_check(BH_TABLE *dup_filter, const char *fmt,...)
{
    VSTRING *buf = vstring_alloc(100);
    int     status;
    va_list ap;

    /*
     * Construct the string to be checked.
     */
    va_start(ap, fmt);
    vstring_vsprintf(buf, fmt, ap);
    va_end(ap);

    /*
     * Do the duplicate check.
     */
    status = been_here_level_check_fixed(dup_filter, vstring_str(buf));

    /*
     * Cleanup.
     */
    vstring_free(buf);
    return (status);
}

/* been_here_level_check_fixed - query duplicate detector */

int     been_here_level_check_fixed(BH_TABLE *dup_filter, const char *string)
{
    char   *folded_string;
    const char *lookup_key;
    int     status;
    HTABLE_INFO *info;

    /*
     * Special processing: case insensitive lookup.
     */
    if (dup_filter->flags & BH_FLAG_FOLD) {
	folded_string = mystrdup(string);
	lookup_key = lowercase(folded_string);
    } else {
	folded_string = 0;
	lookup_key = string;
    }

    /*
     * Do the duplicate check.
     */
    if ((info = htable_locate(dup_filter->table, lookup_key)) != 0)
	status = (-1);
    else
	status = CAST_CHAR_PTR_TO_INT(info->value);
    if (msg_verbose)
	msg_info("been_here_level_check: %s: %d", string, status);

    /*
     * Cleanup.
     */
    if (folded_string)
	myfree(folded_string);

    return (status);
}
