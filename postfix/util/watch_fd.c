/*++
/* NAME
/*	watch_fd 3
/* SUMMARY
/*	monitor file descriptors for change
/* SYNOPSIS
/*	#include <watch_fd.h>
/*
/*	void	watch_fd_register(fd)
/*	int	fd;
/*
/*	void	watch_fd_remove(fd)
/*	int	fd;
/*
/*	int	watch_fd_changed()
/* DESCRIPTION
/*	This module monitors file modification times of arbitrary file
/*	descriptors.
/*
/*	watch_fd_register() records information about the specified
/*	file descriptor.
/*
/*	watch_fd_remove() releases storage allocated by watch_fd_register().
/*
/*	watch_fd_changed() returns non-zero if any of the registered
/* SEE ALSO
/*	msg(3) diagnostics interface
/* DIAGNOSTICS
/*	Problems are reported via the msg(3) diagnostics routines:
/*	the requested amount of memory is not available; improper use
/*	is detected; other fatal errors.
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

/* Utility library. */

#include <msg.h>
#include <binhash.h>
#include <mymalloc.h>
#include <watch_fd.h>

/* Application-specific. */

typedef struct {
    int     fd;				/* file descriptor */
    time_t  mtime;			/* initial modification time */
} WATCH_FD;

static BINHASH *watch_fd_table;

/* watch_fd_register - register file descriptor */

void    watch_fd_register(int fd)
{
    char   *myname = "watch_fd_enter";
    WATCH_FD *info;
    struct stat st;

    if (fstat(fd, &st) < 0)
	msg_fatal("%s: fstat: %m", myname);

    info = (WATCH_FD *) mymalloc(sizeof(*info));
    info->fd = fd;
    info->mtime = st.st_mtime;

    if (watch_fd_table == 0)
	watch_fd_table = binhash_create(0);
    if (binhash_find(watch_fd_table, (char *) &fd, sizeof(fd)))
	msg_panic("%s: entry %d exists", myname, fd);
    binhash_enter(watch_fd_table, (char *) &fd, sizeof(fd), (char *) info);
}

/* watch_fd_remove - remove file descriptor */

void    watch_fd_remove(int fd)
{
    binhash_delete(watch_fd_table, (char *) &fd, sizeof(fd), myfree);
}

/* watch_fd_changed - see if any file has changed */

int     watch_fd_changed(void)
{
    char   *myname = "watch_fd_changed";
    struct stat st;
    BINHASH_INFO **ht_info_list;
    BINHASH_INFO **ht;
    BINHASH_INFO *h;
    WATCH_FD *info;
    int     status;

    ht_info_list = binhash_list(watch_fd_table);
    for (status = 0, ht = ht_info_list; status == 0 && (h = *ht) != 0; ht++) {
	info = (WATCH_FD *) h->value;
	if (fstat(info->fd, &st) < 0)
	    msg_fatal("%s: fstat: %m", myname);
	status = (st.st_mtime != info->mtime);
    }
    myfree((char *) ht_info_list);
    return (status);
}
