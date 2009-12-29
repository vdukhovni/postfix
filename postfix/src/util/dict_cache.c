/*++
/* NAME
/*	dict_cache 3
/* SUMMARY
/*	External cache manager
/* SYNOPSIS
/*	#include <dict_cache.h>
/*
/*	DICT_CACHE *dict_cache_open(dbname, open_flags, dict_flags)
/*	const char *dbname;
/*	int	open_flags;
/*	int	dict_flags;
/*
/*	void	dict_cache_close(cache)
/*	DICT_CACHE *cache;
/*
/*	const char *dict_cache_lookup(cache, cache_key)
/*	DICT_CACHE *cache;
/*	const char *cache_key;
/*
/*	int	dict_cache_update(cache, cache_key, cache_val)
/*	DICT_CACHE *cache;
/*	const char *cache_key;
/*	const char *cache_val;
/*
/*	int	dict_cache_delete(cache, cache_key)
/*	DICT_CACHE *cache;
/*	const char *cache_key;
/*
/*	int	dict_cache_sequence(cache, first_next, cache_key, cache_val)
/*	DICT_CACHE *cache;
/*	int	first_next;
/*	const char **cache_key;
/*	const char **cache_val;
/*
/*	void	dict_cache_expire(cache, flags, interval, validator, context)
/*	DICT_CACHE *cache;
/*	int	flags;
/*	int	interval;
/*	int	(*validator)(const char *cache_key, const char *cache_val,
/*				char *context);
/*	char	*context;
/* AUXILIARY FUNCTIONS
/*	const char *dict_cache_name(cache)
/*	DICT_CACHE	*cache;
/*
/*	DICT_CACHE *dict_cache_import(table)
/*	DICT	*table;
/* DESCRIPTION
/*	This module maintains external cache files with support
/*	for expiration. The underlying table must implement the
/*	"lookup", "update", "delete" and "sequence" operations.
/*
/*	Although this API is similar to the one documented in
/*	dict_open(3), there are subtle differences in the interaction
/*	between the iterators that access all cache elements, and
/*	other operations that access individual cache elements.
/*
/*	In particular, when a "sequence" or "expire" operation is
/*	in progress the cache intercepts requests to delete the
/*	"current" entry, as this would cause some databases to
/*	mis-behave. Instead, the cache implements a "delete behind"
/*	strategy, and deletes such an entry after the "sequence"
/*	or "expire" operation moves on to the next cache element.
/*	The "delete behind" strategy also affects the cache lookup
/*	and update operations as detailed below.
/*
/*	dict_cache_open() opens the specified cache and returns a
/*	handle that must be used for subsequent access. This function
/*	does not return in case of error.
/*
/*	dict_cache_close() closes the specified cache and releases
/*	memory that was allocated by dict_cache_open(), and terminates
/*	any thread that was started with dict_cache_expire().
/*
/*	dict_cache_lookup() looks up the specified cache entry.
/*	The result value is a null pointer when the cache entry was
/*	not found, or when the entry is scheduled for "delete
/*	behind".
/*
/*	dict_cache_update() updates the specified cache entry. If
/*	the entry is scheduled for "delete behind", the delete
/*	operation is canceled (meaning that the cache must be opened
/*	with DICT_FLAG_DUP_REPLACE). This function does not return
/*	in case of error.
/*
/*	dict_cache_delete() removes the specified cache entry.  If
/*	this is the "current" entry of a "sequence" operation, the
/*	entry is scheduled for "delete behind". The result value
/*	is zero when the entry was found.
/*
/*	dict_cache_sequence() iterates over the specified cache and
/*	returns each entry in an implementation-defined order.  The
/*	result value is zero when a cache entry was found.  Programs
/*	must not use both dict_cache_sequence() and dict_cache_expire().
/*
/*	dict_cache_expire() schedules a thread that expires cache
/*	entries periodically. Specify a null validator argument to
/*	cancel the thread.  It is an error to schedule a cache
/*	cleanup thread when one already exists.  Programs must not
/*	use both dict_cache_sequence() and dict_cache_expire().
/*
/*	dict_cache_name() returns the name of the specified cache.
/*
/*	dict_cache_import() encapsulates a pre-opened database
/*	handle and adds the above features.
/*
/*	Arguments:
/* .IP "dbname, open_flags, dict_flags"
/*	These are passed unchanged to dict_open().
/* .IP cache
/*	Cache handle created with dict_cache_open()or dict_cache_import().
/* .IP cache_key
/*	Cache lookup key.
/* .IP cache_val
/*	Information that is stored under a cache lookup key.
/* .IP first_next
/*	One of DICT_SEQ_FUN_FIRST (first cache element) or
/*	DICT_SEQ_FUN_NEXT (next cache element).
/* .sp
/*	Note: there is no "stop" request. To ensure that the "delete
/*	behind" strategy does not interfere with database access,
/*	allow dict_cache_sequence() to run to completion.
/* .IP flags
/*	Bit-wise OR of zero or more of the following:
/* .RS 
/* .IP DICT_CACHE_FLAG_EXP_VERBOSE
/*	Log each cache entry's status during a cache cleanup run.
/* .IP DICT_CACHE_FLAG_EXP_SUMMARY
/*	Log the number of cache entries retained and dropped after
/*	a cache cleaning run.
/* .RE
/* .IP interval
/*	The non-zero time between scans for expired cache entries.
/*	The interval timer starts after a scan completes.
/* .IP validator
/*	Application call-back routine that returns non-zero when a
/*	cache entry should be kept. The validator must not modify
/*	or close the cache.
/* .IP context
/*	Application-specific context.
/* .IP table
/*	A bare dictonary handle.
/* DIAGNOSTICS
/*	These routines terminate with a fatal run-time error
/*	for unrecoverable database errors. This allows the
/*	program to restart and reset the database to an
/*	empty initial state.
/* BUGS
/*	There should be a way to suspend automatic program suicide
/*	until a cache cleanup run is completed. Some entries may
/*	never be removed when the process max_idle time is less
/*	than the time needed to make a full pass over the cache.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* HISTORY
/* .ad
/* .fi
/*	A predecessor of this code was written first for the Postfix
/*	tlsmgr(8) daemon.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include <sys_defs.h>
#include <string.h>
#include <stdlib.h>

/* Utility library. */

#include <msg.h>
#include <dict.h>
#include <mymalloc.h>
#include <events.h>
#include <dict_cache.h>

/* Application-specific. */

 /*
  * XXX Deleting entries while enumerating a map can he tricky. Some map
  * types have a concept of cursor and support a "delete the current element"
  * operation. Some map types without cursors don't behave well when the
  * current first/next entry is deleted (example: with Berkeley DB < 2, the
  * "next" operation produces garbage). To avoid trouble, we delete an entry
  * after advancing the current first/next position beyond it; we use the
  * same strategy with application requests to delete the current entry.
  */

 /*
  * Opaque data structure. Use dict_cache_name() to access the name of the
  * underlying database.
  */
struct DICT_CACHE {
    int     flags;			/* see below */
    DICT   *db;				/* database handle */

    /* Iterator support. */
    char   *saved_curr_key;		/* "current" cache lookup key */
    char   *saved_curr_val;		/* "current" cache lookup result */

    /* Cleanup support. */
    int     exp_flags;			/* logging */
    int     exp_interval;		/* time between cleanup runs */
    DICT_CACHE_VALIDATOR_FN exp_validator;	/* expiration call-back */
    char   *exp_context;		/* call-back context */
    int     retained;			/* entries retained in cleanup run */
    int     dropped;			/* entries removed in cleanup run */
};

#define DC_FLAG_DEL_SAVED_CURRENT_KEY	(1<<0)	/* delete-behind is scheduled */

 /*
  * Macros to make obscure code more readable.
  */
#define DC_SCHEDULE_FOR_DELETE_BEHIND(cp) \
    ((cp)->flags |= DC_FLAG_DEL_SAVED_CURRENT_KEY)

#define DC_MATCH_SAVED_CURRENT_KEY(cp, cache_key) \
    ((cp)->saved_curr_key && strcmp((cp)->saved_curr_key, (cache_key)) == 0)

#define DC_IS_SCHEDULED_FOR_DELETE_BEHIND(cp) \
    (/* NOT: (cp)->saved_curr_key && */ \
	((cp)->flags & DC_FLAG_DEL_SAVED_CURRENT_KEY) != 0)

#define DC_CANCEL_DELETE_BEHIND(cp) \
    ((cp)->flags &= ~DC_FLAG_DEL_SAVED_CURRENT_KEY)

 /*
  * Special key to store the time of last cache cleanup run completion.
  */
#define DC_LAST_CACHE_CLEANUP_COMPLETED "_LAST_CACHE_CLEANUP_COMPLETED_"

/* dict_cache_lookup - load entry from cache */

const char *dict_cache_lookup(DICT_CACHE *cp, const char *cache_key)
{

    /*
     * Search for the cache entry. Don't return an entry that was scheduled
     * for deletion.
     */
    if (DC_IS_SCHEDULED_FOR_DELETE_BEHIND(cp)
	&& DC_MATCH_SAVED_CURRENT_KEY(cp, cache_key)) {
	return (0);
    } else {
	return (dict_get(cp->db, cache_key));
    }
}

/* dict_cache_update - save entry to cache */

void    dict_cache_update(DICT_CACHE *cp, const char *cache_key,
			          const char *cache_val)
{

    /*
     * Store the cache entry and cancel a scheduled delete-behind operation.
     */
    if (DC_IS_SCHEDULED_FOR_DELETE_BEHIND(cp)
	&& DC_MATCH_SAVED_CURRENT_KEY(cp, cache_key))
	DC_CANCEL_DELETE_BEHIND(cp);
    dict_put(cp->db, cache_key, cache_val);
}

/* dict_cache_delete - delete entry from cache */

int     dict_cache_delete(DICT_CACHE *cp, const char *cache_key)
{
    int     zero_means_found;

    /*
     * Delete the entry, unless we would delete the current first/next entry.
     * Instead, schedule the "current" entry for delete-behind to avoid
     * mis-behavior by some databases.
     */
    if (DC_MATCH_SAVED_CURRENT_KEY(cp, cache_key)) {
	DC_SCHEDULE_FOR_DELETE_BEHIND(cp);
	zero_means_found = 0;
    } else {
	zero_means_found = dict_del(cp->db, cache_key);
    }
    return (zero_means_found);
}

/* dict_cache_sequence - look up the first/next cache entry */

int     dict_cache_sequence(DICT_CACHE *cp, int first_next,
			            const char **cache_key,
			            const char **cache_val)
{
    int     zero_means_found;
    const char *raw_cache_key;
    const char *raw_cache_val;
    char   *previous_curr_key;
    char   *previous_curr_val;

    /*
     * Find the first or next database entry. Hide the record with the cache
     * cleanup completion time stamp.
     */
    zero_means_found =
	dict_seq(cp->db, first_next, &raw_cache_key, &raw_cache_val);
    if (zero_means_found == 0
	&& strcmp(raw_cache_key, DC_LAST_CACHE_CLEANUP_COMPLETED) == 0)
	zero_means_found =
	    dict_seq(cp->db, DICT_SEQ_FUN_NEXT, &raw_cache_key, &raw_cache_val);

    /*
     * Save the current cache_key and cache_val before they are clobbered by
     * our own delete operation below. This also prevents surprises when the
     * application accesses the database after this function returns.
     * 
     * We also use the saved cache_key to protect the current entry against
     * application delete requests.
     */
    previous_curr_key = cp->saved_curr_key;
    previous_curr_val = cp->saved_curr_val;
    if (zero_means_found == 0) {
	cp->saved_curr_key = mystrdup(raw_cache_key);
	cp->saved_curr_val = mystrdup(raw_cache_val);
    } else {
	cp->saved_curr_key = 0;
	cp->saved_curr_val = 0;
    }

    /*
     * Delete behind.
     */
    if (DC_IS_SCHEDULED_FOR_DELETE_BEHIND(cp)) {
	DC_CANCEL_DELETE_BEHIND(cp);
	if (dict_del(cp->db, previous_curr_key) != 0)
	    msg_warn("database %s: could not delete entry for %s",
		     cp->db->name, previous_curr_key);
    }

    /*
     * Clean up previous iteration key and value.
     */
    if (previous_curr_key)
	myfree(previous_curr_key);
    if (previous_curr_val)
	myfree(previous_curr_val);

    /*
     * Return the result.
     */
    *cache_key = (cp)->saved_curr_key;
    *cache_val = (cp)->saved_curr_val;
    return (zero_means_found);
}

/* dict_cache_delete_behind_reset - reset "delete behind" state */

static void dict_cache_delete_behind_reset(DICT_CACHE *cp)
{
#define FREE_AND_WIPE(s) do { if (s) { myfree(s); (s) = 0; } } while (0)

    DC_CANCEL_DELETE_BEHIND(cp);
    FREE_AND_WIPE(cp->saved_curr_key);
    FREE_AND_WIPE(cp->saved_curr_val);
}

/* dict_cache_clean_stat_log_reset - log and reset cache cleanup statistics */

static void dict_cache_clean_stat_log_reset(DICT_CACHE *cp,
					           const char *full_partial)
{
    if (cp->flags & DICT_CACHE_FLAG_EXP_SUMMARY)
	msg_info("cache %s %s cleanup: retained=%d dropped=%d entries",
		 cp->db->name, full_partial, cp->retained, cp->dropped);
    cp->retained = cp->dropped = 0;
}

/* dict_cache_expire_event - examine one cache entry */

static void dict_cache_expire_event(int unused_event, char *cache_context)
{
    DICT_CACHE *cp = (DICT_CACHE *) cache_context;
    const char *cache_key;
    const char *cache_val;
    int     next_interval;
    VSTRING *stamp_buf;
    int     first_next;

    /*
     * We interleave cache cleanup with other processing, so that the
     * application's service remains available, with perhaps increased
     * latency.
     */

    /*
     * Start a new cache cleanup run.
     */
    if (cp->saved_curr_key == 0) {
	cp->retained = cp->dropped = 0;
	first_next = DICT_SEQ_FUN_FIRST;
	if (cp->exp_flags & DICT_CACHE_FLAG_EXP_VERBOSE)
	    msg_info("start %s cache cleanup", cp->db->name);
    }

    /*
     * Continue a cache cleanup run in progress.
     */
    else {
	first_next = DICT_SEQ_FUN_NEXT;
    }

    /*
     * Examine one cache entry.
     */
    if (dict_cache_sequence(cp, first_next, &cache_key, &cache_val) == 0) {
	if (cp->exp_validator(cache_key, cache_val, cp->exp_context) == 0) {
	    DC_SCHEDULE_FOR_DELETE_BEHIND(cp);
	    cp->dropped++;
	    if (cp->exp_flags & DICT_CACHE_FLAG_EXP_VERBOSE)
		msg_info("drop %s cache entry for %s", cp->db->name, cache_key);
	} else {
	    cp->retained++;
	    if (cp->exp_flags & DICT_CACHE_FLAG_EXP_VERBOSE)
		msg_info("keep %s cache entry for %s", cp->db->name, cache_key);
	}
	next_interval = 0;
    }

    /*
     * Cache cleanup completed. Report vital statistics.
     */
    else {
	if (cp->exp_flags & DICT_CACHE_FLAG_EXP_VERBOSE)
	    msg_info("done %s cache cleanup scan", cp->db->name);
	dict_cache_clean_stat_log_reset(cp, "full");
	stamp_buf = vstring_alloc(100);
	vstring_sprintf(stamp_buf, "%ld", (long) event_time());
	dict_put(cp->db, DC_LAST_CACHE_CLEANUP_COMPLETED,
		 vstring_str(stamp_buf));
	vstring_free(stamp_buf);
	next_interval = cp->exp_interval;
    }
    event_request_timer(dict_cache_expire_event, cache_context, next_interval);
}

/* dict_cache_expire - schedule or stop the cache cleanup thread */

void    dict_cache_expire(DICT_CACHE *cp, int flags, int interval,
			          DICT_CACHE_VALIDATOR_FN validator,
			          char *context)
{
    const char *myname = "dict_cache_expire";
    const char *last_done;
    time_t  next_interval;

    /*
     * Schedule the cache cleanup thread.
     */
    if (validator != 0) {

	/*
	 * Sanity checks.
	 */
	if (cp->exp_validator != 0)
	    msg_panic("%s: %s cache cleanup is already scheduled",
		      myname, cp->db->name);
	if (interval <= 0)
	    msg_panic("%s: bad %s cache cleanup interval %d",
		      myname, cp->db->name, interval);
	cp->exp_flags = flags;
	cp->exp_interval = interval;
	cp->exp_validator = validator;
	cp->exp_context = context;

	/*
	 * The next start time depends on the last completion time.
	 */
#define NEXT_START(last, delta) ((delta) + (unsigned long) atol(last))
#define NOW	(time((time_t *) 0))		/* NOT: event_time() */

	if ((last_done = dict_get(cp->db, DC_LAST_CACHE_CLEANUP_COMPLETED)) == 0
	    || (next_interval = (NEXT_START(last_done, interval) - NOW)) < 0)
	    next_interval = 0;
	if (next_interval > interval)
	    next_interval = interval;
	if ((cp->exp_flags & DICT_CACHE_FLAG_EXP_VERBOSE) && next_interval > 0)
	    msg_info("%s cache cleanup will start after %ds",
		     cp->db->name, (int) next_interval);
	event_request_timer(dict_cache_expire_event, (char *) cp,
			    (int) next_interval);
    }

    /*
     * Cancel the cache cleanup thread.
     */
    else if (cp->exp_validator) {
	if (cp->retained || cp->dropped)
	    dict_cache_clean_stat_log_reset(cp, "partial");
	dict_cache_delete_behind_reset(cp);
	cp->exp_interval = 0;
	cp->exp_validator = 0;
	cp->exp_context = 0;
	event_cancel_timer(dict_cache_expire_event, (char *) cp);
    }
}

/* dict_cache_open - open cache file */

DICT_CACHE *dict_cache_open(const char *dbname, int open_flags, int dict_flags)
{
    DICT   *dict;

    /*
     * Open the database as requested. Don't attempt to second-guess the
     * application.
     */
    dict = dict_open(dbname, open_flags, dict_flags);
    return (dict_cache_import(dict));
}

/* dict_cache_import - encapsulate pre-opened database */

DICT_CACHE *dict_cache_import(DICT *dict)
{
    DICT_CACHE *cp;

    /*
     * Create the DICT_CACHE object.
     */
    cp = (DICT_CACHE *) mymalloc(sizeof(*cp));
    cp->flags = 0;
    cp->db = dict;
    cp->saved_curr_key = 0;
    cp->saved_curr_val = 0;
    cp->exp_interval = 0;
    cp->exp_validator = 0;
    cp->exp_context = 0;
    cp->retained = 0;
    cp->dropped = 0;

    return (cp);
}

/* dict_cache_close - close cache file */

void    dict_cache_close(DICT_CACHE *cp)
{

    /*
     * Destroy the DICT_CACHE object.
     */
    if (cp->exp_validator)
	dict_cache_expire(cp, 0, 0, (DICT_CACHE_VALIDATOR_FN) 0, (char *) 0);
    dict_close(cp->db);
    if (cp->saved_curr_key)
	myfree(cp->saved_curr_key);
    if (cp->saved_curr_val)
	myfree(cp->saved_curr_val);
    myfree((char *) cp);
}

/* dict_cache_name - get the cache name */

const char *dict_cache_name(DICT_CACHE *cp)
{

    /*
     * This is used for verbose logging or warning messages, so the cost of
     * call is only made where needed (well sort off - code that does not
     * execute still presents overhead for the processor pipeline, processor
     * cache, etc).
     */
    return (cp->db->name);
}
