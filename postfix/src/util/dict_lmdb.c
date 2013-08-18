/*++
/* NAME
/*	dict_lmdb 3
/* SUMMARY
/*	dictionary manager interface to OpenLDAP LMDB files
/* SYNOPSIS
/*	#include <dict_lmdb.h>
/*
/*	size_t	dict_lmdb_map_size;
/*
/*	DICT	*dict_lmdb_open(path, open_flags, dict_flags)
/*	const char *name;
/*	const char *path;
/*	int	open_flags;
/*	int	dict_flags;
/* DESCRIPTION
/*	dict_lmdb_open() opens the named LMDB database and makes it available
/*	via the generic interface described in dict_open(3).
/*
/*	The dict_lmdb_map_size variable specifies a non-default
/*	per-table memory map size.  The map size is also the maximum
/*	size the table can grow to, so it must be set large enough
/*	to accomodate the largest tables in use.
/*
/*	As a safety measure, when Postfix opens an LMDB database
/*	it will set the memory map size to at least 3x the ".lmdb"
/*	file size, so that there is room for the file to grow. This
/*	ensures that a process can recover from a "table full" error
/*	with a simple terminate-and-restart.
/*
/*	As a second safety measure, when an update or delete operation
/*	runs into an MDB_MAP_FULL error, Postfix will extend the
/*	database file to the current ".lmdb" file size, so that the
/*	above workaround will be triggered the next time the database
/*	is opened.
/* DIAGNOSTICS
/*	Fatal errors: cannot open file, file write error, out of memory.
/* SEE ALSO
/*	dict(3) generic dictionary manager
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Howard Chu
/*	Symas Corporation
/*--*/

#include "sys_defs.h"

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

/* Utility library. */

#include "msg.h"
#include "mymalloc.h"
#include "htable.h"
#include "iostuff.h"
#include "vstring.h"
#include "myflock.h"
#include "stringops.h"
#include "dict.h"
#include "dict_lmdb.h"
#include "warn_stat.h"

/* Application-specific. */

typedef struct {
    DICT    dict;			/* generic members */
    MDB_env *env;			/* LMDB environment */
    MDB_dbi dbi;			/* database handle */
    MDB_txn *txn;			/* bulk update transaction */
    MDB_cursor *cursor;			/* for sequence ops */
    VSTRING *key_buf;			/* key buffer */
    VSTRING *val_buf;			/* result buffer */
} DICT_LMDB;

#define SCOPY(buf, data, size) \
    vstring_str(vstring_strncpy(buf ? buf : (buf = vstring_alloc(10)), data, size))

size_t  dict_lmdb_map_size = (10 * 1024 * 1024);	/* 10MB default mmap
							 * size */
unsigned int dict_lmdb_max_readers = 216;	/* 200 postfix processes,
						 * plus some extra */

 /*
  * Out-of-space safety net. When an update or delete operation fails with
  * MDB_MAP_FULL, extend the database file size so that the next
  * dict_lmdb_open() call will force a 3x over-allocation.
  * 
  * XXX This strategy assumes that a bogus file size will not affect LMDB
  * operation. In private communication on August 18, 2013, Howard Chu
  * confirmed this as follows: "It will have no effect. LMDB internally
  * accounts for the last used page#, the filesystem's notion of filesize
  * isn't used for any purpose."
  * 
  * We make no assumptions about which LMDB operations may fail with
  * MDB_MAP_FULL. Instead we wrap all LMDB operations inside a Postfix
  * function that may change a database.
  */
#define DICT_LMDB_WRAPPER(dict, status, operation) \
    ((status = operation) == MDB_MAP_FULL ? \
	(dict_lmdb_grow(dict), status) : status)

/* dict_lmdb_grow - grow the DB file if the last txn failed to grow it */

static void dict_lmdb_grow(DICT *dict)
{
    struct stat st;
    char   *mdb_path = concatenate(dict->name, "." DICT_TYPE_LMDB, (char *) 0);

    /*
     * After MDB_MAP_FULL error, expand the file size to trigger the 3x size
     * limit workaround on the next open() attempt.
     */
    if (stat(mdb_path, &st) == 0 && st.st_size < dict_lmdb_map_size
	&& truncate(mdb_path, dict_lmdb_map_size) < 0)
	msg_warn("dict_lmdb_grow: cannot grow database file %s:%s: %m",
		 dict->type, dict->name);
    myfree(mdb_path);
}

/* dict_lmdb_lookup - find database entry */

static const char *dict_lmdb_lookup(DICT *dict, const char *name)
{
    DICT_LMDB *dict_lmdb = (DICT_LMDB *) dict;
    MDB_val mdb_key;
    MDB_val mdb_value;
    MDB_txn *txn;
    const char *result = 0;
    int     status, klen;

    dict->error = 0;
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
     * Start a read transaction if there's no global txn.
     */
    if (dict_lmdb->txn)
	txn = dict_lmdb->txn;
    else if ((status = mdb_txn_begin(dict_lmdb->env, NULL, MDB_RDONLY, &txn)))
	msg_fatal("%s: txn_begin(read) dictionary: %s", dict_lmdb->dict.name, mdb_strerror(status));

    /*
     * See if this LMDB file was written with one null byte appended to key
     * and value.
     */
    if (dict->flags & DICT_FLAG_TRY1NULL) {
	mdb_key.mv_data = (void *) name;
	mdb_key.mv_size = klen + 1;
	status = mdb_get(txn, dict_lmdb->dbi, &mdb_key, &mdb_value);
	if (!status) {
	    dict->flags &= ~DICT_FLAG_TRY0NULL;
	    result = SCOPY(dict_lmdb->val_buf, mdb_value.mv_data, mdb_value.mv_size);
	}
    }

    /*
     * See if this LMDB file was written with no null byte appended to key
     * and value.
     */
    if (result == 0 && (dict->flags & DICT_FLAG_TRY0NULL)) {
	mdb_key.mv_data = (void *) name;
	mdb_key.mv_size = klen;
	status = mdb_get(txn, dict_lmdb->dbi, &mdb_key, &mdb_value);
	if (!status) {
	    dict->flags &= ~DICT_FLAG_TRY1NULL;
	    result = SCOPY(dict_lmdb->val_buf, mdb_value.mv_data, mdb_value.mv_size);
	}
    }

    /*
     * Close the read txn if it's not the global txn.
     */
    if (!dict_lmdb->txn)
	mdb_txn_abort(txn);

    return (result);
}

/* dict_lmdb_update - add or update database entry */

static int dict_lmdb_update(DICT *dict, const char *name, const char *value)
{
    DICT_LMDB *dict_lmdb = (DICT_LMDB *) dict;
    MDB_val mdb_key;
    MDB_val mdb_value;
    MDB_txn *txn;
    int     status;

    dict->error = 0;

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
     * Start a write transaction if there's no global txn.
     */
    if (dict_lmdb->txn)
	txn = dict_lmdb->txn;
    else if (DICT_LMDB_WRAPPER(dict, status,
			       mdb_txn_begin(dict_lmdb->env, NULL, 0, &txn)))
	msg_fatal("%s: txn_begin(write) dictionary: %s", dict_lmdb->dict.name, mdb_strerror(status));

    /*
     * Do the update.
     */
    (void) DICT_LMDB_WRAPPER(dict, status,
			  mdb_put(txn, dict_lmdb->dbi, &mdb_key, &mdb_value,
	      (dict->flags & DICT_FLAG_DUP_REPLACE) ? 0 : MDB_NOOVERWRITE));
    if (status) {
	if (status == MDB_KEYEXIST) {
	    if (dict->flags & DICT_FLAG_DUP_IGNORE)
		 /* void */ ;
	    else if (dict->flags & DICT_FLAG_DUP_WARN)
		msg_warn("%s: duplicate entry: \"%s\"", dict_lmdb->dict.name, name);
	    else
		msg_fatal("%s: duplicate entry: \"%s\"", dict_lmdb->dict.name, name);
	} else {
	    msg_fatal("error writing LMDB database %s: %s", dict_lmdb->dict.name, mdb_strerror(status));
	}
    }

    /*
     * Commit the transaction if it's not the global txn.
     */
    if (!dict_lmdb->txn && DICT_LMDB_WRAPPER(dict, status, mdb_txn_commit(txn)))
	msg_fatal("error committing LMDB database %s: %s", dict_lmdb->dict.name, mdb_strerror(status));

    return (status);
}

/* dict_lmdb_delete - delete one entry from the dictionary */

static int dict_lmdb_delete(DICT *dict, const char *name)
{
    DICT_LMDB *dict_lmdb = (DICT_LMDB *) dict;
    MDB_val mdb_key;
    MDB_txn *txn;
    int     status = 1, klen, rc;

    dict->error = 0;
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
     * Start a write transaction if there's no global txn.
     */
    if (dict_lmdb->txn)
	txn = dict_lmdb->txn;
    else if (DICT_LMDB_WRAPPER(dict, status,
			       mdb_txn_begin(dict_lmdb->env, NULL, 0, &txn)))
	msg_fatal("%s: txn_begin(write) dictionary: %s", dict_lmdb->dict.name, mdb_strerror(status));

    /*
     * See if this LMDB file was written with one null byte appended to key
     * and value.
     */
    if (dict->flags & DICT_FLAG_TRY1NULL) {
	mdb_key.mv_data = (void *) name;
	mdb_key.mv_size = klen + 1;
	(void) DICT_LMDB_WRAPPER(dict, status,
			      mdb_del(txn, dict_lmdb->dbi, &mdb_key, NULL));
	if (status) {
	    if (status == MDB_NOTFOUND)
		status = 1;
	    else
		msg_fatal("error deleting from %s: %s", dict_lmdb->dict.name, mdb_strerror(status));
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
	(void) DICT_LMDB_WRAPPER(dict, status,
			      mdb_del(txn, dict_lmdb->dbi, &mdb_key, NULL));
	if (status) {
	    if (status == MDB_NOTFOUND)
		status = 1;
	    else
		msg_fatal("error deleting from %s: %s", dict_lmdb->dict.name, mdb_strerror(status));
	} else {
	    dict->flags &= ~DICT_FLAG_TRY1NULL;	/* found */
	}
    }

    /*
     * Commit the transaction if it's not the global txn.
     */
    if (!dict_lmdb->txn && DICT_LMDB_WRAPPER(dict, rc, mdb_txn_commit(txn)))
	msg_fatal("error committing LMDB database %s: %s", dict_lmdb->dict.name, mdb_strerror(rc));

    return (status);
}

/* traverse the dictionary */

static int dict_lmdb_sequence(DICT *dict, int function,
			              const char **key, const char **value)
{
    const char *myname = "dict_lmdb_sequence";
    DICT_LMDB *dict_lmdb = (DICT_LMDB *) dict;
    MDB_val mdb_key;
    MDB_val mdb_value;
    MDB_txn *txn;
    MDB_cursor_op op;
    int     status;

    dict->error = 0;

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
     * Open a read transaction and cursor if needed.
     */
    if (dict_lmdb->cursor == 0) {
	if ((status = mdb_txn_begin(dict_lmdb->env, NULL, MDB_RDONLY, &txn)))
	    msg_fatal("%s: txn_begin(read) dictionary: %s", dict_lmdb->dict.name, mdb_strerror(status));
	if ((status = mdb_cursor_open(txn, dict_lmdb->dbi, &dict_lmdb->cursor)))
	    msg_fatal("%s: cursor_open dictionary: %s", dict_lmdb->dict.name, mdb_strerror(status));
    }

    /*
     * Database lookup.
     */
    status = mdb_cursor_get(dict_lmdb->cursor, &mdb_key, &mdb_value, op);
    if (status && status != MDB_NOTFOUND)
	msg_fatal("%s: seeking dictionary: %s", dict_lmdb->dict.name, mdb_strerror(status));

    if (status == MDB_NOTFOUND) {

	/*
	 * Caller must read to end, to ensure cursor gets closed.
	 */
	status = 1;
	txn = mdb_cursor_txn(dict_lmdb->cursor);
	mdb_cursor_close(dict_lmdb->cursor);
	mdb_txn_abort(txn);
	dict_lmdb->cursor = 0;
    } else {

	/*
	 * Copy the key so that it is guaranteed null terminated.
	 */
	*key = SCOPY(dict_lmdb->key_buf, mdb_key.mv_data, mdb_key.mv_size);

	if (mdb_value.mv_data != 0 && mdb_value.mv_size > 0) {

	    /*
	     * Copy the value so that it is guaranteed null terminated.
	     */
	    *value = SCOPY(dict_lmdb->val_buf, mdb_value.mv_data, mdb_value.mv_size);
	    status = 0;
	}
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

    if (dict_lmdb->txn) {
	int     status;

	(void) DICT_LMDB_WRAPPER(dict, status, mdb_txn_commit(dict_lmdb->txn));
	if (status)
	    msg_fatal("%s: closing dictionary: %s", dict_lmdb->dict.name, mdb_strerror(status));
	dict_lmdb->cursor = NULL;
    }
    if (dict_lmdb->cursor) {
	MDB_txn *txn = mdb_cursor_txn(dict_lmdb->cursor);

	mdb_cursor_close(dict_lmdb->cursor);
	mdb_txn_abort(txn);
    }
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

    mdb_path = concatenate(path, "." DICT_TYPE_LMDB, (char *) 0);

    env_flags = MDB_NOSUBDIR;
    if (open_flags == O_RDONLY)
	env_flags |= MDB_RDONLY;

    if ((status = mdb_env_create(&env)))
	msg_fatal("env_create %s: %s", mdb_path, mdb_strerror(status));

    /*
     * Try to ensure that the LMDB size limit is at least 3x the current LMDB
     * file size. This ensures that Postfix daemon processes can recover from
     * a "table full" error with a simple terminate-and-restart.
     * 
     * Note: read-only applications must increase their LMDB size limit, too,
     * otherwise they won't be able to read a table after it grows.
     */
#ifndef SIZE_T_MAX
#define SIZE_T_MAX __MAXINT__(size_t)
#endif
#define LMDB_SIZE_FUDGE_FACTOR	3

    if (stat(mdb_path, &st) == 0
	&& st.st_size >= dict_lmdb_map_size / LMDB_SIZE_FUDGE_FACTOR) {
	msg_warn("%s: file size %lu >= (%s map size limit %ld)/%d -- "
		 "using a larger map size limit",
		 mdb_path, (unsigned long) st.st_size,
		 DICT_TYPE_LMDB, (long) dict_lmdb_map_size,
		 LMDB_SIZE_FUDGE_FACTOR);
	/* Defense against naive optimizers. */
	if (st.st_size < SIZE_T_MAX / LMDB_SIZE_FUDGE_FACTOR)
	    dict_lmdb_map_size = st.st_size * LMDB_SIZE_FUDGE_FACTOR;
	else
	    dict_lmdb_map_size = SIZE_T_MAX;
    }
    if ((status = mdb_env_set_mapsize(env, dict_lmdb_map_size)))
	msg_fatal("env_set_mapsize %s: %s", mdb_path, mdb_strerror(status));

    if ((status = mdb_env_set_maxreaders(env, dict_lmdb_max_readers)))
	msg_fatal("env_set_maxreaders %s: %s", mdb_path, mdb_strerror(status));

    if ((status = mdb_env_open(env, mdb_path, env_flags, 0644)))
	msg_fatal("env_open %s: %s", mdb_path, mdb_strerror(status));

    if ((status = mdb_txn_begin(env, NULL, env_flags & MDB_RDONLY, &txn)))
	msg_fatal("txn_begin %s: %s", mdb_path, mdb_strerror(status));

    /*
     * mdb_open requires a txn, but since the default DB always exists in an
     * LMDB environment, we usually don't need to do anything else with the
     * txn.
     */
    if ((status = mdb_open(txn, NULL, 0, &dbi)))
	msg_fatal("mdb_open %s: %s", mdb_path, mdb_strerror(status));

    /*
     * Cases where we use the mdb_open transaction:
     * 
     * - With O_TRUNC we make the "drop" request before populating the database.
     * 
     * - With DICT_FLAG_BULK_UPDATE we commit the transaction when the database
     * is closed.
     */
    if (open_flags & O_TRUNC) {
	if ((status = mdb_drop(txn, dbi, 0)))
	    msg_fatal("truncate %s: %s", mdb_path, mdb_strerror(status));
	if ((dict_flags & DICT_FLAG_BULK_UPDATE) == 0) {
	    if ((status = mdb_txn_commit(txn)))
		msg_fatal("truncate %s: %s", mdb_path, mdb_strerror(status));
	    txn = NULL;
	}
    } else if ((env_flags & MDB_RDONLY) != 0
	       || (dict_flags & DICT_FLAG_BULK_UPDATE) == 0) {
	mdb_txn_abort(txn);
	txn = NULL;
    }
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

    /* Save the write txn if we opened with O_TRUNC */
    dict_lmdb->txn = txn;

    dict_lmdb->cursor = 0;
    dict_lmdb->key_buf = 0;
    dict_lmdb->val_buf = 0;

    myfree(mdb_path);

    return (DICT_DEBUG (&dict_lmdb->dict));
}

#endif
