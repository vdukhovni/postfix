/*++
/* NAME
/*	stream_connect 3
/* SUMMARY
/*	connect to stream listener
/* SYNOPSIS
/*	#include <connect.h>
/*
/*	int	stream_connect(path, block_mode, timeout)
/*	const char *path;
/*	int	block_mode;
/*	int	timeout;
/* DESCRIPTION
/*	stream_connect() connects to a stream listener for the specified
/*	pathname, and returns the resulting file descriptor.
/*
/*	Arguments:
/* .IP path
/*	Null-terminated string with listener endpoint name.
/* .IP block_mode
/*	Either NON_BLOCKING for a non-blocking stream, or BLOCKING for
/*	blocking mode. However, a stream connection succeeds or fails
/*	immediately.
/* .IP timeout
/*	This argument is ignored; it is present for compatibility with
/*	other interfaces. Stream connections succeed or fail immediately.
/* DIAGNOSTICS
/*	The result is -1 in case the connection could not be made.
/*	Fatal errors: other system call failures.
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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/* Utility library. */

#include <msg.h>
#include <connect.h>

/* stream_connect - connect to stream listener */

int     stream_connect(const char *path, int block_mode, int unused_timeout)
{
#ifdef STREAM_CONNECTIONS
    char   *myname = "stream_connect";
    struct stat st;
    int     fd;
    int     flags;

    /*
     * The requested file system object must exist, otherwise we can't reach
     * the server.
     */
    if (block_mode == NON_BLOCKING)
	flags = O_RDWR | O_NONBLOCK;
    else
	flags = O_RDWR;
    if ((fd = open(path, flags, 0)) < 0)
	return (-1);

    /*
     * XXX Horror. If the open() result is a regular file, no server was
     * listening. In this case we simulate what would have happened with
     * UNIX-domain sockets.
     */
    if (fstat(fd, &st) < 0)
	msg_fatal("%s: fstat: %m", myname);
    if (S_ISREG(st.st_mode)) {
	close(fd);
	errno = ECONNREFUSED;
	return (-1);
    }

    /*
     * This is for {unix,inet}_connect() compatibility.
     */
    if (block_mode == NON_BLOCKING)
	non_blocking(fd, NON_BLOCKING);

    /*
     * No trouble detected, so far.
     */
    return (fd);
#else
    msg_fatal("stream connections are not implemented");
#endif
}

