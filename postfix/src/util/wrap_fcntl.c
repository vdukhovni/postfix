/*++
/* NAME
/*	wrap_fcntl 3
/* SUMMARY
/*	mockable fcntl/flock wrappers
/* SYNOPSIS
/*	#include <wrap_fcntl.h>
/*
/*	int     wrap_fcntl(
/*	int	fd,
/*	int	cmd,
/*	...)
/*
/*	int     wrap_fcntl(
/*	int	fd,
/*	int	cmd)
/* DESCRIPTION
/*	This module is a NOOP when the NO_MOCK_WRAPPERS macro is defined.
/*
/*	By default this module redirects fcntl() and flock() calls
/*	to the above listed wrapper functions that may be overridden
/*	with mocks. This arrangement prevents mock fcntl() or flock()
/*	functions from interfering with fcntl() or flock() calls made
/*	by system libraries or by third-party libraries.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>

 /*
  * Utility library.
  */
#include <msg.h>
#include <wrap_fcntl.h>

#ifdef HAS_FCNTL_LOCK

/* wrap_fcntl - mockable wrapper */

int     wrap_fcntl(int fd, int cmd,...)
{
    va_list ap;
    int     ret;

#undef fcntl

    va_start(ap, cmd);
    switch (cmd) {

    case F_GETFD:
    case F_GETFL:
	/* Operations with no third argument. */
	ret = fcntl(fd, cmd);
	break;

    case F_DUPFD:
    case F_SETFD:
    case F_SETFL:
	/* Operations with an integer third argument. */
	ret = fcntl(fd, cmd, va_arg(ap, int));
	break;

    case F_SETLK:
    case F_SETLKW:
    case F_GETLK:
	/* Lock operations. */
	ret = fcntl(fd, cmd, va_arg(ap, struct flock *));
	break;

    default:
	/* Platform-specific, should never be called. */
	msg_warn("%s: unexpected cmd=%d", __func__, cmd);
	ret = -1;
	errno = ENOTSUP;
	break;
    }
    va_end(ap);
    return (ret);
}

#endif					/* HAS_FCNTL_LOCK */

#ifdef HAS_FLOCK_LOCK

/* wrap_flock - mockable wrapper */

int     wrap_flock(int fd, int cmd)
{
#undef flock
    return (flock(fd, cmd));
}

#endif					/* HAS_FLOCK_LOCK */
