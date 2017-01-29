/*++
/* NAME
/*	unsafe 3
/* SUMMARY
/*	are we running at non-user privileges
/* SYNOPSIS
/*	#include <safe.h>
/*
/*	int	unsafe()
/* DESCRIPTION
/*	The \fBunsafe()\fR routine attempts to determine if the process runs
/*	with any privileges that do not belong to the user. The purpose is
/*	to make it easy to taint any user-provided data such as the current
/*	working directory, the process environment, etcetera.
/*
/*	On UNIX systems, the result is true when any of the following
/*	conditions is true:
/* .IP \(bu
/*	The real UID is non-zero.
/* .IP \(bu
/*	The effective UID is non-zero.
/* .PP
/*	Additionally, any of the following conditions must be true:
/* .IP \(bu
/*	The issetuid kernel flag is non-zero (on systems that support
/*	this concept).
/* .IP \(bu
/*	The real and effective user id differ.
/* .IP \(bu
/*	The real and effective group id differ.
/* .PP
/*	Thus, when a process runs as the super-user, it is excluded
/*	from privilege-escalation concerns, but only if both real
/*	UID and effective UID are zero.
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
#include <unistd.h>

/* Utility library. */

#include "safe.h"

/* unsafe - can we trust user-provided environment, working directory, etc. */

int     unsafe(void)
{
    return ((getuid() || geteuid())
	    && (geteuid() != getuid()
#ifdef HAS_ISSETUGID
		|| issetugid()
#endif
		|| getgid() != getegid()));
}
