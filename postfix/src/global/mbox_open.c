/*++
/* NAME
/*	mbox_open 3
/* SUMMARY
/*	mailbox access
/* SYNOPSIS
/*	#include <mbox_open.h>
/*
/*	int	mbox_open(path, flags, mode, st, user, group, lock_style, why)
/*	const char *path;
/*	int	flags;
/*	int	mode;
/*	struct stat *st;
/*	uid_t	user;
/*	gid_t	group;
/*	int	lock_style;
/*	VSTRING	*why;
/*
/*	void	mbox_release(path, lock_style)
/*	const char *path;
/*	int	lock_style;
/* DESCRIPTION
/*	This module manages access to UNIX mailbox-style files.
/*
/*	mbox_open() acquires exclusive access to the named file.
/*	The \fBpath, flags, mode, st, user, group, why\fR arguments
/*	are passed to the \fBsafe_open\fR() routine. Attempts to change
/*	file ownership will succeed only if the process runs with
/*	adequate effective privileges.
/*	The \fBlock_style\fR argument specifies a lock style from
/*	mbox_lock_mask(). Kernel locks are applied to regular files only.
/*
/*	mbox_release() releases the named mailbox. It is up to the
/*	application to close the file.
/* DIAGNOSTICS
/*	mbox_open() returns a null pointer in case of problems, and
/*	sets errno to EAGAIN if someone else has exclusive access.
/*	Other errors are likely to have a more permanent nature.
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
#include <sys/stat.h>
#include <errno.h>

/* Utility library. */

#include <msg.h>
#include <vstream.h>
#include <vstring.h>
#include <safe_open.h>

/* Global library. */

#include <dot_lockfile.h>
#include <deliver_flock.h>
#include <mbox_conf.h>
#include <mbox_open.h>

/* mbox_open - open mailbox-style file for exclusive access */

VSTREAM *mbox_open(const char *path, int flags, int mode, struct stat * st,
		           uid_t chown_uid, gid_t chown_gid,
		           int lock_style, VSTRING *why)
{
    struct stat local_statbuf;
    VSTREAM *fp;

    /*
     * Create dotlock file. This locking method does not work well over NFS:
     * creating files atomically is a problem, and a successful operation can
     * fail with EEXIST.
     */
    if ((lock_style & MBOX_DOT_LOCK) && dot_lockfile(path, why) < 0) {
	if (errno == EEXIST) {
	    errno = EAGAIN;
	    return (0);
	}

	/*
	 * If file.lock can't be created, ignore the problem. We need this so
	 * that Postfix can deliver as unprivileged user to /dev/null
	 * aliases.
	 */
	if ((lock_style & MBOX_DOT_LOCK_MAY_FAIL) == 0)
	    return (0);
    }

    /*
     * Open or create the target file.
     */
    if (st == 0)
	st = &local_statbuf;
    if ((fp = safe_open(path, flags, mode, st, chown_uid, chown_gid, why)) == 0) {
	if (lock_style & MBOX_DOT_LOCK)
	    dot_unlockfile(path);
	return (0);
    }

    /*
     * Acquire kernel locks, but only if the target is a regular file, in
     * case we're running on some overly pedantic system. flock() locks do
     * not work over NFS; fcntl() locks are supposed to work over NFS, but in
     * the real world, NFS lock daemons often have serious problems.
     */
#define LOCK_FAIL(mbox_lock, myflock_style) ((lock_style & (mbox_lock)) != 0 \
         && deliver_flock(vstream_fileno(fp), (myflock_style), why) < 0)

    if (S_ISREG(st->st_mode)
	&& (LOCK_FAIL(MBOX_FLOCK_LOCK, MYFLOCK_STYLE_FLOCK)
	    || LOCK_FAIL(MBOX_FCNTL_LOCK, MYFLOCK_STYLE_FCNTL))) {
	if (myflock_locked(vstream_fileno(fp)))
	    errno = EAGAIN;
	if (lock_style & MBOX_DOT_LOCK)
	    dot_unlockfile(path);
	return (0);
    }
    return (fp);
}

/* mbox_release - release mailbox exclusive access */

void    mbox_release(const char *path, int lock_style)
{
    if (lock_style & MBOX_DOT_LOCK)
	dot_unlockfile(path);
}
