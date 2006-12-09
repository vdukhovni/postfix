/*++
/* NAME
/*	msg 3
/* SUMMARY
/*	diagnostic interface
/* SYNOPSIS
/*	#include <msg.h>
/*
/*	int	msg_verbose;
/*
/*	void	msg_info(format, ...)
/*	const char *format;
/*
/*	void	msg_warn(format, ...)
/*	const char *format;
/*
/*	void	msg_error(format, ...)
/*	const char *format;
/*
/*	NORETURN msg_fatal(format, ...)
/*	const char *format;
/*
/*	NORETURN msg_fatal_status(status, format, ...)
/*	int	status;
/*	const char *format;
/*
/*	NORETURN msg_panic(format, ...)
/*	const char *format;
/*
/*	MSG_CLEANUP_FN msg_cleanup(cleanup)
/*	void (*cleanup)(void);
/* AUXILIARY FUNCTIONS
/*	int	msg_error_limit(count)
/*	int	count;
/*
/*	void	msg_error_clear()
/* DESCRIPTION
/*	This module reports diagnostics. By default, diagnostics are sent
/*	to the standard error stream, but the disposition can be changed
/*	by the user. See the hints below in the SEE ALSO section.
/*
/*	msg_info(), msg_warn(), msg_error(), msg_fatal*() and msg_panic()
/*	produce a one-line record with the program name, a severity code
/*	(except for msg_info()), and an informative message. The program
/*	name must have been set by calling one of the msg_XXX_init()
/*	functions (see the SEE ALSO section).
/*
/*	The aforementioned logging routines are protected against
/*	ordinary recursive calls and against re-entry by a signal
/*	handler.
/*
/*	Protection against re-entry by signal handlers requires
/*	that:
/* .IP \(bu
/*	The signal handler must never return. In other words, the
/*	signal handler must either call _exit(), kill itself with
/*	a signal, or do both.
/* .IP \(bu
/*	The signal handler must not execute before the msg_XXX_init()
/*	functions complete initialization.
/* .PP
/*	When re-entrancy is detected, the requested logging and
/*	optional cleanup operations are skipped. Skipping the logging
/*	operation prevents deadlock on Linux releases that use
/*	mutexes within system library routines such as syslog().
/*
/*	msg_error() reports a recoverable error and increments the error
/*	counter. When the error count exceeds a pre-set limit (default: 13)
/*	the program terminates by calling msg_fatal().
/*
/*	msg_fatal() reports an unrecoverable error and terminates the program
/*	with a non-zero exit status.
/*
/*	msg_fatal_status() reports an unrecoverable error and terminates the
/*	program with the specified exit status.
/*
/*	msg_panic() reports an internal inconsistency, terminates the
/*	program immediately (i.e. without calling the optional user-specified
/*	cleanup routine), and forces a core dump when possible.
/*
/*	msg_cleanup() specifies a function that msg_fatal[_status]() should
/*	invoke before terminating the program, and returns the
/*	current function pointer. Specify a null argument to disable
/*	this feature.
/*
/*	Note: each msg_cleanup() call-back function, and each Postfix
/*	or system function called by that call-back function, either
/*	protects itself against recursive calls and re-entry by a
/*	terminating signal handler, or is called exclusively by
/*	functions in the msg(3) module.
/*
/*	msg_error_limit() sets the error message count limit, and returns.
/*	the old limit.
/*
/*	msg_error_clear() sets the error message count to zero.
/*
/*	msg_verbose is a global flag that can be set to make software
/*	more verbose about what it is doing. By default the flag is zero.
/*	By convention, a larger value means more noise.
/* SEE ALSO
/*	msg_output(3) specify diagnostics disposition
/*	msg_stdio(3) direct diagnostics to standard I/O stream
/*	msg_vstream(3) direct diagnostics to VSTREAM.
/*	msg_syslog(3) direct diagnostics to syslog daemon
/* BUGS
/*	Some output functions may suffer from intentional or accidental
/*	record length restrictions that are imposed by library routines
/*	and/or by the runtime environment.
/*
/*	Code that spawns a child process should almost always reset
/*	the cleanup handler. The exception is when the parent exits
/*	immediately and the child continues.
/*
/*	msg_cleanup() may be unsafe in code that changes process
/*	privileges, because the call-back routine may run with the
/*	wrong privileges.
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

/* System libraries. */

#include <sys_defs.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

/* Application-specific. */

#include "msg.h"
#include "msg_output.h"

 /*
  * Default is verbose logging off.
  */
int     msg_verbose = 0;

 /*
  * Private state.
  */
static MSG_CLEANUP_FN msg_cleanup_fn = 0;
static int msg_error_count = 0;
static int msg_error_bound = 13;

 /*
  * Global scope, to discourage the compiler from doing smart things.
  */
volatile int msg_exiting = 0;

/* msg_info - report informative message */

void    msg_info(const char *fmt,...)
{
    va_list ap;

    BEGIN_PROTECT_AGAINST_RECURSION_OR_TERMINATING_SIGNAL_HANDLER(msg_exiting);
    va_start(ap, fmt);
    msg_vprintf(MSG_INFO, fmt, ap);
    va_end(ap);
    END_PROTECT_AGAINST_RECURSION_OR_TERMINATING_SIGNAL_HANDLER(msg_exiting);
}

/* msg_warn - report warning message */

void    msg_warn(const char *fmt,...)
{
    va_list ap;

    BEGIN_PROTECT_AGAINST_RECURSION_OR_TERMINATING_SIGNAL_HANDLER(msg_exiting);
    va_start(ap, fmt);
    msg_vprintf(MSG_WARN, fmt, ap);
    va_end(ap);
    END_PROTECT_AGAINST_RECURSION_OR_TERMINATING_SIGNAL_HANDLER(msg_exiting);
}

/* msg_error - report recoverable error */

void    msg_error(const char *fmt,...)
{
    va_list ap;

    BEGIN_PROTECT_AGAINST_RECURSION_OR_TERMINATING_SIGNAL_HANDLER(msg_exiting);
    va_start(ap, fmt);
    msg_vprintf(MSG_ERROR, fmt, ap);
    va_end(ap);
    END_PROTECT_AGAINST_RECURSION_OR_TERMINATING_SIGNAL_HANDLER(msg_exiting);
    if (++msg_error_count >= msg_error_bound)
	msg_fatal("too many errors - program terminated");
}

/* msg_fatal - report error and terminate gracefully */

NORETURN msg_fatal(const char *fmt,...)
{
    va_list ap;

    BEGIN_PROTECT_AGAINST_RECURSION_OR_TERMINATING_SIGNAL_HANDLER(msg_exiting);
    va_start(ap, fmt);
    msg_vprintf(MSG_FATAL, fmt, ap);
    va_end(ap);
    if (msg_cleanup_fn)
	msg_cleanup_fn();
    END_PROTECT_AGAINST_RECURSION_OR_TERMINATING_SIGNAL_HANDLER(msg_exiting);
    sleep(1);
    /* In case we're running as a signal handler. */
    _exit(1);
}

/* msg_fatal_status - report error and terminate gracefully */

NORETURN msg_fatal_status(int status, const char *fmt,...)
{
    va_list ap;

    BEGIN_PROTECT_AGAINST_RECURSION_OR_TERMINATING_SIGNAL_HANDLER(msg_exiting);
    va_start(ap, fmt);
    msg_vprintf(MSG_FATAL, fmt, ap);
    va_end(ap);
    if (msg_cleanup_fn)
	msg_cleanup_fn();
    END_PROTECT_AGAINST_RECURSION_OR_TERMINATING_SIGNAL_HANDLER(msg_exiting);
    sleep(1);
    /* In case we're running as a signal handler. */
    _exit(status);
}

/* msg_panic - report error and dump core */

NORETURN msg_panic(const char *fmt,...)
{
    va_list ap;

    BEGIN_PROTECT_AGAINST_RECURSION_OR_TERMINATING_SIGNAL_HANDLER(msg_exiting);
    va_start(ap, fmt);
    msg_vprintf(MSG_PANIC, fmt, ap);
    va_end(ap);
    END_PROTECT_AGAINST_RECURSION_OR_TERMINATING_SIGNAL_HANDLER(msg_exiting);
    sleep(1);
    abort();					/* Die! */
    /* In case we're running as a signal handler. */
    _exit(1);					/* DIE!! */
}

/* msg_cleanup - specify cleanup routine */

MSG_CLEANUP_FN msg_cleanup(MSG_CLEANUP_FN cleanup_fn)
{
    MSG_CLEANUP_FN old_fn = msg_cleanup_fn;

    msg_cleanup_fn = cleanup_fn;
    return (old_fn);
}

/* msg_error_limit - set error message counter limit */

int     msg_error_limit(int limit)
{
    int     old = msg_error_bound;

    msg_error_bound = limit;
    return (old);
}

/* msg_error_clear - reset error message counter */

void    msg_error_clear(void)
{
    msg_error_count = 0;
}
