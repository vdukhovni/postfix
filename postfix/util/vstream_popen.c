/*++
/* NAME
/*	vstream_popen 3
/* SUMMARY
/*	open stream to child process
/* SYNOPSIS
/*	#include <vstream.h>
/*
/*	VSTREAM	*vstream_popen(command, flags)
/*	cont char *command;
/*	int	flags;
/*
/*	int	vstream_pclose(stream)
/*	VSTREAM	*stream;
/* DESCRIPTION
/*	vstream_popen() opens a one-way or two-way stream to the specified
/*	\fIcommand\fR, which is executed by a child process. The \fIflags\fR
/*	argument is as with vstream_fopen(). The child's standard input and
/*	standard output are redirected to the stream, which is based on a
/*	socketpair.
/*
/*	vstream_pclose() closes the named stream and returns the child
/*	exit status. It is an error to specify a stream that was not
/*	returned by vstream_popen() or that is no longer open.
/* DIAGNOSTICS
/*	Panics: interface violations. Fatal errors: out of memory.
/*
/*	vstream_popen() returns a null pointer in case of trouble.
/*	The nature of the problem is specified via the \fIerrno\fR
/*	global variable.
/*
/*	vstream_pclose() returns -1 in case of trouble.
/*	The nature of the problem is specified via the \fIerrno\fR
/*	global variable.
/* SEE ALSO
/*	vstream(3) light-weight buffered I/O
/* BUGS
/*	The interface, stolen from popen()/pclose(), ignores errors
/*	returned when the stream is closed, and does not distinguish
/*	between exit status codes and kill signals.
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
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

/* Utility library. */

#include <msg.h>
#include <binhash.h>
#include <exec_command.h>
#include <vstream.h>

/* Application-specific. */

static BINHASH *vstream_popen_table = 0;

/* vstream_popen - open stream to child process */

VSTREAM *vstream_popen(const char *command, int flags)
{
    VSTREAM *stream;
    int     sockfd[2];
    pid_t   pid;
    int     fd;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockfd) < 0)
	return (0);

    switch (pid = fork()) {
    case -1:					/* error */
	return (0);
    case 0:					/* child */
	if (close(sockfd[1]))
	    msg_warn("close: %m");
	for (fd = 0; fd < 2; fd++)
	    if (sockfd[0] != fd)
		if (dup2(sockfd[0], fd) < 0)
		    msg_fatal("dup2: %m");
	if (sockfd[0] >= 2 && close(sockfd[0]))
	    msg_warn("close: %m");
	exec_command(command);
	/* NOTREACHED */
    default:					/* parent */
	if (close(sockfd[0]))
	    msg_warn("close: %m");
	stream = vstream_fdopen(sockfd[1], flags);
	if (vstream_popen_table == 0)
	    vstream_popen_table = binhash_create(10);
	binhash_enter(vstream_popen_table, (char *) &stream,
		      sizeof(stream), (char *) pid);
	return (stream);
    }
}

/* vstream_pclose - close stream to child process */

int     vstream_pclose(VSTREAM *stream)
{
    char   *myname = "vstream_pclose";
    BINHASH_INFO *info;
    int     pid;
    WAIT_STATUS_T wait_status;

    /*
     * Sanity check.
     */
    if (vstream_popen_table == 0
	|| (info = binhash_locate(vstream_popen_table, (char *) &stream,
				  sizeof(stream))) == 0)
	msg_panic("%s: spurious stream %p", myname, (char *) stream);

    /*
     * Close the stream and reap the child exit status. Ignore errors while
     * flushing the stream. The child might already have terminated.
     */
    (void) vstream_fclose(stream);
    do {
	pid = waitpid((pid_t) info->value, &wait_status, 0);
    } while (pid == -1 && errno == EINTR);
    binhash_delete(vstream_popen_table, (char *) &stream, sizeof(stream),
		   (void (*) (char *)) 0);

    return (pid == -1 ? -1 :
	    WIFSIGNALED(wait_status) ? WTERMSIG(wait_status) :
	    WEXITSTATUS(wait_status));
}

#ifdef TEST

#include <fcntl.h>
#include <vstring.h>
#include <vstring_vstream.h>

 /*
  * Test program. Run a command and copy lines one by one.
  */
int     main(int argc, char **argv)
{
    VSTRING *buf = vstring_alloc(100);
    VSTREAM *stream;
    int     status;

    /*
     * Sanity check.
     */
    if (argc != 2)
	msg_fatal("usage: %s 'command'", argv[0]);

    /*
     * Open stream to child process.
     */
    if ((stream = vstream_popen(argv[1], O_RDWR)) == 0)
	msg_fatal("vstream_popen: %m");

    /*
     * Copy loop, one line at a time.
     */
    while (vstring_fgets(buf, stream) != 0) {
	if (vstream_fwrite(VSTREAM_OUT, vstring_str(buf), VSTRING_LEN(buf))
	    != VSTRING_LEN(buf))
	    msg_fatal("vstream_fwrite: %m");
	if (vstream_fflush(VSTREAM_OUT) != 0)
	    msg_fatal("vstream_fflush: %m");
	if (vstring_fgets(buf, VSTREAM_IN) == 0)
	    break;
	if (vstream_fwrite(stream, vstring_str(buf), VSTRING_LEN(buf))
	    != VSTRING_LEN(buf))
	    msg_fatal("vstream_fwrite: %m");
    }

    /*
     * Cleanup.
     */
    vstring_free(buf);
    if ((status = vstream_pclose(stream)) != 0)
	msg_warn("exit status: %d", status);

    exit(status);
}

#endif
