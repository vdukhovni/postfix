/*++
/* NAME
/*	mbox_open 3
/* SUMMARY
/*	mailbox access
/* SYNOPSIS
/*	#include <mbox_open.h>
/*
/*	typedef struct {
/* .in +4
/*	/* public members... */
/*	VSTREAM	*fp;
/* .in -4
/*	} MBOX;
/*
/*	MBOX	*mbox_open(path, flags, mode, st, user, group, lock_style, why)
/*	const char *path;
/*	int	flags;
/*	int	mode;
/*	struct stat *st;
/*	uid_t	user;
/*	gid_t	group;
/*	int	lock_style;
/*	VSTRING	*why;
/*
/*	void	mbox_release(mbox)
/*	MBOX	*mbox;
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
/*	The result is a handle that must be destroyed by mbox_release().
/*
/*	mbox_release() releases the named mailbox. It is up to the
/*	application to close the stream.
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
#include <iostuff.h>
#include <mymalloc.h>

/* Global library. */

#include <dot_lockfile.h>
#include <deliver_flock.h>
#include <mbox_conf.h>
#include <mbox_open.h>

/* mbox_open - open mailbox-style file for exclusive access */

MBOX   *mbox_open(const char *path, int flags, int mode, struct stat * st,
		          uid_t chown_uid, gid_t chown_gid,
		          int lock_style, VSTRING *why)
{
    struct stat local_statbuf;
    MBOX   *mp;
    int     locked = 0;
    VSTREAM *fp;

    /*
     * Create dotlock file. This locking method does not work well over NFS:
     * creating files atomically is a problem, and a successful operation can
     * fail with EEXIST.
     * 
     * If file.lock can't be created, ignore the problem if the application says
     * so. We need this so that Postfix can deliver as unprivileged user to
     * /dev/null style aliases. Alternatively, we could open the file first,
     * and dot-lock the file only if it is a regular file, just like we do
     * with kernel locks.
     */
    if (lock_style & MBOX_DOT_LOCK) {
	if (dot_lockfile(path, why) == 0) {
	    locked |= MBOX_DOT_LOCK;
	} else {
	    if (errno == EEXIST) {
		errno = EAGAIN;
		return (0);
	    }
	    if ((lock_style & MBOX_DOT_LOCK_MAY_FAIL) == 0) {
		return (0);
	    }
	}
    }

    /*
     * Open or create the target file. In case of a privileged open, the
     * privileged user may be attacked through an unsafe parent directory. In
     * case of an unprivileged open, the mail system may be attacked by a
     * malicious user-specified path, and the unprivileged user may be
     * attacked through an unsafe parent directory. Open non-blocking to fend
     * off attacks involving FIFOs and other weird targets.
     */
    if (st == 0)
	st = &local_statbuf;
    if ((fp = safe_open(path, flags, mode | O_NONBLOCK, st,
			chown_uid, chown_gid, why)) == 0) {
	if (locked & MBOX_DOT_LOCK)
	    dot_unlockfile(path);
	return (0);
    }
    non_blocking(vstream_fileno(fp), BLOCKING);
    close_on_exec(vstream_fileno(fp), CLOSE_ON_EXEC);

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
	if (locked & MBOX_DOT_LOCK)
	    dot_unlockfile(path);
	return (0);
    }
    mp = (MBOX *) mymalloc(sizeof(*mp));
    mp->path = mystrdup(path);
    mp->fp = fp;
    mp->locked = locked;
    return (mp);
}

/* mbox_release - release mailbox exclusive access */

void    mbox_release(MBOX *mp)
{
    if (mp->locked & MBOX_DOT_LOCK)
	dot_unlockfile(mp->path);
    myfree(mp->path);
    myfree((char *) mp);
}
