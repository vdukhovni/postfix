/*++
/* NAME
/*	mkmap_lmdb 3
/* SUMMARY
/*	create or open database, LMDB style
/* SYNOPSIS
/*	#include <mkmap.h>
/*
/*	MKMAP	*mkmap_lmdb_open(path)
/*	const char *path;
/*
/* DESCRIPTION
/*	This module implements support for creating LMDB databases.
/*
/*	mkmap_lmdb_open() takes a file name, appends the ".lmdb"
/*	suffix, and does whatever initialization is required
/*	before the OpenLDAP LMDB open routine is called.
/*
/*	All errors are fatal.
/* SEE ALSO
/*	dict_lmdb(3), LMDB dictionary interface.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Howard Chu
/*	Symas Corporation
/*--*/

/* System library. */

#include <sys_defs.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <stringops.h>
#include <dict.h>
#include <dict_lmdb.h>
#include <myflock.h>
#include <warn_stat.h>

#if defined(HAS_LMDB) && defined(SNAPSHOT)
#ifdef PATH_LMDB_H
#include PATH_LMDB_H
#else
#include <lmdb.h>
#endif

/* Global library. */

#include <mail_conf.h>
#include <mail_params.h>

/* Application-specific. */

#include "mkmap.h"

int     var_proc_limit;

/* mkmap_lmdb_open */

MKMAP  *mkmap_lmdb_open(const char *path)
{
    MKMAP  *mkmap = (MKMAP *) mymalloc(sizeof(*mkmap));
    static const CONFIG_INT_TABLE int_table[] = {
	VAR_PROC_LIMIT, DEF_PROC_LIMIT, &var_proc_limit, 1, 0,
	0,
    };

    get_mail_conf_int_table(int_table);

    /*
     * XXX Why is this not set in mail_params.c (with proper #ifdefs)?
     * 
     * Override the default per-table map size for map (re)builds.
     * 
     * lmdb_map_size is defined in util/dict_lmdb.c and defaults to 10MB. It
     * needs to be large enough to contain the largest tables in use.
     * 
     * XXX This should be specified via the DICT interface so that the buffer
     * size becomes an object property, instead of being specified by poking
     * a global variable so that it becomes a class property.
     * 
     * XXX Wietse disagrees: storage infrastructure that requires up-front
     * max-size information is evil. This unlike Postfix (e.g. line length or
     * process count) limits which are a defense against out-of-control or
     * malicious external actors.
     * 
     * XXX Need to check that an existing table can be rebuilt with a larger
     * size limit than was used for the initial build.
     */
    dict_lmdb_map_size = var_lmdb_map_size;

    /*
     * XXX Why is this not set in mail_params.c (with proper #ifdefs)?
     * 
     * Set the max number of concurrent readers per table. This is the
     * maximum number of postfix processes, plus some extra for CLI users.
     * 
     * XXX Postfix uses asynchronous or blocking I/O with single-threaded
     * processes so this limit will never be reached, assuming that the limit
     * is a per-client property, not a shared database property.
     */
    dict_lmdb_max_readers = var_proc_limit * 2 + 16;

    /*
     * Fill in the generic members.
     */
    mkmap->open = dict_lmdb_open;
    mkmap->after_open = 0;
    mkmap->after_close = 0;

    /*
     * LMDB uses MVCC so it needs no special lock management here.
     */

    return (mkmap);
}

#endif
