/*++
/* NAME
/*	postlift_run 8
/* SUMMARY
/*	postlift process management
/* SYNOPSIS
/*	pid_t	plift_run(
/*	const char *path,
/*	char	**argv,
/*	char	**envp,
/*	int	stdin_fd,
/*	int	stdout_fd)
/*
/*	int	plift_wait(
/*	pid_t	child_pid,
/*	int	client_fd,
/*	int	default_status)
/* DESCRIPTION
/*	plift_run() creates a child process with execve() with the
/*	provided pathname, command line, environment, and standard
/*	input/output file descriptors. It will make at most var_fork_tries
/*	attempts to fork a child process. The result is the child PID
/*	(success) or -1 (failure).
/*
/*	plift_wait() waits until the child process with PID child_pid
/*	terminates. It kills the child with SIGHUP when the client file
/*	descriptor client_fd becomes readable (usually as the result of
/*	client disconnect) to prevent child processes from lingering
/*	indefinitely. plift_wait() returns the child exit status,
/*	or default_status if the child did not terminate with exit().
/* CONFIGURATION PARAMETERS
/* .IP "\fBfork_attempts (5)\fR"
/*	The maximal number of attempts to fork() a child process.
/* .IP "\fBfork_delay (1s)\fR"
/*	The delay between attempts to fork() a child process.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* HISTORY
/*	This service was introduced with Postfix version 3.12.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

 /*
  * Utility library.
  */
#include <argv.h>
#include <events.h>
#include <msg.h>

 /*
  * Global library.
  */
#include <mail_params.h>

 /*
  * Application-specific.
  */
#include <postlift_sig.h>
#include <postlift_run.h>

/* plift_run - fork() and execve() command with I/O redirect */

pid_t   plift_run(const char *path, char **argv, char **envp,
		          int stdin_fd, int stdout_fd)
{
    pid_t   pid;
    int     count;

    for (count = 0; count < var_fork_tries; count++) {
	switch (pid = fork()) {
	case -1:
	    msg_warn("fork %s: %m", path);
	    break;
	case 0:
	    if (dup2(stdin_fd, STDIN_FILENO) < 0
		|| dup2(stdout_fd, STDOUT_FILENO) < 0)
		msg_fatal("dup2 stdin or stdout: %m");
	    execve(path, argv, envp);
	    msg_fatal("execve %s: %m", path);
	    /* NOTREACHED */
	default:
	    return (pid);
	}
	sleep(var_fork_delay);
    }
    return (pid);
}

/* handle_client_event - handle client disconnect */

static void handle_client_event(int unused_event, void *context)
{
    pid_t   pid = *(pid_t *) context;

    /*
     * A legitimate client will quietly wait for the child process to
     * terminate. If the client doesn't wait, kill the child with SIGHUP to
     * prevent a potential resource exhaustion attack.
     */
    if (kill(pid, SIGHUP) < 0)
	msg_warn("terminate child pid %lu: %m", (unsigned long) pid);
}

/* plift_wait - wait for client disconnect or for child exit */

int     plift_wait(pid_t pid, int client_fd, int default_status)
{
    pid_t   wpid;
    int     status;

    /*
     * Wait for child death events and client stream status changes.
     */
    plift_sigsetup();
    event_enable_read(client_fd, handle_client_event, (void *) &pid);
    do {
	event_loop(-1);
	if (plift_gotsigchld) {
	    if (msg_verbose)
		msg_info("got sigchld");
	    plift_gotsigchld = 0;
	}
	wpid = waitpid(pid, &status, 0);
    } while (wpid == -1 && errno == EINTR);
    return (wpid == -1 ? default_status :
	    WIFEXITED(status) ? WEXITSTATUS(status) :
	    default_status);
}
