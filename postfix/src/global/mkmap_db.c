/*++
/* NAME
/*	mkmap_db 3
/* SUMMARY
/*	create or open database, DB style
/* SYNOPSIS
/*	#include <mkmap.h>
/*
/*	MKMAP	*mkmap_hash_open(path)
/*	const char *path;
/*
/*	MKMAP	*mkmap_btree_open(path)
/*	const char *path;
/* DESCRIPTION
/*	This module implements support for creating DB databases.
/*
/*	mkmap_hash_open() and mkmap_btree_open() take a file name,
/*	append the ".db" suffix, and create or open the named DB
/*	database. This routine is a DB-specific helper for the more
/*	general mkmap_open() interface.
/*
/*	All errors are fatal.
/* SEE ALSO
/*	dict_db(3), DB dictionary interface.
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
#include <mymalloc.h>
#include <stringops.h>
#include <dict.h>
#include <dict_db.h>

/* Global library. */

#include <mail_params.h>

/* Application-specific. */

#include "mkmap.h"

#ifdef HAS_DB
#ifdef PATH_DB_H
#include PATH_DB_H
#else
#include <db.h>
#endif

/* mkmap_db_open - create or open database */

static MKMAP *mkmap_db_open(const char *path,
			          DICT *(*db_open) (const char *, int, int))
{
    MKMAP  *mkmap = (MKMAP *) mymalloc(sizeof(*mkmap));

    /*
     * Override the default mpool size for map (re)builds.
     * 
     * db_mpool_size" is defined in util/dict_db.c and defaults to 256K, which
     * works well for the lookup code.
     * 
     * We use a larger memory pool when building ".db" files. For "hash" files
     * performance degrades rapidly unless the memory pool is O(file size).
     * 
     * For "btree" files peformance is good with sorted input even for small
     * memory pools, but with random input degrades rapidly unless the memory
     * pool is O(file size).
     * 
     * XXX This should be specified via the DICT interface so that the buffer
     * size becomes an object property, instead of being specified by poking
     * a global variable so that it becomes a class property.
     */
    dict_db_mpool_size = var_db_create_buf;

    /*
     * Fill in the generic members.
     */
    mkmap->lock_file = concatenate(path, ".db", (char *) 0);
    mkmap->open = db_open;

    /*
     * Unfortunately, not all systems that might support db databases do
     * support locking on open(), so we open the file before updating it.
     */
    if ((mkmap->lock_fd = open(mkmap->lock_file, O_CREAT | O_RDWR, 0644)) < 0)
	msg_fatal("open %s: %m", mkmap->lock_file);

    return (mkmap);
}

/* mkmap_hash_open - create or open hashed DB file */

MKMAP  *mkmap_hash_open(const char *path)
{
    return (mkmap_db_open(path, dict_hash_open));
}

/* mkmap_btree_open - create or open btree DB file */

MKMAP  *mkmap_btree_open(const char *path)
{
    return (mkmap_db_open(path, dict_btree_open));
}

#endif
