/*++
/* NAME
/*	dict_dbm 3
/* SUMMARY
/*	dictionary manager interface to DBM files
/* SYNOPSIS
/*	#include <dict_dbm.h>
/*
/*	DICT	*dict_dbm_open(path, open_flags, dict_flags)
/*	const char *name;
/*	const char *path;
/*	int	open_flags;
/*	int	dict_flags;
/* DESCRIPTION
/*	dict_dbm_open() opens the named DBM database and makes it available
/*	via the generic interface described in dict_open(3).
/* DIAGNOSTICS
/*	Fatal errors: cannot open file, file write error, out of memory.
/* SEE ALSO
/*	dict(3) generic dictionary manager
/*	ndbm(3) data base subroutines
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

#ifdef HAS_DBM

/* System library. */

#include <ndbm.h>
#include <string.h>

/* Utility library. */

#include "msg.h"
#include "mymalloc.h"
#include "htable.h"
#include "iostuff.h"
#include "vstring.h"
#include "myflock.h"
#include "dict.h"
#include "dict_dbm.h"

/* Application-specific. */

typedef struct {
    DICT    dict;			/* generic members */
    DBM    *dbm;			/* open database */
    char   *path;			/* pathname */
} DICT_DBM;

/* dict_dbm_lookup - find database entry */

static const char *dict_dbm_lookup(DICT *dict, const char *name)
{
    DICT_DBM *dict_dbm = (DICT_DBM *) dict;
    datum   dbm_key;
    datum   dbm_value;
    static VSTRING *buf;
    const char *result = 0;

    dict_errno = 0;

    /*
     * Acquire an exclusive lock.
     */
    if ((dict->flags & DICT_FLAG_LOCK) && myflock(dict->fd, MYFLOCK_SHARED) < 0)
	msg_fatal("%s: lock dictionary: %m", dict_dbm->path);

    /*
     * See if this DBM file was written with one null byte appended to key
     * and value.
     */
    if (dict->flags & DICT_FLAG_TRY1NULL) {
	dbm_key.dptr = (void *) name;
	dbm_key.dsize = strlen(name) + 1;
	dbm_value = dbm_fetch(dict_dbm->dbm, dbm_key);
	if (dbm_value.dptr != 0) {
	    dict->flags &= ~DICT_FLAG_TRY0NULL;
	    result = dbm_value.dptr;
	}
    }

    /*
     * See if this DBM file was written with no null byte appended to key and
     * value.
     */
    if (result == 0 && (dict->flags & DICT_FLAG_TRY0NULL)) {
	dbm_key.dptr = (void *) name;
	dbm_key.dsize = strlen(name);
	dbm_value = dbm_fetch(dict_dbm->dbm, dbm_key);
	if (dbm_value.dptr != 0) {
	    if (buf == 0)
		buf = vstring_alloc(10);
	    vstring_strncpy(buf, dbm_value.dptr, dbm_value.dsize);
	    dict->flags &= ~DICT_FLAG_TRY1NULL;
	    result = vstring_str(buf);
	}
    }

    /*
     * Release the exclusive lock.
     */
    if ((dict->flags & DICT_FLAG_LOCK) && myflock(dict->fd, MYFLOCK_NONE) < 0)
	msg_fatal("%s: unlock dictionary: %m", dict_dbm->path);

    return (result);
}

/* dict_dbm_update - add or update database entry */

static void dict_dbm_update(DICT *dict, const char *name, const char *value)
{
    DICT_DBM *dict_dbm = (DICT_DBM *) dict;
    datum   dbm_key;
    datum   dbm_value;
    int     status;

    dbm_key.dptr = (void *) name;
    dbm_value.dptr = (void *) value;
    dbm_key.dsize = strlen(name);
    dbm_value.dsize = strlen(value);

    /*
     * If undecided about appending a null byte to key and value, choose a
     * default depending on the platform.
     */
    if ((dict->flags & DICT_FLAG_TRY1NULL)
	&& (dict->flags & DICT_FLAG_TRY0NULL)) {
#ifdef DBM_NO_TRAILING_NULL
	dict->flags &= ~DICT_FLAG_TRY1NULL;
	dict->flags |= DICT_FLAG_TRY0NULL;
#else
	dict->flags &= ~DICT_FLAG_TRY0NULL;
	dict->flags |= DICT_FLAG_TRY1NULL;
#endif
    }

    /*
     * Optionally append a null byte to key and value.
     */
    if (dict->flags & DICT_FLAG_TRY1NULL) {
	dbm_key.dsize++;
	dbm_value.dsize++;
    }

    /*
     * Acquire an exclusive lock.
     */
    if ((dict->flags & DICT_FLAG_LOCK) && myflock(dict->fd, MYFLOCK_EXCLUSIVE) < 0)
	msg_fatal("%s: lock dictionary: %m", dict_dbm->path);

    /*
     * Do the update.
     */
    if ((status = dbm_store(dict_dbm->dbm, dbm_key, dbm_value, DBM_INSERT)) < 0)
	msg_fatal("error writing DBM database %s: %m", dict_dbm->path);
    if (status) {
	if (dict->flags & DICT_FLAG_DUP_IGNORE)
	     /* void */ ;
	else if (dict->flags & DICT_FLAG_DUP_WARN)
	    msg_warn("%s: duplicate entry: \"%s\"", dict_dbm->path, name);
	else
	    msg_fatal("%s: duplicate entry: \"%s\"", dict_dbm->path, name);
    }

    /*
     * Release the exclusive lock.
     */
    if ((dict->flags & DICT_FLAG_LOCK) && myflock(dict->fd, MYFLOCK_NONE) < 0)
	msg_fatal("%s: unlock dictionary: %m", dict_dbm->path);
}

/* dict_dbm_close - disassociate from data base */

static void dict_dbm_close(DICT *dict)
{
    DICT_DBM *dict_dbm = (DICT_DBM *) dict;

    dbm_close(dict_dbm->dbm);
    myfree(dict_dbm->path);
    myfree((char *) dict_dbm);
}

/* dict_dbm_open - open DBM data base */

DICT   *dict_dbm_open(const char *path, int open_flags, int dict_flags)
{
    DICT_DBM *dict_dbm;
    DBM    *dbm;

    /*
     * XXX SunOS 5.x has no const in dbm_open() prototype.
     */
    if ((dbm = dbm_open((char *) path, open_flags, 0644)) == 0)
	msg_fatal("open database %s.{dir,pag}: %m", path);

    dict_dbm = (DICT_DBM *) mymalloc(sizeof(*dict_dbm));
    dict_dbm->dict.lookup = dict_dbm_lookup;
    dict_dbm->dict.update = dict_dbm_update;
    dict_dbm->dict.close = dict_dbm_close;
    dict_dbm->dict.fd = dbm_dirfno(dbm);
    close_on_exec(dict_dbm->dict.fd, CLOSE_ON_EXEC);
    dict_dbm->dict.flags = dict_flags | DICT_FLAG_FIXED;
    if ((dict_flags & (DICT_FLAG_TRY0NULL | DICT_FLAG_TRY1NULL)) == 0)
	dict_dbm->dict.flags |= (DICT_FLAG_TRY0NULL | DICT_FLAG_TRY1NULL);
    dict_dbm->dbm = dbm;
    dict_dbm->path = mystrdup(path);

    return (&dict_dbm->dict);
}

#endif
