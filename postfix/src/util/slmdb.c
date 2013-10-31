/*++
/* NAME
/*	slmdb 3
/* SUMMARY
/*	Simplified LMDB API
/* SYNOPSIS
/*	#include <slmdb.h>
/*
/*	size_t	slmdb_map_size;
/*
/*	int	slmdb_open(slmdb, path, open_flags, lmdb_flags, bulk_mode,
/*				curr_limit, size_incr, hard_limit)
/*	SLMDB *slmdb;
/*	const char *path;
/*	int	open_flags;
/*	int	lmdb_flags;
/*	int	bulk_mode;
/*	size_t	curr_limit;
/*	int	size_incr;
/*	size_t	hard_limit;
/*
/*	int	slmdb_close(slmdb)
/*	SLMDB *slmdb;
/*
/*	int	slmdb_get(slmdb, mdb_key, mdb_value)
/*	SLMDB *slmdb;
/*	MDB_val	*mdb_key;
/*	MDB_val	*mdb_value;
/*
/*	int	slmdb_put(slmdb, mdb_key, mdb_value, flags)
/*	SLMDB *slmdb;
/*	MDB_val	*mdb_key;
/*	MDB_val	*mdb_value;
/*	int	flags;
/*
/*	int	slmdb_del(slmdb, mdb_key)
/*	SLMDB *slmdb;
/*	MDB_val	*mdb_key;
/*
/*	int	slmdb_cursor_get(slmdb, mdb_key, mdb_value, op)
/*	SLMDB *slmdb;
/*	MDB_val	*mdb_key;
/*	MDB_val	*mdb_value;
/*	MDB_cursor_op op;
/* AUXILIARY FUNCTIONS
/*	int	slmdb_fd(slmdb)
/*	SLMDB *slmdb;
/*
/*	size_t	slmdb_curr_limit(slmdb)
/*	SLMDB *slmdb;
/*
/*	int	slmdb_control(slmdb, id, ...)
/*	SLMDB *slmdb;
/*	int	id;
/* DESCRIPTION
/*	This module simplifies the LMDB API by hiding recoverable
/*	errors from the application.  Details are given in the
/*	section "ERROR RECOVERY".
/*
/*	slmdb_open() opens an LMDB database.  The result value is
/*	an LMDB status code (zero in case of success).
/*
/*	slmdb_close() finalizes an optional bulk-mode transaction
/*	and closes a successfully-opened LMDB database.  The result
/*	value is an LMDB status code (zero in case of success).
/*
/*	slmdb_get() is an mdb_get() wrapper with automatic error
/*	recovery.  The result value is an LMDB status code (zero
/*	in case of success).
/*
/*	slmdb_put() is an mdb_put() wrapper with automatic error
/*	recovery.  The result value is an LMDB status code (zero
/*	in case of success).
/*
/*	slmdb_del() is an mdb_del() wrapper with automatic error
/*	recovery.  The result value is an LMDB status code (zero
/*	in case of success).
/*
/*	slmdb_cursor_get() iterates over an LMDB database.  The
/*	result value is an LMDB status code (zero in case of success).
/*
/*	slmdb_fd() returns the file descriptor for an open LMDB
/*	database.  This may be used for file status queries or
/*	application-controlled locking.
/*
/*	slmdb_curr_limit() returns the current database size limit
/*	for the specified database.
/*
/*	slmdb_control() specifies optional features. The arguments
/*	are a list of (name, value) pairs, terminated with
/*	SLMDB_CTL_END.  The result is 0 in case of success, or -1
/*	with errno indicating the nature of the problem. The following
/*	text enumerates the symbolic request names and the types
/*	of the corresponding additional arguments.
/* .IP "SLMDB_CTL_LONGJMP_FN (void (*)(void *, int))
/*	Application long-jump call-back function pointer. The
/*	function must not return and is called to repeat a failed
/*	bulk-mode transaction from the start. The arguments are
/*	the application context and the setjmp() or sigsetjmp()
/*	result value.
/* .IP "SLMDB_CTL_NOTIFY_FN (void (*)(void *, int, ...))"
/*	Application notification call-back function pointer. The
/*	function is called after succesful error recovery with as
/*	arguments the application context, the MDB error code, and
/*	additional arguments that depend on the error code.
/*	Details are given in the section "ERROR RECOVERY".
/* .IP "SLMDB_CTL_CONTEXT (void *)"
/*	Application context that is passed in application notification
/*	and long-jump call-back function calls.
/* .IP "SLMDB_CTL_API_RETRY_LIMIT (int)"
/*	How many times to recover from LMDB errors within the
/*	execution of a single slmdb(3) API call before giving up.
/* .IP "SLMDB_CTL_BULK_RETRY_LIMIT (int)"
/*	How many times to recover from a bulk-mode transaction
/*	before giving up.
/* ERROR RECOVERY
/* .ad
/* .fi
/*	This module automatically repeats failed requests after
/*	recoverable errors, up to limits specified with slmdb_control().
/*
/*	Recoverable errors are reported through an optional
/*	notification function specified with slmdb_control().  With
/*	recoverable MDB_MAP_FULL and MDB_MAP_RESIZED errors, the
/*	additional argument is a size_t value with the updated
/*	current database size limit; with recoverable MDB_READERS_FULL
/*	errors there is no additional argument.
/* BUGS
/*	Recovery from MDB_MAP_FULL involves resizing the database
/*	memory mapping.  According to LMDB documentation this
/*	requires that there is no concurrent activity in the same
/*	database by other threads in the same memory address space.
/* SEE ALSO
/*	lmdb(3) API manpage (currently, non-existent).
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Howard Chu
/*	Symas Corporation
/*
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

/* Application-specific. */

#include <slmdb.h>

 /*
  * LMDB 0.9.8 allows the application to update the database size limit
  * on-the-fly (typically after an MDB_MAP_FULL error). The only limit that
  * remains is imposed by the hardware address space. The implementation is
  * supposed to handle databases larger than physical memory. However, at
  * some point in time there was no such guarantee for (bulk) transactions
  * larger than physical memory.
  * 
  * LMDB 0.9.9 allows the application to manage locks. This elimimates multiple
  * problems:
  * 
  * - The need for a (world-)writable lockfile, which is a show-stopper for
  * multiprogrammed applications that have privileged writers and
  * unprivileged readers.
  * 
  * - Hard-coded inode numbers (in ftok() output) in lockfile content that can
  * prevent automatic crash recovery, and related to that, sub-optimal
  * semaphore performance on BSD systems.
  */
#if MDB_VERSION_FULL < MDB_VERINT(0, 9, 9)
#error "Build with LMDB version 0.9.9 or later"
#endif

#define SLMDB_DEF_API_RETRY_LIMIT 2	/* Retries per dict(3) API call */
#define SLMDB_DEF_BULK_RETRY_LIMIT \
        (2 * sizeof(size_t) * CHAR_BIT)	/* Retries per bulk-mode transaction */

 /*
  * The purpose of the error-recovering functions below is to hide LMDB
  * quirks (MAP_FULL, MAP_RESIZED, MDB_READERS_FULL), so that the caller can
  * pretend that those quirks don't exist, and focus on its own job.
  * 
  * - To recover from a single-transaction LMDB error, each wrapper function
  * uses tail recursion instead of goto. Since LMDB errors are rare, code
  * clarity is more important than speed.
  * 
  * - To recover from a bulk-transaction LMDB error, the error-recovery code
  * jumps back into the caller to some pre-arranged point (the closest thing
  * that C has to exception handling). The application is then expected to
  * repeat the bulk transaction from scratch.
  */

 /*
  * We increment the recursion counter each time we try to recover from
  * error, and reset the recursion counter when returning to the application
  * from the slmdb API.
  */
#define SLMDB_API_RETURN(slmdb, status) do { \
	(slmdb)->api_retry_count = 0; \
	return (status); \
    } while (0)

/* slmdb_prepare - LMDB-specific (re)initialization before actual access */

static int slmdb_prepare(SLMDB *slmdb)
{
    int     status;

    /*
     * This is called before accessing the database, or after recovery from
     * an LMDB error. Note: this code cannot recover from errors itself.
     * slmdb->txn is either the database open() transaction or a
     * freshly-created bulk-mode transaction.
     * 
     * - With O_TRUNC we make a "drop" request before updating the database.
     * 
     * - With a bulk-mode transaction we commit when the database is closed.
     * 
     * XXX If we want to make the slmdb API suitable for general use, then the
     * bulk/non-bulk handling must be generalized.
     */
    if (slmdb->open_flags & O_TRUNC) {
	if ((status = mdb_drop(slmdb->txn, slmdb->dbi, 0)) != 0)
	    return (status);
	if ((slmdb->bulk_mode) == 0) {
	    if ((status = mdb_txn_commit(slmdb->txn)))
		return (status);
	    slmdb->txn = 0;
	}
    } else if ((slmdb->lmdb_flags & MDB_RDONLY) != 0
	       || (slmdb->bulk_mode) == 0) {
	mdb_txn_abort(slmdb->txn);
	slmdb->txn = 0;
    }
    slmdb->api_retry_count = 0;
    return (status);
}

/* slmdb_recover - recover from LMDB errors */

static int slmdb_recover(SLMDB *slmdb, int status)
{
    MDB_envinfo info;

    /*
     * Limit the number of recovery attempts per slmdb(3) API request.
     */
    if ((slmdb->api_retry_count += 1) >= slmdb->api_retry_limit)
	return (status);

    /*
     * If we can recover from the error, we clear the error condition and the
     * caller should retry the failed operation immediately. Otherwise, the
     * caller should terminate with a fatal run-time error and the program
     * should be re-run later.
     * 
     * slmdb->txn must be either null (non-bulk transaction error), or an
     * aborted bulk-mode transaction.
     * 
     * XXX If we want to make the slmdb API suitable for general use, then the
     * bulk/non-bulk handling must be generalized.
     */
    switch (status) {

	/*
	 * As of LMDB 0.9.8 when a non-bulk update runs into a "map full"
	 * error, we can resize the environment's memory map and clear the
	 * error condition. The caller should retry immediately.
	 */
    case MDB_MAP_FULL:
	/* Can we increase the memory map? Give up if we can't. */
	if (slmdb->curr_limit < slmdb->hard_limit / slmdb->size_incr) {
	    slmdb->curr_limit = slmdb->curr_limit * slmdb->size_incr;
	} else if (slmdb->curr_limit < slmdb->hard_limit) {
	    slmdb->curr_limit = slmdb->hard_limit;
	} else {
	    /* Sorry, we are already maxed out. */
	    break;
	}
	if (slmdb->notify_fn)
	    slmdb->notify_fn(slmdb->cb_context, MDB_MAP_FULL,
			     slmdb->curr_limit);
	status = mdb_env_set_mapsize(slmdb->env, slmdb->curr_limit);
	break;

	/*
	 * When a writer resizes the database, read-only applications must
	 * increase their LMDB memory map size limit, too. Otherwise, they
	 * won't be able to read a table after it grows.
	 * 
	 * As of LMDB 0.9.8 we can import the new memory map size limit into the
	 * database environment by calling mdb_env_set_mapsize() with a zero
	 * size argument. Then we extract the map size limit for later use.
	 * The caller should retry immediately.
	 */
    case MDB_MAP_RESIZED:
	if ((status = mdb_env_set_mapsize(slmdb->env, 0)) == 0) {
	    /* Do not panic. Maps may shrink after bulk update. */
	    mdb_env_info(slmdb->env, &info);
	    slmdb->curr_limit = info.me_mapsize;
	    if (slmdb->notify_fn)
		slmdb->notify_fn(slmdb->cb_context, MDB_MAP_RESIZED,
				 slmdb->curr_limit);
	}
	break;

	/*
	 * What is it with these built-in hard limits that cause systems to
	 * stop when demand is at its highest? When the system is under
	 * stress it should slow down and keep making progress.
	 */
    case MDB_READERS_FULL:
	if (slmdb->notify_fn)
	    slmdb->notify_fn(slmdb->cb_context, MDB_READERS_FULL);
	sleep(1);
	status = 0;
	break;

	/*
	 * We can't solve this problem. The application should terminate with
	 * a fatal run-time error and the program should be re-run later.
	 */
    default:
	break;
    }

    /*
     * If a bulk-transaction error is recoverable, build a new bulk
     * transaction from scratch, by making a long jump back into the caller
     * at some pre-arranged point.
     */
    if (slmdb->txn != 0 && status == 0 && slmdb->longjmp_fn != 0
	&& (slmdb->bulk_retry_count += 1) <= slmdb->bulk_retry_limit) {
	if ((status = mdb_txn_begin(slmdb->env, (MDB_txn *) 0,
				    slmdb->lmdb_flags & MDB_RDONLY,
				    &slmdb->txn)) == 0
	    && (status = slmdb_prepare(slmdb)) == 0)
	    slmdb->longjmp_fn(slmdb->cb_context, 1);
    }
    return (status);
}

/* slmdb_txn_begin - mdb_txn_begin() wrapper with LMDB error recovery */

static int slmdb_txn_begin(SLMDB *slmdb, int rdonly, MDB_txn **txn)
{
    int     status;

    if ((status = mdb_txn_begin(slmdb->env, (MDB_txn *) 0, rdonly, txn)) != 0
	&& (status = slmdb_recover(slmdb, status)) == 0)
	status = slmdb_txn_begin(slmdb, rdonly, txn);

    return (status);
}

/* slmdb_get - mdb_get() wrapper with LMDB error recovery */

int     slmdb_get(SLMDB *slmdb, MDB_val *mdb_key, MDB_val *mdb_value)
{
    MDB_txn *txn;
    int     status;

    /*
     * Start a read transaction if there's no bulk-mode txn.
     */
    if (slmdb->txn)
	txn = slmdb->txn;
    else if ((status = slmdb_txn_begin(slmdb, MDB_RDONLY, &txn)) != 0)
	SLMDB_API_RETURN(slmdb, status);

    /*
     * Do the lookup.
     */
    if ((status = mdb_get(txn, slmdb->dbi, mdb_key, mdb_value)) != 0
	&& status != MDB_NOTFOUND) {
	mdb_txn_abort(txn);
	if ((status = slmdb_recover(slmdb, status)) == 0)
	    status = slmdb_get(slmdb, mdb_key, mdb_value);
	SLMDB_API_RETURN(slmdb, status);
    }

    /*
     * Close the read txn if it's not the bulk-mode txn.
     */
    if (slmdb->txn == 0)
	mdb_txn_abort(txn);

    SLMDB_API_RETURN(slmdb, status);
}

/* slmdb_put - mdb_put() wrapper with LMDB error recovery */

int     slmdb_put(SLMDB *slmdb, MDB_val *mdb_key,
		          MDB_val *mdb_value, int flags)
{
    MDB_txn *txn;
    int     status;

    /*
     * Start a write transaction if there's no bulk-mode txn.
     */
    if (slmdb->txn)
	txn = slmdb->txn;
    else if ((status = slmdb_txn_begin(slmdb, 0, &txn)) != 0)
	SLMDB_API_RETURN(slmdb, status);

    /*
     * Do the update.
     */
    if ((status = mdb_put(txn, slmdb->dbi, mdb_key, mdb_value, flags)) != 0) {
	mdb_txn_abort(txn);
	if (status != MDB_KEYEXIST) {
	    if ((status = slmdb_recover(slmdb, status)) == 0)
		status = slmdb_put(slmdb, mdb_key, mdb_value, flags);
	    SLMDB_API_RETURN(slmdb, status);
	}
    }

    /*
     * Commit the transaction if it's not the bulk-mode txn.
     */
    if (status == 0 && slmdb->txn == 0 && (status = mdb_txn_commit(txn)) != 0
	&& (status = slmdb_recover(slmdb, status)) == 0)
	status = slmdb_put(slmdb, mdb_key, mdb_value, flags);

    SLMDB_API_RETURN(slmdb, status);
}

/* slmdb_del - mdb_del() wrapper with LMDB error recovery */

int     slmdb_del(SLMDB *slmdb, MDB_val *mdb_key)
{
    MDB_txn *txn;
    int     status;

    /*
     * Start a write transaction if there's no bulk-mode txn.
     */
    if (slmdb->txn)
	txn = slmdb->txn;
    else if ((status = slmdb_txn_begin(slmdb, 0, &txn)) != 0)
	SLMDB_API_RETURN(slmdb, status);

    /*
     * Do the update.
     */
    if ((status = mdb_del(txn, slmdb->dbi, mdb_key, (MDB_val *) 0)) != 0) {
	mdb_txn_abort(txn);
	if (status != MDB_NOTFOUND) {
	    if ((status = slmdb_recover(slmdb, status)) == 0)
		status = slmdb_del(slmdb, mdb_key);
	    SLMDB_API_RETURN(slmdb, status);
	}
    }

    /*
     * Commit the transaction if it's not the bulk-mode txn.
     */
    if (status == 0 && slmdb->txn == 0 && (status = mdb_txn_commit(txn)) != 0
	&& (status = slmdb_recover(slmdb, status)) == 0)
	status = slmdb_del(slmdb, mdb_key);

    SLMDB_API_RETURN(slmdb, status);
}

/* slmdb_cursor_get - mdb_cursor_get() wrapper with LMDB error recovery */

int     slmdb_cursor_get(SLMDB *slmdb, MDB_val *mdb_key,
			         MDB_val *mdb_value, MDB_cursor_op op)
{
    MDB_txn *txn;
    int     status;

    /*
     * Open a read transaction and cursor if needed.
     */
    if (slmdb->cursor == 0) {
	slmdb_txn_begin(slmdb, MDB_RDONLY, &txn);
	if ((status = mdb_cursor_open(txn, slmdb->dbi, &slmdb->cursor)) != 0) {
	    mdb_txn_abort(txn);
	    if ((status = slmdb_recover(slmdb, status)) == 0)
		status = slmdb_cursor_get(slmdb, mdb_key, mdb_value, op);
	    SLMDB_API_RETURN(slmdb, status);
	}
    }

    /*
     * Database lookup.
     */
    status = mdb_cursor_get(slmdb->cursor, mdb_key, mdb_value, op);

    /*
     * Handle end-of-database or other error.
     */
    if (status != 0) {
	if (status == MDB_NOTFOUND) {
	    txn = mdb_cursor_txn(slmdb->cursor);
	    mdb_cursor_close(slmdb->cursor);
	    mdb_txn_abort(txn);
	    slmdb->cursor = 0;
	} else {
	    if ((status = slmdb_recover(slmdb, status)) == 0)
		status = slmdb_cursor_get(slmdb, mdb_key, mdb_value, op);
	    SLMDB_API_RETURN(slmdb, status);
	    /* Do not hand-optimize out the above return statement. */
	}
    }
    SLMDB_API_RETURN(slmdb, status);
}

/* slmdb_control - control optional settings */

int     slmdb_control(SLMDB *slmdb, int first,...)
{
    va_list ap;
    int     status = 0;
    int     reqno;

    va_start(ap, first);
    for (reqno = first; reqno != SLMDB_CTL_END; reqno = va_arg(ap, int)) {
	switch (reqno) {
	case SLMDB_CTL_LONGJMP_FN:
	    slmdb->longjmp_fn = va_arg(ap, SLMDB_LONGJMP_FN);
	    break;
	case SLMDB_CTL_NOTIFY_FN:
	    slmdb->notify_fn = va_arg(ap, SLMDB_NOTIFY_FN);
	    break;
	case SLMDB_CTL_CONTEXT:
	    slmdb->cb_context = va_arg(ap, void *);
	    break;
	case SLMDB_CTL_API_RETRY_LIMIT:
	    slmdb->api_retry_limit = va_arg(ap, int);
	    break;
	case SLMDB_CTL_BULK_RETRY_LIMIT:
	    slmdb->bulk_retry_limit = va_arg(ap, int);
	    break;
	default:
	    errno = EINVAL;
	    status = -1;
	    break;
	}
    }
    va_end(ap);
    return (status);
}

/* slmdb_close - wrapper with LMDB error recovery */

int     slmdb_close(SLMDB *slmdb)
{
    int     status = 0;

    /*
     * Finish an open bulk transaction. If slmdb_recover() returns after a
     * bulk-transaction error, then it was unable to recover.
     */
    if (slmdb->txn != 0
	&& (status = mdb_txn_commit(slmdb->txn)) != 0)
	status = slmdb_recover(slmdb, status);

    /*
     * Clean up after an unfinished sequence() operation.
     */
    if (slmdb->cursor) {
	MDB_txn *txn = mdb_cursor_txn(slmdb->cursor);

	mdb_cursor_close(slmdb->cursor);
	mdb_txn_abort(txn);
    }
    mdb_env_close(slmdb->env);

    SLMDB_API_RETURN(slmdb, status);
}

/* slmdb_open - open wrapped LMDB database */

int     slmdb_open(SLMDB *slmdb, const char *path, int open_flags,
		           int lmdb_flags, int bulk_mode, size_t curr_limit,
		           int size_incr, size_t hard_limit)
{
    struct stat st;
    MDB_env *env;
    MDB_txn *txn;
    MDB_dbi dbi;
    int     db_fd;
    int     status;

    /*
     * Create LMDB environment.
     */
    if ((status = mdb_env_create(&env)) != 0)
	return (status);

    /*
     * Make sure that the memory map has room to store and commit an initial
     * "drop" transaction. We have no way to recover from errors before the
     * first application-level request.
     */
#define SLMDB_FUDGE      8192

    if (curr_limit < SLMDB_FUDGE)
	curr_limit = SLMDB_FUDGE;
    if (stat(path, &st) == 0 && st.st_size > curr_limit - SLMDB_FUDGE) {
	if (st.st_size > hard_limit)
	    hard_limit = st.st_size;
	if (st.st_size < hard_limit - SLMDB_FUDGE)
	    curr_limit = st.st_size + SLMDB_FUDGE;
	else
	    curr_limit = hard_limit;
    }

    /*
     * mdb_open() requires a txn, but since the default DB always exists in
     * an LMDB environment, we usually don't need to do anything else with
     * the txn. It is currently used for truncate and for bulk transactions.
     */
    if ((status = mdb_env_set_mapsize(env, curr_limit)) != 0
	|| (status = mdb_env_open(env, path, lmdb_flags, 0644)) != 0
	|| (status = mdb_txn_begin(env, (MDB_txn *) 0,
				   lmdb_flags & MDB_RDONLY, &txn)) != 0
	|| (status = mdb_open(txn, (const char *) 0, 0, &dbi)) != 0
	|| (status = mdb_env_get_fd(env, &db_fd)) != 0) {
	mdb_env_close(env);
	return (status);
    }

    /*
     * Bundle up.
     */
    slmdb->open_flags = open_flags;
    slmdb->lmdb_flags = lmdb_flags;
    slmdb->bulk_mode = bulk_mode;
    slmdb->curr_limit = curr_limit;
    slmdb->size_incr = size_incr;
    slmdb->hard_limit = hard_limit;
    slmdb->env = env;
    slmdb->dbi = dbi;
    slmdb->db_fd = db_fd;
    slmdb->cursor = 0;
    slmdb->api_retry_count = 0;
    slmdb->bulk_retry_count = 0;
    slmdb->api_retry_limit = SLMDB_DEF_API_RETRY_LIMIT;
    slmdb->bulk_retry_limit = SLMDB_DEF_BULK_RETRY_LIMIT;
    slmdb->longjmp_fn = 0;
    slmdb->notify_fn = 0;
    slmdb->cb_context = 0;
    slmdb->txn = txn;

    if ((status = slmdb_prepare(slmdb)) != 0)
	mdb_env_close(env);

    return (status);
}
