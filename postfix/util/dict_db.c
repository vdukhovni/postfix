/*++
/* NAME
/*	dict_db 3
/* SUMMARY
/*	dictionary manager interface to DB files
/* SYNOPSIS
/*	#include <dict_db.h>
/*
/*	DICT	*dict_hash_open(path, flags)
/*	const char *path;
/*	int	flags;
/*
/*	DICT	*dict_btree_open(path, flags)
/*	const char *path;
/*	int	flags;
/* DESCRIPTION
/*	dict_XXX_open() opens the specified DB database.  The result is
/*	a pointer to a structure that can be used to access the dictionary
/*	using the generic methods documented in dict_open(3).
/*
/*	Arguments:
/* .IP path
/*	The database pathname, not including the ".db" suffix.
/* .IP flags
/*	flags passed to dbopen().
/* SEE ALSO
/*	dict(3) generic dictionary manager
/* DIAGNOSTICS
/*	Fatal errors: cannot open file, write error, out of memory.
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

#include "sys_defs.h"

#ifdef HAS_DB

/* System library. */

#include <limits.h>
#ifdef PATH_DB_H
#include PATH_DB_H
#else
#include <db.h>
#endif
#include <string.h>

/* Utility library. */

#include "msg.h"
#include "mymalloc.h"
#include "vstring.h"
#include "stringops.h"
#include "iostuff.h"
#include "dict.h"
#include "dict_db.h"

/* Application-specific. */

typedef struct {
    DICT    dict;			/* generic members */
    int     flags;			/* see below */
    DB     *db;				/* open db file */
    char   *path;			/* pathname */
} DICT_DB;

#define DICT_DB_TRY0NULL	(1<<0)
#define DICT_DB_TRY1NULL	(1<<1)

#define DICT_DB_CACHE_SIZE	(1024 * 1024)
#define DICT_DB_NELM	4096

/* dict_db_lookup - find database entry */

static const char *dict_db_lookup(DICT *dict, const char *name)
{
    DICT_DB *dict_db = (DICT_DB *) dict;
    DB     *db = dict_db->db;
    DBT     db_key;
    DBT     db_value;
    int     status;
    static VSTRING *buf;

    /*
     * See if this DB file was written with one null byte appended to key and
     * value.
     */
    if (dict_db->flags & DICT_DB_TRY1NULL) {
	db_key.data = (void *) name;
	db_key.size = strlen(name) + 1;
	if ((status = db->get(db, &db_key, &db_value, 0)) < 0)
	    msg_fatal("error reading %s: %m", dict_db->path);
	if (status == 0) {
	    dict_db->flags &= ~DICT_DB_TRY0NULL;
	    return (db_value.data);
	}
    }

    /*
     * See if this DB file was written with no null byte appended to key and
     * value.
     */
    if (dict_db->flags & DICT_DB_TRY0NULL) {
	db_key.data = (void *) name;
	db_key.size = strlen(name);
	if ((status = db->get(db, &db_key, &db_value, 0)) < 0)
	    msg_fatal("error reading %s: %m", dict_db->path);
	if (status == 0) {
	    if (buf == 0)
		buf = vstring_alloc(10);
	    vstring_strncpy(buf, db_value.data, db_value.size);
	    dict_db->flags &= ~DICT_DB_TRY1NULL;
	    return (vstring_str(buf));
	}
    }
    return (0);
}

/* dict_db_update - add or update database entry */

static void dict_db_update(DICT *dict, const char *name, const char *value)
{
    DICT_DB *dict_db = (DICT_DB *) dict;
    DB     *db = dict_db->db;
    DBT     db_key;
    DBT     db_value;
    int     status;

    db_key.data = (void *) name;
    db_value.data = (void *) value;
    db_key.size = strlen(name);
    db_value.size = strlen(value);

    /*
     * If undecided about appending a null byte to key and value, choose a
     * default depending on the platform.
     */
    if ((dict_db->flags & DICT_DB_TRY1NULL)
	&& (dict_db->flags & DICT_DB_TRY0NULL)) {
#ifdef DB_NO_TRAILING_NULL
	dict_db->flags = DICT_DB_TRY0NULL;
#else
	dict_db->flags = DICT_DB_TRY1NULL;
#endif
    }

    /*
     * Optionally append a null byte to key and value.
     */
    if (dict_db->flags & DICT_DB_TRY1NULL) {
	db_key.size++;
	db_value.size++;
    }

    /*
     * Do the update.
     */
    if ((status = db->put(db, &db_key, &db_value, R_NOOVERWRITE)) < 0)
	msg_fatal("error writing %s: %m", dict_db->path);
    if (status) {
	if (dict->flags & DICT_FLAG_DUP_IGNORE)
	     /* void */ ;
	else if (dict->flags & DICT_FLAG_DUP_WARN)
	    msg_warn("%s: duplicate entry: \"%s\"", dict_db->path, name);
	else
	    msg_fatal("%s: duplicate entry: \"%s\"", dict_db->path, name);
    }
}

/* dict_db_close - close data base */

static void dict_db_close(DICT *dict)
{
    DICT_DB *dict_db = (DICT_DB *) dict;

    if (dict_db->db->close(dict_db->db) < 0)
	msg_fatal("close database %s: %m", dict_db->path);
    myfree(dict_db->path);
    myfree((char *) dict_db);
}

/* dict_db_open - open data base */

static DICT *dict_db_open(const char *path, int flags, int type, void *tweak)
{
    DICT_DB *dict_db;
    DB     *db;
    char   *db_path;

    db_path = concatenate(path, ".db", (char *) 0);
    if ((db = dbopen(db_path, flags, 0644, type, tweak)) == 0)
	msg_fatal("open database %s: %m", db_path);

    dict_db = (DICT_DB *) mymalloc(sizeof(*dict_db));
    dict_db->dict.lookup = dict_db_lookup;
    dict_db->dict.update = dict_db_update;
    dict_db->dict.close = dict_db_close;
    dict_db->dict.fd = db->fd(db);
    close_on_exec(dict_db->dict.fd, CLOSE_ON_EXEC);
    dict_db->flags = DICT_DB_TRY1NULL | DICT_DB_TRY0NULL;
    dict_db->db = db;
    dict_db->path = db_path;
    return (&dict_db->dict);
}

/* dict_hash_open - create association with data base */

DICT   *dict_hash_open(const char *path, int flags)
{
    HASHINFO tweak;

    memset((char *) &tweak, 0, sizeof(tweak));
    tweak.nelem = DICT_DB_NELM;
    tweak.cachesize = DICT_DB_CACHE_SIZE;
    return (dict_db_open(path, flags, DB_HASH, (void *) &tweak));
}

/* dict_btree_open - create association with data base */

DICT   *dict_btree_open(const char *path, int flags)
{
    BTREEINFO tweak;

    memset((char *) &tweak, 0, sizeof(tweak));
    tweak.cachesize = DICT_DB_CACHE_SIZE;

    return (dict_db_open(path, flags, DB_BTREE, (void *) &tweak));
}

#endif
