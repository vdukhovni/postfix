/*++
/* NAME
/*	plift_sig 3
/* SUMMARY
/*	Postfix plift - signal processing
/* SYNOPSIS
/*	#include <postlift_sig.h>
/*
/*	int	plift_gotsigchld;
/*
/*	void	plift_sigsetup(void)
/* DESCRIPTION
/*	This module implements the plift process signal handling interface.
/*
/*	plift_gotsigchld is set to SIGCHLD
/*	when the process receives a child death signal.
/*
/*	plift_sigsetup() enables processing of child death signals.
/* DIAGNOSTICS
/* BUGS
/*	Need a way to register cleanup actions.
/* SEE ALSO
/*	master_sig(3)
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*
/*	Wietse Venema
/*	Google, Inc.
/*	111 8th Avenue
/*	New York, NY 10011, USA
/*
/*	Wietse Venema
/*	porcupine.org
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>
#include <signal.h>
#include <unistd.h>

#ifdef USE_SIG_RETURN
#include <sys/syscall.h>
#undef USE_SIG_PIPE
#else
#define USE_SIG_PIPE
#include <errno.h>
#include <fcntl.h>
#endif

 /*
  * Utility library.
  */
#include <msg.h>
#include <posix_signals.h>
#include <killme_after.h>
#ifdef USE_SIG_PIPE
#include <iostuff.h>
#include <events.h>
#endif

 /*
  * Application-specific.
  */
#include <postlift_sig.h>

int     plift_sig_pipe[2];

#ifdef USE_SIG_PIPE
#define SIG_PIPE_WRITE_FD plift_sig_pipe[1]
#define SIG_PIPE_READ_FD plift_sig_pipe[0]
#endif

int     plift_gotsigchld;

#ifdef USE_SIG_RETURN

/* plift_sigchld - register arrival of child death signal */

static void plift_sigchld(int sig, int code, struct sigcontext * scp)
{

    /*
     * WARNING WARNING WARNING.
     * 
     * This code runs at unpredictable moments, as a signal handler. Don't put
     * any code here other than for setting a global flag, or code that is
     * intended to be run within a signal handler.
     */
    plift_gotsigchld = sig;
    if (scp != NULL && scp->sc_syscall == SYS_select) {
	scp->sc_syscall_action = SIG_RETURN;
#ifndef SA_RESTART
    } else if (scp != NULL) {
	scp->sc_syscall_action = SIG_RESTART;
#endif
    }
}

#else

/* plift_sigchld - force wakeup from select() */

static void plift_sigchld(int unused_sig)
{
    int     saved_errno = errno;

    /*
     * WARNING WARNING WARNING.
     * 
     * This code runs at unpredictable moments, as a signal handler. Don't put
     * any code here other than for setting a global flag, or code that is
     * intended to be run within a signal handler. Restore errno in case we
     * are interrupting the epilog of a failed system call.
     */
    plift_gotsigchld = 1;
    if (write(SIG_PIPE_WRITE_FD, "", 1) != 1)
	msg_warn("write to SIG_PIPE_WRITE_FD failed: %m");
    errno = saved_errno;
}

/* plift_sig_event - called upon return from select() */

static void plift_sig_event(int unused_event, void *unused_context)
{
    char    c[1];

    while (read(SIG_PIPE_READ_FD, c, 1) > 0)
	 /* void */ ;
}

#endif

/* plift_sigsetup - set up signal handlers */

void    plift_sigsetup(void)
{
    struct sigaction action;

    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

#ifdef USE_SIG_PIPE
    if (pipe(plift_sig_pipe))
	msg_fatal("pipe: %m");
    non_blocking(SIG_PIPE_WRITE_FD, NON_BLOCKING);
    non_blocking(SIG_PIPE_READ_FD, NON_BLOCKING);
    close_on_exec(SIG_PIPE_WRITE_FD, CLOSE_ON_EXEC);
    close_on_exec(SIG_PIPE_READ_FD, CLOSE_ON_EXEC);
    event_enable_read(SIG_PIPE_READ_FD, plift_sig_event, (void *) 0);
#endif

    /*
     * Intercept SIGCHLD (child exit).
     */
#ifdef SA_RESTART
    action.sa_flags |= SA_RESTART;
#endif
    action.sa_flags |= SA_NOCLDSTOP;
    action.sa_handler = plift_sigchld;
    if (sigaction(SIGCHLD, &action, (struct sigaction *) 0) < 0)
	msg_fatal("%s: sigaction(%d): %m", __func__, SIGCHLD);
}
