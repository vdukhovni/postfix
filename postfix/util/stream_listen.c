/*++
/* NAME
/*	stream_listen 3
/* SUMMARY
/*	start stream listener
/* SYNOPSIS
/*	#include <listen.h>
/*
/*	int	stream_listen(path, backlog, block_mode)
/*	const char *path;
/*	int	backlog;
/*	int	block_mode;
/*
/*	int	stream_accept(fd)
/*	int	fd;
/* DESCRIPTION
/*	This module implements a substitute local IPC for systems that do
/*	not have properly-working UNIX-domain sockets.
/*
/*	stream_listen() creates a listener endpoint with the specified
/*	permissions, and returns a file descriptor to be used for accepting
/*	connections.
/*
/*	stream_accept() accepts a connection.
/*
/*	Arguments:
/* .IP path
/*	Null-terminated string with connection destination.
/* .IP backlog
/*	This argument exists for compatibility and is ignored.
/* .IP block_mode
/*	This argument exists for compatibility and is ignored.
/*	blocking mode.
/* .IP fd
/*	File descriptor returned by stream_listen().
/* DIAGNOSTICS
/*	Fatal errors: stream_listen() aborts upon any system call failure.
/*	stream_accept() leaves all error handling up to the caller.
/* BUGS
/*	This implementation leaks one file descriptor. This is fixed when
/*	endpoints become objects rather than file descriptors.
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

/* System interfaces. */

#include <sys_defs.h>

#ifdef STREAM_CONNECTIONS

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stropts.h>
#include <fcntl.h>

#endif

/* Utility library. */

#include "msg.h"
#include "listen.h"

/* stream_listen - create stream listener */

int     stream_listen(const char *path, int unused_backlog, 
int unused_block_mode)
{
#ifdef STREAM_CONNECTIONS
    char   *myname = "stream_listen";
    static int pair[2];
    int     fd;

    /*
     * Initialize: create the specified endpoint with the right permissions.
     */
#define PERMS 0666
    if (unlink(path) && errno != ENOENT)
	msg_fatal("%s: remove %s: %m", myname, path);
    if ((fd = open(path, PERMS, O_CREAT | O_TRUNC | O_WRONLY)) < 0)
	msg_fatal("%s: create file %s: %m", myname, path);
    if (fchmod(fd, PERMS) < 0)
	msg_fatal("%s: chmod 0%o: %m", myname, PERMS);
    if (close(fd) < 0)
	msg_fatal("%s: close file %s:  %m", myname, path);

    /*
     * Associate one pipe end with the file just created. See: Richard
     * Stevens, Advanced Programming in the UNIX Environment Ch. 15.5.1
     * 
     * On Solaris 2.4/SPARC, this gives us a "listen queue" of some 460
     * connections.
     */
    if (pipe(pair) < 0)
	msg_fatal("%s: create pipe: %m", myname);
    if (ioctl(pair[1], I_PUSH, "connld") < 0)
	msg_fatal("%s: push connld module: %m", myname);
    if (fattach(pair[1], path) < 0)
	msg_fatal("%s: fattach %s: %m", myname, path);

    /*
     * Return one end, and leak the other. This will be fixed when all
     * endpoints are objects instead of bare file descriptors.
     */
    return (pair[0]);
#else
    msg_fatal("stream connections are not implemented");
#endif
}

/* stream_accept - accept stream connection */

int     stream_accept(int fd)
{
#ifdef STREAM_CONNECTIONS
    struct strrecvfd fdinfo;

    /*
     * This will return EAGAIN on a non-blocking stream when someone else
     * snatched the connection from us.
     */
    if (ioctl(fd, I_RECVFD, &fdinfo) < 0)
	return (-1);
    return (fdinfo.fd);
#else
	      msg_fatal("stream connections are not implemented");
#endif
}
