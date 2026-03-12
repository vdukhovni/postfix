/*++
/* NAME
/*	mail_version 3
/* SUMMARY
/*	time-dependent probe sender addresses
/* SYNOPSIS
/*	#include <mail_version.h>
/*
/*	typedef struct {
/*	    char   *program;		/* postfix */
/*	    int     major;		/* 2 */
/*	    int     minor;		/* 9 */
/*	    int     patch;		/* patchlevel or -1 */
/*	    char   *snapshot;		/* null or snapshot info */
/*	} MAIL_VERSION;
/*
/*	MAIL_VERSION *mail_version_parse(version_string, why)
/*	const char *version_string;
/*	const char **why;
/*
/*	void	mail_version_free(mp)
/*	MAIL_VERSION *mp;
/*
/*	const char *get_mail_version()
/*
/*	int	check_mail_version(version_string)
/*	const char *version_string;
/* DESCRIPTION
/*	This module understands the format of Postfix version strings
/*	(for example the default value of "mail_version"), and
/*	provides support to compare the compile-time version of a
/*	Postfix program with the run-time version of a Postfix
/*	library. Apparently, some distributions don't use proper
/*	so-number versioning, causing programs to fail erratically
/*	after an update replaces the library but not the program.
/*
/*	A Postfix version string consists of two or three parts
/*	separated by a single "-" character:
/* .IP \(bu
/*	The first part is a string with the program name.
/* .IP \(bu
/*	The second part is the program version: either two or three
/*	non-negative integer numbers separated by single "."
/*	character. Stable releases have a major version, minor
/*	version and patchlevel; experimental releases (snapshots)
/*	have only major and minor version numbers.
/* .IP \(bu
/*	The third part is ignored with a stable release, otherwise
/*	it is a string with the snapshot release date plus some
/*	optional information.
/*
/*	mail_version_parse() parses a version string.
/*
/*	get_mail_version() returns the version string (the value
/*	of DEF_MAIL_VERSION) that is compiled into the library.
/*
/*	check_mail_version() compares the caller's version string
/*	(usually the value of DEF_MAIL_VERSION) that is compiled
/*	into the caller, and logs a warning when the strings differ.
/* DIAGNOSTICS
/*	In the case of a parsing error, mail_version_parse() returns
/*	a null pointer, and sets the why argument to a string with
/*	problem details.
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
#include <stdlib.h>
#include <errno.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <stringops.h>
#include <split_at.h>

/* Global library. */

#include <mail_version.h>

/* check_mail_version - compare caller version with library version */

void    check_mail_version(const char *version_string)
{
    if (strcmp(version_string, DEF_MAIL_VERSION) != 0)
	msg_warn("Postfix library version mis-match: wanted %s, found %s",
		 version_string, DEF_MAIL_VERSION);
}
