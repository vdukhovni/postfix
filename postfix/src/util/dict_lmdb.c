/*++
/* NAME
/*	dict_lmdb 3
/* SUMMARY
/*	dictionary manager interface to OpenLDAP LMDB files
/* SYNOPSIS
/*	#include <dict_lmdb.h>
/*
/*	size_t	dict_lmdb_map_size;
/*	unsigned int dict_lmdb_max_readers;
/*
/*	DICT	*dict_lmdb_open(path, open_flags, dict_flags)
/*	const char *name;
/*	const char *path;
/*	int	open_flags;
/*	int	dict_flags;
/* DESCRIPTION
/*	dict_lmdb_open() opens the named LMDB database and makes
/*	it available via the generic interface described in
/*	dict_open(3).
/*
/*	The dict_lmdb_map_size variable specifies the initial
/*	database memory map size.  When a map becomes full its size
/*	is doubled, and other programs pick up the size change.
/*
/*	The dict_lmdb_max_readers variable specifies the hard (ugh)
/*	limit on the number of read transactions that may be open
/*	at the same time. This should be propertional to the number
/*	of processes that read the table.
/* DIAGNOSTICS
/*	Fatal errors: cannot open file, file write error, out of
/*	memory.
/* BUGS
/*	The on-the-fly map resize operations require no concurrent
/*	activity in the same database by other threads in the same
/*	process.
/* SEE ALSO
/*	dict(3) generic dictionary manager
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

#include <sys_defs.h>

#ifdef HAS_LMDB

/* System library. */

#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#ifdef PATH_LMDB_H
#include PATH_LMDB_H
#else
#include <lmdb.h>
#endif

 /*
  * As of LMDB 0.9.8 the database size limit can be updated on-the-fly. The
  * only limit that remains is imposed by the hardware address space. Earlier
  * LMDB versions are not suitable for use with Postfix.
  */
#if MDB_VERSION_FULL < MDB_VERINT(0, 9, 8)
#error "Build with LMDB version 0.9.8 or later"
#endif

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <htable.h>
#include <iostuff.h>
#include <vstring.h>
#include <myflock.h>
#include <stringops.h>
#include <dict.h>
#include <dict_lmdb.h>
#include <warn_stat.h>

/* Application-specific. */

typedef struct {
    DICT    dict;			/* generic members */
    MDB_env *env;			/* LMDB environment */
    MDB_dbi dbi;			/* database handle */
    MDB_txn *txn;			/* bulk update transaction */
    MDB_cursor *cursor;			/* for sequence ops */
    size_t  map_size;			/* per-database size limit */
    VSTRING *key_buf;			/* key buffer */
    VSTRING *val_buf;			/* value buffer */
    /* The following facilitate LMDB quirk workarounds. */
    int     dict_api_retries;		/* workarounds per dict(3) call */
    int     bulk_mode_retries;		/* workarounds per bulk transaction */
    int     dict_open_flags;		/* dict(3) open flags */
    int     mdb_open_flags;		/* LMDB open flags */
    int     readers_full;		/* MDB_READERS_FULL errors */
} DICT_LMDB;

 /*
  * The LMDB database filename suffix happens to equal our DICT_TYPE_LMDB
  * prefix, but that doesn't mean it is kosher to use DICT_TYPE_LMDB where a
  * suffix is needed, so we define an explicit suffix here.
  */
#define DICT_LMDB_SUFFIX	"lmdb"

 /*
  * Make a safe string copy that is guaranteed to be null-terminated.
  */
#define SCOPY(buf, data, size) \
    vstring_str(vstring_strncpy(buf ? buf : (buf = vstring_alloc(10)), data, size))

 /*
  * Postfix writers recover from a "map full" error by increasing the memory
  * map size with a factor DICT_LMDB_SIZE_INCR (up to some limit) and
  * retrying the transaction.
  * 
  * Each dict(3) API call is retried no more than a few times. For bulk-mode
  * transactions the number of retries is proportional to the size of the
  * address space.
  */
#ifndef SSIZE_T_MAX			/* The maximum map size */
#define SSIZE_T_MAX __MAXINT__(ssize_t)	/* XXX Assumes two's complement */
#endif

#define DICT_LMDB_SIZE_INCR	2	/* Increase size by 1 bit on retry */
#define DICT_LMDB_SIZE_MAX	SSIZE_T_MAX

#define DICT_LMDB_API_RETRY_LIMIT 100	/* Retries per dict(3) API call */
#define DICT_LMDB_BULK_RETRY_LIMIT \
	(2 * sizeof(size_t) * CHAR_BIT)	/* Retries per bulk-mode transaction */

 /*
  * XXX Should dict_lmdb_max_readers be configurable? Is this a per-database
  * property? Per-process? Does it need to be the same for all processes?
  */
size_t  dict_lmdb_map_size = 8192;	/* Minimum size without SIGSEGV */
unsigned int dict_lmdb_max_readers = 216;	/* 200 postfix processes,
						 * plus some extra */

/* #define msg_verbose 1 */

 /*
  * The purpose of the error-recovering functions below is to hide LMDB
  * quirks (MAP_FULL, MAP_CHANGED, READERS_FULL), so that the dict(3) API
  * routines can pretend that those quirks don't exist, and focus on their
  * own job.
  * 
  * - To recover from a single-transaction LMDB error, each wrapper function
  * uses tail recursion instead of goto. Since LMDB errors are rare, code
  * clarity is more important than speed.
  * 
  * - To recover from a bulk-mode transaction LMDB error, the error-recovery
  * code jumps back into the caller to some pre-arranged point (the closest
  * thing that C has to exception handling). With postmap, this means that
  * bulk-mode LMDB error recovery is limited to input that is seekable.
  */

/* dict_lmdb_prepare - LMDB-specific (re)initialization before actual access */

static void dict_lmdb_prepare(DICT_LMDB *dict_lmdb)
{
    int     status;

    /*
     * This is called before accessing the database, or after recovery from
     * an LMDB error. dict_lmdb->txn is either the database open()
     * transaction or a freshly-created bulk-mode transaction.
     * 
     * - With O_TRUNC we make a "drop" request before populating the database.
     * 
     * - With DICT_FLAG_BULK_UPDATE we commit a bulk-mode transaction when the
     * database is closed.
     */
    if (dict_lmdb->dict_open_flags & O_TRUNC) {
	if ((status = mdb_drop(dict_lmdb->txn, dict_lmdb->dbi, 0)) != 0)
	    msg_fatal("truncate %s:%s: %s",
		      dict_lmdb->dict.type, dict_lmdb->dict.name,
		      mdb_strerror(status));
	if ((dict_lmdb->dict.flags & DICT_FLAG_BULK_UPDATE) == 0) {
	    if ((status = mdb_txn_commit(dict_lmdb->txn)))
		msg_fatal("truncate %s:%s: %s",
			  dict_lmdb->dict.type, dict_lmdb->dict.name,
			  mdb_strerror(status));
	    dict_lmdb->txn = NULL;
	}
    } else if ((dict_lmdb->mdb_open_flags & MDB_RDONLY) != 0
	       || (dict_lmdb->dict.flags & DICT_FLAG_BULK_UPDATE) == 0) {
	mdb_txn_abort(dict_lmdb->txn);
	dict_lmdb->txn = NULL;
    }
}

/* dict_lmdb_recover - recover from LMDB errors */

static int dict_lmdb_recover(DICT_LMDB *dict_lmdb, int status)
{
    const char *myname = "dict_lmdb_recover";
    MDB_envinfo info;

    /*
     * Limit the number of recovery attempts per dict(3) API request.
     */
    if ((dict_lmdb->dict_api_retries += 1) > DICT_LMDB_API_RETRY_LIMIT) {
	if (msg_verbose)
	    msg_info("%s: %s:%s too many recovery attempts %d",
		     myname, dict_lmdb->dict.type, dict_lmdb->dict.name,
		     dict_lmdb->dict_api_retries);
	return (status);
    }

    /*
     * If we can recover from the error, we clear the error condition and the
     * caller should retry the failed operation immediately. Otherwise, the
     * caller should terminate with a fatal run-time error and the program
     * should be re-run later.
     * 
     * dict_lmdb->txn is either null (non-bulk transaction error), or an aborted
     * bulk-mode transaction. If we want to make this wrapper layer suitable
     * for general use, then the bulk/non-bulk distinction should be made
     * less specific to Postfix.
     */
    switch (status) {

	/*
	 * As of LMDB 0.9.8 when a non-bulk update runs into a "map full"
	 * error, we can resize the environment's memory map and clear the
	 * error condition. The caller should retry immediately.
	 */
    case MDB_MAP_FULL:
	/* Can we increase the memory map? Give up if we can't. */
	if (dict_lmdb->map_size < DICT_LMDB_SIZE_MAX / DICT_LMDB_SIZE_INCR) {
	    dict_lmdb->map_size = dict_lmdb->map_size * DICT_LMDB_SIZE_INCR;
	} else if (dict_lmdb->map_size < DICT_LMDB_SIZE_MAX) {
	    dict_lmdb->map_size = DICT_LMDB_SIZE_MAX;
	} else {
	    /* Sorry, but we are already maxed out. */
	    break;
	}
	/* Resize the memory map.  */
	if (msg_verbose)
	    msg_info("updating database %s:%s size limit to %lu",
		     dict_lmdb->dict.type, dict_lmdb->dict.name,
		     (unsigned long) dict_lmdb->map_size);
	if ((status = mdb_env_set_mapsize(dict_lmdb->env,
					  dict_lmdb->map_size)) != 0)
	    msg_fatal("env_set_mapsize %s:%s to %lu: %s",
		      dict_lmdb->dict.type, dict_lmdb->dict.name,
		      (unsigned long) dict_lmdb->map_size,
		      mdb_strerror(status));
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
	if ((status = mdb_env_set_mapsize(dict_lmdb->env, 0)) != 0)
	    msg_fatal("env_set_mapsize %s:%s to 0: %s",
		      dict_lmdb->dict.type, dict_lmdb->dict.name,
		      mdb_strerror(status));
	/* Do not panic. Maps may shrink after bulk update. */
	mdb_env_info(dict_lmdb->env, &info);
	dict_lmdb->map_size = info.me_mapsize;
	if (msg_verbose)
	    msg_info("importing database %s:%s new size limit %lu",
		     dict_lmdb->dict.type, dict_lmdb->dict.name,
		     (unsigned long) dict_lmdb->map_size);
	break;

	/*
	 * What is it with these built-in hard limits that cause systems to
	 * fail when resources are needed most? When the system is under
	 * stress it should slow down, not stop working.
	 */
    case MDB_READERS_FULL:
	if (dict_lmdb->readers_full++ == 0)
	    msg_warn("database %s:%s: %s - increase lmdb_max_readers",
		     dict_lmdb->dict.type, dict_lmdb->dict.name,
		     mdb_strerror(status));
	rand_sleep(1000000, 1000000);
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
     * If a bulk-mode transaction error is recoverable, build a new bulk-mode
     * transaction from scratch, by making a long jump back into the caller
     * at some pre-arranged point.
     */
    if (dict_lmdb->txn != 0 && status == 0
     && (dict_lmdb->bulk_mode_retries += 1) <= DICT_LMDB_BULK_RETRY_LIMIT) {
	status = mdb_txn_begin(dict_lmdb->env, NULL,
			       dict_lmdb->mdb_open_flags & MDB_RDONLY,
			       &dict_lmdb->txn);
	if (status != 0)
	    msg_fatal("txn_begin %s:%s: %s",
		      dict_lmdb->dict.type, dict_lmdb->dict.name,
		      mdb_strerror(status));
	dict_lmdb_prepare(dict_lmdb);
	dict_longjmp(&dict_lmdb->dict, 1);
    }
    return (status);
}

/* dict_lmdb_txn_begin - mdb_txn_begin() wrapper with LMDB error recovery */

static void dict_lmdb_txn_begin(DICT_LMDB *dict_lmdb, int rdonly, MDB_txn **txn)
{
    int     status;

    if ((status = mdb_txn_begin(dict_lmdb->env, NULL, rdonly, txn)) != 0) {
	if ((status = dict_lmdb_recover(dict_lmdb, status)) == 0) {
	    dict_lmdb_txn_begin(dict_lmdb, rdonly, txn);
	    return;
	}
	msg_fatal("%s:%s: error starting %s transaction: %s",
		  dict_lmdb->dict.type, dict_lmdb->dict.name,
		  rdonly ? "read" : "write", mdb_strerror(status));
    }
}

/* dict_lmdb_get - mdb_get() wrapper with LMDB error recovery */

static int dict_lmdb_get(DICT_LMDB *dict_lmdb, MDB_val *mdb_key,
			         MDB_val *mdb_value)
{
    MDB_txn *txn;
    int     status;

    /*
     * Start a read transaction if there's no bulk-mode txn.
     */
    if (dict_lmdb->txn)
	txn = dict_lmdb->txn;
    else
	dict_lmdb_txn_begin(dict_lmdb, MDB_RDONLY, &txn);

    /*
     * Do the lookup.
     */
    if ((status = mdb_get(txn, dict_lmdb->dbi, mdb_key, mdb_value)) != 0
	&& status != MDB_NOTFOUND) {
	mdb_txn_abort(txn);
	if (dict_lmdb->txn == 0)
	    txn = 0;
	if ((status = dict_lmdb_recover(dict_lmdb, status)) == 0)
	    return (dict_lmdb_get(dict_lmdb, mdb_key, mdb_value));
    }

    /*
     * Close the read txn if it's not the bulk-mode txn.
     */
    if (txn && dict_lmdb->txn == 0)
	mdb_txn_abort(txn);

    return (status);
}

/* dict_lmdb_put - mdb_put() wrapper with LMDB error recovery */

static int dict_lmdb_put(DICT_LMDB *dict_lmdb, MDB_val *mdb_key,
			         MDB_val *mdb_value, int flags)
{
    MDB_txn *txn;
    int     status;

    /*
     * Start a write transaction if there's no bulk-mode txn.
     */
    if (dict_lmdb->txn)
	txn = dict_lmdb->txn;
    else
	dict_lmdb_txn_begin(dict_lmdb, 0, &txn);

    /*
     * Do the update.
     */
    if ((status = mdb_put(txn, dict_lmdb->dbi, mdb_key, mdb_value, flags)) != 0
	&& status != MDB_KEYEXIST) {
	mdb_txn_abort(txn);
	if (dict_lmdb->txn == 0)
	    txn = 0;
	if ((status = dict_lmdb_recover(dict_lmdb, status)) == 0)
	    return (dict_lmdb_put(dict_lmdb, mdb_key, mdb_value, flags));
    }

    /*
     * Commit the transaction if it's not the bulk-mode txn.
     */
    if (txn && dict_lmdb->txn == 0 && (status = mdb_txn_commit(txn)) != 0) {
	if ((status = dict_lmdb_recover(dict_lmdb, status)) == 0)
	    return (dict_lmdb_put(dict_lmdb, mdb_key, mdb_value, flags));
	msg_fatal("error committing database %s:%s: %s",
		  dict_lmdb->dict.type, dict_lmdb->dict.name,
		  mdb_strerror(status));
    }
    return (status);
}

/* dict_lmdb_del - mdb_del() wrapper with LMDB error recovery */

static int dict_lmdb_del(DICT_LMDB *dict_lmdb, MDB_val *mdb_key)
{
    MDB_txn *txn;
    int     status;

    /*
     * Start a write transaction if there's no bulk-mode txn.
     */
    if (dict_lmdb->txn)
	txn = dict_lmdb->txn;
    else
	dict_lmdb_txn_begin(dict_lmdb, 0, &txn);

    /*
     * Do the update.
     */
    if ((status = mdb_del(txn, dict_lmdb->dbi, mdb_key, NULL)) != 0
	&& status != MDB_NOTFOUND) {
	mdb_txn_abort(txn);
	if (dict_lmdb->txn == 0)
	    txn = 0;
	if ((status = dict_lmdb_recover(dict_lmdb, status)) == 0)
	    return (dict_lmdb_del(dict_lmdb, mdb_key));
    }

    /*
     * Commit the transaction if it's not the bulk-mode txn.
     */
    if (txn && dict_lmdb->txn == 0 && (status = mdb_txn_commit(txn)) != 0) {
	if ((status = dict_lmdb_recover(dict_lmdb, status)) == 0)
	    return (dict_lmdb_del(dict_lmdb, mdb_key));
	msg_fatal("error committing database %s:%s: %s",
		  dict_lmdb->dict.type, dict_lmdb->dict.name,
		  mdb_strerror(status));
    }
    return (status);
}

/* dict_lmdb_cursor_get - mdb_cursor_get() wrapper with LMDB error recovery */

static int dict_lmdb_cursor_get(DICT_LMDB *dict_lmdb, MDB_val *mdb_key,
				        MDB_val *mdb_value, MDB_cursor_op op)
{
    MDB_txn *txn;
    int     status;

    /*
     * Open a read transaction and cursor if needed.
     */
    if (dict_lmdb->cursor == 0) {
	dict_lmdb_txn_begin(dict_lmdb, MDB_RDONLY, &txn);
	if ((status = mdb_cursor_open(txn, dict_lmdb->dbi, &dict_lmdb->cursor))) {
	    if ((status = dict_lmdb_recover(dict_lmdb, status)) == 0)
		return (dict_lmdb_cursor_get(dict_lmdb, mdb_key, mdb_value, op));
	    msg_fatal("%s:%s: cursor_open database: %s",
		      dict_lmdb->dict.type, dict_lmdb->dict.name,
		      mdb_strerror(status));
	}
    }

    /*
     * Database lookup.
     */
    status = mdb_cursor_get(dict_lmdb->cursor, mdb_key, mdb_value, op);

    /*
     * Handle end-of-database or other error.
     */
    if (status != 0) {
	if (status == MDB_NOTFOUND) {
	    txn = mdb_cursor_txn(dict_lmdb->cursor);
	    mdb_cursor_close(dict_lmdb->cursor);
	    mdb_txn_abort(txn);
	    dict_lmdb->cursor = 0;
	} else {
	    if ((status = dict_lmdb_recover(dict_lmdb, status)) == 0)
		return (dict_lmdb_cursor_get(dict_lmdb, mdb_key, mdb_value, op));
	}
    }
    return (status);
}

/* dict_lmdb_finish - wrapper with LMDB error recovery */

static void dict_lmdb_finish(DICT_LMDB *dict_lmdb)
{
    int     status;

    /*
     * Finish the bulk-mode transaction. If dict_lmdb_recover() returns after
     * a bulk-mode transaction error, then it was unable to recover.
     */
    if (dict_lmdb->txn) {
	if ((status = mdb_txn_commit(dict_lmdb->txn)) != 0) {
	    (void) dict_lmdb_recover(dict_lmdb, status);
	    msg_fatal("%s:%s: closing dictionary: %s",
		      dict_lmdb->dict.type, dict_lmdb->dict.name,
		      mdb_strerror(status));
	}
    }

    /*
     * Clean up after an unfinished sequence() operation.
     */
    if (dict_lmdb->cursor) {
	MDB_txn *txn = mdb_cursor_txn(dict_lmdb->cursor);

	mdb_cursor_close(dict_lmdb->cursor);
	mdb_txn_abort(txn);
    }
}

 /*
  * With all recovery from LMDB quirks encapsulated in the routines above,
  * the dict(3) API routines below can pretend that LMDB quirks don't exist
  * and focus on their own job: accessing or updating the database.
  */

/* dict_lmdb_lookup - find database entry */

static const char *dict_lmdb_lookup(DICT *dict, const char *name)
{
    DICT_LMDB *dict_lmdb = (DICT_LMDB *) dict;
    MDB_val mdb_key;
    MDB_val mdb_value;
    const char *result = 0;
    int     status, klen;

    dict->error = 0;
    dict_lmdb->dict_api_retries = 0;
    klen = strlen(name);

    /*
     * Sanity check.
     */
    if ((dict->flags & (DICT_FLAG_TRY1NULL | DICT_FLAG_TRY0NULL)) == 0)
	msg_panic("dict_lmdb_lookup: no DICT_FLAG_TRY1NULL | DICT_FLAG_TRY0NULL flag");

    /*
     * Optionally fold the key.
     */
    if (dict->flags & DICT_FLAG_FOLD_FIX) {
	if (dict->fold_buf == 0)
	    dict->fold_buf = vstring_alloc(10);
	vstring_strcpy(dict->fold_buf, name);
	name = lowercase(vstring_str(dict->fold_buf));
    }

    /*
     * See if this LMDB file was written with one null byte appended to key
     * and value.
     */
    if (dict->flags & DICT_FLAG_TRY1NULL) {
	mdb_key.mv_data = (void *) name;
	mdb_key.mv_size = klen + 1;
	status = dict_lmdb_get(dict_lmdb, &mdb_key, &mdb_value);
	if (status == 0) {
	    dict->flags &= ~DICT_FLAG_TRY0NULL;
	    result = SCOPY(dict_lmdb->val_buf, mdb_value.mv_data,
			   mdb_value.mv_size);
	} else if (status != MDB_NOTFOUND) {
	    msg_fatal("error reading %s:%s: %s",
		      dict_lmdb->dict.type, dict_lmdb->dict.name,
		      mdb_strerror(status));
	}
    }

    /*
     * See if this LMDB file was written with no null byte appended to key
     * and value.
     */
    if (result == 0 && (dict->flags & DICT_FLAG_TRY0NULL)) {
	mdb_key.mv_data = (void *) name;
	mdb_key.mv_size = klen;
	status = dict_lmdb_get(dict_lmdb, &mdb_key, &mdb_value);
	if (status == 0) {
	    dict->flags &= ~DICT_FLAG_TRY1NULL;
	    result = SCOPY(dict_lmdb->val_buf, mdb_value.mv_data,
			   mdb_value.mv_size);
	} else if (status != MDB_NOTFOUND) {
	    msg_fatal("error reading %s:%s: %s",
		      dict_lmdb->dict.type, dict_lmdb->dict.name,
		      mdb_strerror(status));
	}
    }
    return (result);
}

/* dict_lmdb_update - add or update database entry */

static int dict_lmdb_update(DICT *dict, const char *name, const char *value)
{
    DICT_LMDB *dict_lmdb = (DICT_LMDB *) dict;
    MDB_val mdb_key;
    MDB_val mdb_value;
    int     status;

    dict->error = 0;
    dict_lmdb->dict_api_retries = 0;

    /*
     * Sanity check.
     */
    if ((dict->flags & (DICT_FLAG_TRY1NULL | DICT_FLAG_TRY0NULL)) == 0)
	msg_panic("dict_lmdb_update: no DICT_FLAG_TRY1NULL | DICT_FLAG_TRY0NULL flag");

    /*
     * Optionally fold the key.
     */
    if (dict->flags & DICT_FLAG_FOLD_FIX) {
	if (dict->fold_buf == 0)
	    dict->fold_buf = vstring_alloc(10);
	vstring_strcpy(dict->fold_buf, name);
	name = lowercase(vstring_str(dict->fold_buf));
    }
    mdb_key.mv_data = (void *) name;

    mdb_value.mv_data = (void *) value;
    mdb_key.mv_size = strlen(name);
    mdb_value.mv_size = strlen(value);

    /*
     * If undecided about appending a null byte to key and value, choose a
     * default depending on the platform.
     */
    if ((dict->flags & DICT_FLAG_TRY1NULL)
	&& (dict->flags & DICT_FLAG_TRY0NULL)) {
#ifdef LMDB_NO_TRAILING_NULL
	dict->flags &= ~DICT_FLAG_TRY1NULL;
#else
	dict->flags &= ~DICT_FLAG_TRY0NULL;
#endif
    }

    /*
     * Optionally append a null byte to key and value.
     */
    if (dict->flags & DICT_FLAG_TRY1NULL) {
	mdb_key.mv_size++;
	mdb_value.mv_size++;
    }

    /*
     * Do the update.
     */
    status = dict_lmdb_put(dict_lmdb, &mdb_key, &mdb_value,
	       (dict->flags & DICT_FLAG_DUP_REPLACE) ? 0 : MDB_NOOVERWRITE);
    if (status != 0) {
	if (status == MDB_KEYEXIST) {
	    if (dict->flags & DICT_FLAG_DUP_IGNORE)
		 /* void */ ;
	    else if (dict->flags & DICT_FLAG_DUP_WARN)
		msg_warn("%s:%s: duplicate entry: \"%s\"",
			 dict_lmdb->dict.type, dict_lmdb->dict.name, name);
	    else
		msg_fatal("%s:%s: duplicate entry: \"%s\"",
			  dict_lmdb->dict.type, dict_lmdb->dict.name, name);
	} else {
	    msg_fatal("error updating %s:%s: %s",
		      dict_lmdb->dict.type, dict_lmdb->dict.name,
		      mdb_strerror(status));
	}
    }
    return (status);
}

/* dict_lmdb_delete - delete one entry from the dictionary */

static int dict_lmdb_delete(DICT *dict, const char *name)
{
    DICT_LMDB *dict_lmdb = (DICT_LMDB *) dict;
    MDB_val mdb_key;
    int     status = 1, klen;

    dict->error = 0;
    dict_lmdb->dict_api_retries = 0;
    klen = strlen(name);

    /*
     * Sanity check.
     */
    if ((dict->flags & (DICT_FLAG_TRY1NULL | DICT_FLAG_TRY0NULL)) == 0)
	msg_panic("dict_lmdb_delete: no DICT_FLAG_TRY1NULL | DICT_FLAG_TRY0NULL flag");

    /*
     * Optionally fold the key.
     */
    if (dict->flags & DICT_FLAG_FOLD_FIX) {
	if (dict->fold_buf == 0)
	    dict->fold_buf = vstring_alloc(10);
	vstring_strcpy(dict->fold_buf, name);
	name = lowercase(vstring_str(dict->fold_buf));
    }

    /*
     * See if this LMDB file was written with one null byte appended to key
     * and value.
     */
    if (dict->flags & DICT_FLAG_TRY1NULL) {
	mdb_key.mv_data = (void *) name;
	mdb_key.mv_size = klen + 1;
	status = dict_lmdb_del(dict_lmdb, &mdb_key);
	if (status != 0) {
	    if (status == MDB_NOTFOUND)
		status = 1;
	    else
		msg_fatal("error deleting from %s:%s: %s",
			  dict_lmdb->dict.type, dict_lmdb->dict.name,
			  mdb_strerror(status));
	} else {
	    dict->flags &= ~DICT_FLAG_TRY0NULL;	/* found */
	}
    }

    /*
     * See if this LMDB file was written with no null byte appended to key
     * and value.
     */
    if (status > 0 && (dict->flags & DICT_FLAG_TRY0NULL)) {
	mdb_key.mv_data = (void *) name;
	mdb_key.mv_size = klen;
	status = dict_lmdb_del(dict_lmdb, &mdb_key);
	if (status != 0) {
	    if (status == MDB_NOTFOUND)
		status = 1;
	    else
		msg_fatal("error deleting from %s:%s: %s",
			  dict_lmdb->dict.type, dict_lmdb->dict.name,
			  mdb_strerror(status));
	} else {
	    dict->flags &= ~DICT_FLAG_TRY1NULL;	/* found */
	}
    }
    return (status);
}

/* dict_lmdb_sequence - traverse the dictionary */

static int dict_lmdb_sequence(DICT *dict, int function,
			              const char **key, const char **value)
{
    const char *myname = "dict_lmdb_sequence";
    DICT_LMDB *dict_lmdb = (DICT_LMDB *) dict;
    MDB_val mdb_key;
    MDB_val mdb_value;
    MDB_cursor_op op;
    int     status;

    dict->error = 0;
    dict_lmdb->dict_api_retries = 0;

    /*
     * Determine the seek function.
     */
    switch (function) {
    case DICT_SEQ_FUN_FIRST:
	op = MDB_FIRST;
	break;
    case DICT_SEQ_FUN_NEXT:
	op = MDB_NEXT;
	break;
    default:
	msg_panic("%s: invalid function: %d", myname, function);
    }

    /*
     * Database lookup.
     */
    status = dict_lmdb_cursor_get(dict_lmdb, &mdb_key, &mdb_value, op);

    switch (status) {

	/*
	 * Copy the key and value so they are guaranteed null terminated.
	 */
    case 0:
	*key = SCOPY(dict_lmdb->key_buf, mdb_key.mv_data, mdb_key.mv_size);
	if (mdb_value.mv_data != 0 && mdb_value.mv_size > 0)
	    *value = SCOPY(dict_lmdb->val_buf, mdb_value.mv_data,
			   mdb_value.mv_size);
	break;

	/*
	 * End-of-database.
	 */
    case MDB_NOTFOUND:
	status = 1;
	break;

	/*
	 * Bust.
	 */
    default:
	msg_fatal("error seeking %s:%s: %s",
		  dict_lmdb->dict.type, dict_lmdb->dict.name,
		  mdb_strerror(status));
    }
    return (status);
}

/* dict_lmdb_lock - noop lock handler */

static int dict_lmdb_lock(DICT *dict, int unused_op)
{
    /* LMDB does its own concurrency control */
    return 0;
}

/* dict_lmdb_close - disassociate from data base */

static void dict_lmdb_close(DICT *dict)
{
    DICT_LMDB *dict_lmdb = (DICT_LMDB *) dict;

    dict_lmdb->dict_api_retries = 0;
    dict_lmdb_finish(dict_lmdb);
    if (dict_lmdb->dict.stat_fd >= 0)
	close(dict_lmdb->dict.stat_fd);
    mdb_env_close(dict_lmdb->env);
    if (dict_lmdb->key_buf)
	vstring_free(dict_lmdb->key_buf);
    if (dict_lmdb->val_buf)
	vstring_free(dict_lmdb->val_buf);
    if (dict->fold_buf)
	vstring_free(dict->fold_buf);
    dict_free(dict);
}

/* dict_lmdb_open - open LMDB data base */

DICT   *dict_lmdb_open(const char *path, int open_flags, int dict_flags)
{
    DICT_LMDB *dict_lmdb;
    struct stat st;
    MDB_env *env;
    MDB_txn *txn;
    MDB_dbi dbi;
    char   *mdb_path;
    int     env_flags, status;
    size_t  map_size = dict_lmdb_map_size;

    mdb_path = concatenate(path, "." DICT_TYPE_LMDB, (char *) 0);

    env_flags = MDB_NOSUBDIR;
    if (open_flags == O_RDONLY)
	env_flags |= MDB_RDONLY;

    if ((status = mdb_env_create(&env)))
	msg_fatal("env_create %s: %s", mdb_path, mdb_strerror(status));

    if (stat(mdb_path, &st) == 0 && st.st_size > map_size) {
	if (st.st_size / map_size < DICT_LMDB_SIZE_MAX / map_size) {
	    map_size = (st.st_size / map_size + 1) * map_size;
	} else {
	    map_size = st.st_size;
	}
	if (msg_verbose)
	    msg_info("using %s:%s map size %lu",
		     DICT_TYPE_LMDB, path, (unsigned long) map_size);
    }
    if ((status = mdb_env_set_mapsize(env, map_size)))
	msg_fatal("env_set_mapsize %s: %s", mdb_path, mdb_strerror(status));

    if ((status = mdb_env_set_maxreaders(env, dict_lmdb_max_readers)))
	msg_fatal("env_set_maxreaders %s: %s", mdb_path, mdb_strerror(status));

    /*
     * Gracefully handle the most common mistake.
     */
    if ((status = mdb_env_open(env, mdb_path, env_flags, 0644))) {
	mdb_env_close(env);
	return (dict_surrogate(DICT_TYPE_LMDB, path, open_flags, dict_flags,
			       "open database %s: %m", mdb_path));
    }
    if ((status = mdb_txn_begin(env, NULL, env_flags & MDB_RDONLY, &txn)))
	msg_fatal("txn_begin %s: %s", mdb_path, mdb_strerror(status));

    /*
     * mdb_open() requires a txn, but since the default DB always exists in
     * an LMDB environment, we usually don't need to do anything else with
     * the txn. It is currently used for bulk transactions.
     */
    if ((status = mdb_open(txn, NULL, 0, &dbi)))
	msg_fatal("mdb_open %s: %s", mdb_path, mdb_strerror(status));

    /*
     * Bundle up.
     */
    dict_lmdb = (DICT_LMDB *) dict_alloc(DICT_TYPE_LMDB, path, sizeof(*dict_lmdb));
    dict_lmdb->dict.lookup = dict_lmdb_lookup;
    dict_lmdb->dict.update = dict_lmdb_update;
    dict_lmdb->dict.delete = dict_lmdb_delete;
    dict_lmdb->dict.sequence = dict_lmdb_sequence;
    dict_lmdb->dict.close = dict_lmdb_close;
    dict_lmdb->dict.lock = dict_lmdb_lock;
    if ((dict_lmdb->dict.stat_fd = open(mdb_path, O_RDONLY)) < 0)
	msg_fatal("dict_lmdb_open: %s: %m", mdb_path);
    if (fstat(dict_lmdb->dict.stat_fd, &st) < 0)
	msg_fatal("dict_lmdb_open: fstat: %m");
    dict_lmdb->dict.mtime = st.st_mtime;
    dict_lmdb->dict.owner.uid = st.st_uid;
    dict_lmdb->dict.owner.status = (st.st_uid != 0);

    /*
     * Warn if the source file is newer than the indexed file, except when
     * the source file changed only seconds ago.
     */
    if ((dict_flags & DICT_FLAG_LOCK) != 0
	&& stat(path, &st) == 0
	&& st.st_mtime > dict_lmdb->dict.mtime
	&& st.st_mtime < time((time_t *) 0) - 100)
	msg_warn("database %s is older than source file %s", mdb_path, path);

    close_on_exec(dict_lmdb->dict.stat_fd, CLOSE_ON_EXEC);
    dict_lmdb->dict.flags = dict_flags | DICT_FLAG_FIXED;
    if ((dict_flags & (DICT_FLAG_TRY0NULL | DICT_FLAG_TRY1NULL)) == 0)
	dict_lmdb->dict.flags |= (DICT_FLAG_TRY0NULL | DICT_FLAG_TRY1NULL);
    if (dict_flags & DICT_FLAG_FOLD_FIX)
	dict_lmdb->dict.fold_buf = vstring_alloc(10);
    dict_lmdb->env = env;
    dict_lmdb->dbi = dbi;
    dict_lmdb->map_size = map_size;

    dict_lmdb->cursor = 0;
    dict_lmdb->key_buf = 0;
    dict_lmdb->val_buf = 0;

    /* The following facilitate transparent error recovery. */
    dict_lmdb->dict_api_retries = 0;
    dict_lmdb->bulk_mode_retries = 0;
    dict_lmdb->dict_open_flags = open_flags;
    dict_lmdb->mdb_open_flags = env_flags;
    dict_lmdb->txn = txn;
    dict_lmdb->readers_full = 0;
    dict_lmdb_prepare(dict_lmdb);
    if (dict_flags & DICT_FLAG_BULK_UPDATE)
	dict_jmp_alloc(&dict_lmdb->dict);	/* build into dict_alloc() */

    myfree(mdb_path);

    return (DICT_DEBUG (&dict_lmdb->dict));
}

#endif
