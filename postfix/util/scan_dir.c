/*++
/* NAME
/*	scan_dir 3
/* SUMMARY
/*	directory scanning
/* SYNOPSIS
/*	#include <scan_dir.h>
/*
/*	SCAN_DIR *scan_dir_open(path)
/*	const char *path;
/*
/*	char	*scan_dir_next(scan)
/*	SCAN_DIR *scan;
/*
/*	char	*scan_dir_path(scan)
/*	SCAN_DIR *scan;
/*
/*	SCAN_DIR *scan_dir_close(scan)
/*	SCAN_DIR *scan;
/* DESCRIPTION
/*	These functions scan directories for names. The "." and
/*	".." names are skipped. Essentially, this is <dirent>
/*	extended with error handling and with knowledge of the
/*	name of the directory being scanned.
/*
/*	scan_dir_open() opens the named directory and
/*	returns a handle for subsequent use.
/*
/*	scan_dir_close() closes the directory and cleans up
/*	and returns a null pointer.
/*
/*	scan_dir_next() returns the next filename in the specified
/*	directory. It skips the "." and ".." entries.
/*
/*	scan_dir_path() returns the name of the directory being scanned.
/* DIAGNOSTICS
/*	All errors are fatal.
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
#include <dirent.h>
#include <string.h>

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#define NAMLEN(dirent) strlen((dirent)->d_name)
#else
#define dirent direct
#define NAMLEN(dirent) (dirent)->d_namlen
#ifdef HAVE_SYS_NDIR_H
#include <sys/ndir.h>
#endif
#ifdef HAVE_SYS_DIR_H
#include <sys/dir.h>
#endif
#ifdef HAVE_NDIR_H
#include <ndir.h>
#endif
#endif

/* Utility library. */

#include "msg.h"
#include "mymalloc.h"
#include "scan_dir.h"

 /*
  * Opaque structure, so we don't have to expose the user to the above #ifdef
  * spaghetti.
  */
struct SCAN_DIR {
    char   *path;
    DIR    *dir;
};

/* scan_dir_path - return the path of the directory being read.  */

char   *scan_dir_path(SCAN_DIR *scan)
{
    return scan->path;
}

/* scan_dir_open - start directory scan */

SCAN_DIR *scan_dir_open(const char *path)
{
    SCAN_DIR *scan;

    scan = (SCAN_DIR *) mymalloc(sizeof(*scan));
    if ((scan->dir = opendir(path)) == 0)
	msg_fatal("open directory %s: %m", path);
    if (msg_verbose > 1)
	msg_info("scan_dir_open: %s", path);
    scan->path = mystrdup(path);
    return (scan);
}

char   *scan_dir_next(SCAN_DIR *scan)
{
    struct dirent *dp;

#define STRNE(x,y)	(strcmp((x),(y)) != 0)

    while ((dp = readdir(scan->dir)) != 0) {
	if (STRNE(dp->d_name, ".") && STRNE(dp->d_name, "..")) {
	    if (msg_verbose > 1)
		msg_info("scan_dir_next: %s", dp->d_name);
	    return (dp->d_name);
	}
    }
    return (0);
}

/* scan_dir_close - terminate directory scan */

SCAN_DIR *scan_dir_close(SCAN_DIR *scan)
{
    if (closedir(scan->dir))
	msg_fatal("close directory %s: %m", scan->path);
    if (msg_verbose > 1)
	msg_info("scan_dir_close: %s", scan->path);
    myfree(scan->path);
    myfree((char *) scan);
    return (0);
}
