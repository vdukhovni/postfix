/*++
/* NAME
/*	postscreen_tests 3
/* SUMMARY
/*	postscreen tests timestamp/flag bulk support
/* SYNOPSIS
/*	#include <postscreen.h>
/*
/*	void	PS_INIT_TESTS(state)
/*	PS_STATE *state;
/*
/*	void	ps_new_tests(state)
/*	PS_STATE *state;
/*
/*	void	ps_parse_tests(state, stamp_text, time_value)
/*	PS_STATE *state;
/*	const char *stamp_text;
/*	time_t time_value;
/*
/*	char	*ps_print_tests(buffer, state)
/*	VSTRING	*buffer;
/*	PS_STATE *state;
/*
/*	char	*ps_print_grey_key(buffer, client, helo, sender, rcpt)
/*	VSTRING	*buffer;
/*	const char *client;
/*	const char *helo;
/*	const char *sender;
/*	const char *rcpt;
/* DESCRIPTION
/*	The functions in this module overwrite the per-test expiration
/*	time stamps and all flags bits.  Some functions are implemented
/*	as unsafe macros, meaning they evaluate one ore more arguments
/*	multiple times.
/*
/*	PS_INIT_TESTS() is an unsafe macro that sets the per-test
/*	expiration time stamps to PS_TIME_STAMP_INVALID, and that
/*	zeroes all the flags bits. These values are not meant to
/*	be stored into the postscreen(8) cache.
/*
/*	ps_new_tests() sets all test expiration time stamps to
/*	PS_TIME_STAMP_NEW, and overwrites all flags bits. Only
/*	enabled tests are flagged with PS_STATE_FLAG_TODO; the
/*	object is flagged with PS_STATE_FLAG_NEW.
/*
/*	ps_parse_tests() parses a cache file record and overwrites
/*	all flags bits. Tests are considered "expired" when they
/*	would be expired at the specified time value. Only enabled
/*	tests are flagged as "expired"; the object is flagged as
/*	"new" if some enabled tests have "new" time stamps.
/*
/*	ps_print_tests() creates a cache file record for the
/*	specified flags and per-test expiration time stamps.
/*	This may modify the time stamps for disabled tests.
/*
/*	ps_print_grey_key() prints a greylist lookup key.
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
#include <stdio.h>			/* sscanf */

/* Utility library. */

#include <msg.h>

/* Global library. */

#include <mail_params.h>

/* Application-specific. */

#include <postscreen.h>

 /*
  * Kludge to detect if some test is enabled.
  */
#define PS_PREGR_TEST_ENABLE()	(*var_ps_pregr_banner != 0)
#define PS_DNSBL_TEST_ENABLE()	(*var_ps_dnsbl_sites != 0)

 /*
  * Format of a persistent cache entry (which is almost but not quite the
  * same as the in-memory representation).
  * 
  * Each cache entry has one time stamp for each test.
  * 
  * - A time stamp of PS_TIME_STAMP_INVALID must never appear in the cache. It
  * is reserved for in-memory objects that are still being initialized.
  * 
  * - A time stamp of PS_TIME_STAMP_NEW indicates that the test never passed.
  * Postscreen will log the client with "pass new" when it passes the final
  * test.
  * 
  * - A time stamp of PS_TIME_STAMP_DISABLED indicates that the test never
  * passed, and that the test was disabled when the cache entry was written.
  * 
  * - Otherwise, the test was passed, and the time stamp indicates when that
  * test result expires.
  * 
  * A cache entry is expired when the time stamps of all passed tests are
  * expired.
  */

/* ps_new_tests - initialize new test results from scratch */

void    ps_new_tests(PS_STATE *state)
{

    /*
     * We know this client is brand new.
     */
    state->flags = PS_STATE_FLAG_NEW;

    /*
     * Give all tests a PS_TIME_STAMP_NEW time stamp, so that we can later
     * recognize cache entries that haven't passed all enabled tests. When we
     * write a cache entry to the database, any new-but-disabled tests will
     * get a PS_TIME_STAMP_DISABLED time stamp.
     */
    state->pregr_stamp = PS_TIME_STAMP_NEW;
    state->dnsbl_stamp = PS_TIME_STAMP_NEW;
    state->pipel_stamp = PS_TIME_STAMP_NEW;
    state->nsmtp_stamp = PS_TIME_STAMP_NEW;
    state->barlf_stamp = PS_TIME_STAMP_NEW;

    /*
     * Don't flag disabled tests as "todo", because there would be no way to
     * make those bits go away.
     */
    if (PS_PREGR_TEST_ENABLE())
	state->flags |= PS_STATE_FLAG_PREGR_TODO;
    if (PS_DNSBL_TEST_ENABLE())
	state->flags |= PS_STATE_FLAG_DNSBL_TODO;
    if (var_ps_pipel_enable)
	state->flags |= PS_STATE_FLAG_PIPEL_TODO;
    if (var_ps_nsmtp_enable)
	state->flags |= PS_STATE_FLAG_NSMTP_TODO;
    if (var_ps_barlf_enable)
	state->flags |= PS_STATE_FLAG_BARLF_TODO;
}

/* ps_parse_tests - parse test results from cache */

void    ps_parse_tests(PS_STATE *state,
		               const char *stamp_str,
		               time_t time_value)
{
    unsigned long pregr_stamp;
    unsigned long dnsbl_stamp;
    unsigned long pipel_stamp;
    unsigned long nsmtp_stamp;
    unsigned long barlf_stamp;

    /*
     * We don't know what tests have expired or have never passed.
     */
    state->flags = 0;

    /*
     * Parse the cache entry, and allow for older postscreen versions that
     * implemented fewer tests. We pretend that these tests were disabled
     * when the cache entry was written.
     * 
     * Flag the cache entry as "new" when the cache entry has fields for all
     * enabled tests, but the remote SMTP client has not yet passed all those
     * tests.
     */
    switch (sscanf(stamp_str, "%lu;%lu;%lu;%lu;%lu",
		   &pregr_stamp, &dnsbl_stamp, &pipel_stamp, &nsmtp_stamp,
		   &barlf_stamp)) {
    case 0:
	pregr_stamp = PS_TIME_STAMP_DISABLED;
    case 1:
	dnsbl_stamp = PS_TIME_STAMP_DISABLED;
    case 2:
	pipel_stamp = PS_TIME_STAMP_DISABLED;
    case 3:
	nsmtp_stamp = PS_TIME_STAMP_DISABLED;
    case 4:
	barlf_stamp = PS_TIME_STAMP_DISABLED;
    default:
	break;
    }
    state->pregr_stamp = pregr_stamp;
    state->dnsbl_stamp = dnsbl_stamp;
    state->pipel_stamp = pipel_stamp;
    state->nsmtp_stamp = nsmtp_stamp;
    state->barlf_stamp = barlf_stamp;

    if (pregr_stamp == PS_TIME_STAMP_NEW
	|| dnsbl_stamp == PS_TIME_STAMP_NEW
	|| pipel_stamp == PS_TIME_STAMP_NEW
	|| nsmtp_stamp == PS_TIME_STAMP_NEW
	|| barlf_stamp == PS_TIME_STAMP_NEW)
	state->flags |= PS_STATE_FLAG_NEW;

    /*
     * Don't flag a cache entry as expired just because some test was never
     * passed.
     * 
     * Don't flag disabled tests as "todo", because there would be no way to
     * make those bits go away.
     */
    if (PS_PREGR_TEST_ENABLE() && time_value > state->pregr_stamp) {
	state->flags |= PS_STATE_FLAG_PREGR_TODO;
	if (state->pregr_stamp > PS_TIME_STAMP_DISABLED)
	    state->flags |= PS_STATE_FLAG_CACHE_EXPIRED;
    }
    if (PS_DNSBL_TEST_ENABLE() && time_value > state->dnsbl_stamp) {
	state->flags |= PS_STATE_FLAG_DNSBL_TODO;
	if (state->dnsbl_stamp > PS_TIME_STAMP_DISABLED)
	    state->flags |= PS_STATE_FLAG_CACHE_EXPIRED;
    }
    if (var_ps_pipel_enable && time_value > state->pipel_stamp) {
	state->flags |= PS_STATE_FLAG_PIPEL_TODO;
	if (state->pipel_stamp > PS_TIME_STAMP_DISABLED)
	    state->flags |= PS_STATE_FLAG_CACHE_EXPIRED;
    }
    if (var_ps_nsmtp_enable && time_value > state->nsmtp_stamp) {
	state->flags |= PS_STATE_FLAG_NSMTP_TODO;
	if (state->nsmtp_stamp > PS_TIME_STAMP_DISABLED)
	    state->flags |= PS_STATE_FLAG_CACHE_EXPIRED;
    }
    if (var_ps_barlf_enable && time_value > state->barlf_stamp) {
	state->flags |= PS_STATE_FLAG_BARLF_TODO;
	if (state->barlf_stamp > PS_TIME_STAMP_DISABLED)
	    state->flags |= PS_STATE_FLAG_CACHE_EXPIRED;
    }

    /*
     * Gratuitously make postscreen logging more useful by turning on all
     * enabled pre-handshake tests when any pre-handshake test is turned on.
     * 
     * XXX Don't enable PREGREET gratuitously before the test expires. With a
     * short TTL for DNSBL whitelisting, turning on PREGREET would force a
     * full postscreen_greet_wait too frequently.
     */
#if 0
    if (state->flags & PS_STATE_MASK_EARLY_TODO) {
	if (PS_PREGR_TEST_ENABLE())
	    state->flags |= PS_STATE_FLAG_PREGR_TODO;
	if (PS_DNSBL_TEST_ENABLE())
	    state->flags |= PS_STATE_FLAG_DNSBL_TODO;
    }
#endif
}

/* ps_print_tests - print postscreen cache record */

char   *ps_print_tests(VSTRING *buf, PS_STATE *state)
{
    const char *myname = "ps_print_tests";

    /*
     * Sanity check.
     */
    if ((state->flags & PS_STATE_MASK_ANY_UPDATE) == 0)
	msg_panic("%s: attempt to save a no-update record", myname);

    /*
     * Give disabled tests a dummy time stamp so that we don't log a client
     * with "pass new" when some disabled test becomes enabled at some later
     * time.
     */
    if (PS_PREGR_TEST_ENABLE() == 0 && state->pregr_stamp == PS_TIME_STAMP_NEW)
	state->pregr_stamp = PS_TIME_STAMP_DISABLED;
    if (PS_DNSBL_TEST_ENABLE() == 0 && state->dnsbl_stamp == PS_TIME_STAMP_NEW)
	state->dnsbl_stamp = PS_TIME_STAMP_DISABLED;
    if (var_ps_pipel_enable == 0 && state->pipel_stamp == PS_TIME_STAMP_NEW)
	state->pipel_stamp = PS_TIME_STAMP_DISABLED;
    if (var_ps_nsmtp_enable == 0 && state->nsmtp_stamp == PS_TIME_STAMP_NEW)
	state->nsmtp_stamp = PS_TIME_STAMP_DISABLED;
    if (var_ps_barlf_enable == 0 && state->barlf_stamp == PS_TIME_STAMP_NEW)
	state->barlf_stamp = PS_TIME_STAMP_DISABLED;

    vstring_sprintf(buf, "%lu;%lu;%lu;%lu;%lu",
		    (unsigned long) state->pregr_stamp,
		    (unsigned long) state->dnsbl_stamp,
		    (unsigned long) state->pipel_stamp,
		    (unsigned long) state->nsmtp_stamp,
		    (unsigned long) state->barlf_stamp);
    return (STR(buf));
}

/* ps_print_grey_key - print postscreen cache record */

char   *ps_print_grey_key(VSTRING *buf, const char *client,
			          const char *helo, const char *sender,
			          const char *rcpt)
{
    return (STR(vstring_sprintf(buf, "%s/%s/%s/%s",
				client, helo, sender, rcpt)));
}
