/*++
/* NAME
/*	msg_output 3
/* SUMMARY
/*	diagnostics output management
/* SYNOPSIS
/*	#include <msg_output.h>
/*
/*	typedef void (*MSG_OUTPUT_FN)(int level, const char *text)
/*
/*	void	msg_output_push(output_fn, context)
/*	MSG_OUTPUT_FN output_fn;
/*	void	*context;
/*
/*	msg_output_pop(output_fn, context)
/*	MSG_OUTPUT_FN output_fn
/*	void	*context;
/*
/*	void	msg_printf(level, format, ...)
/*	int	level;
/*	const char *format;
/*
/*	void	msg_vprintf(level, format, ap)
/*	int	level;
/*	const char *format;
/*	va_list ap;
/* DESCRIPTION
/*	This module implements low-level output management for the
/*	msg(3) diagnostics interface.
/*
/*	msg_output_push() registers an output handler for the diagnostics
/*	interface, ignoring a duplicate request. Output handlers are
/*	called in the specified order. An output handler takes as
/*	arguments: a severity level (MSG_INFO, MSG_WARN, MSG_ERROR,
/*	MSG_FATAL, MSG_PANIC, i.e., integer values ranging from 0 to
/*	MSG_LAST inclusive); pre-formatted, sanitized, text in the form
/*	of a null-terminated string; and the caller-provided context.
/*
/*	msg_output_pop() unregisters the specified output handler and
/*	context, and later registrations made with msg_output_push(). It
/*	invokes a panic when the handler and context are not found.
/*
/*	msg_printf() and msg_vprintf() format their arguments, sanitize the
/*	result, and call the output handlers registered with msg_output().
/*
/*	msg_text() copies a pre-formatted text, sanitizes the result, and
/*	calls the output handlers registered with msg_output().
/* REENTRANCY
/* .ad
/* .fi
/*	The above output routines are protected against ordinary
/*	recursive calls and against re-entry by signal
/*	handlers, with the following limitations:
/* .IP \(bu
/*	The signal handlers must never return. In other words, the
/*	signal handlers must do one or more of the following: call
/*	_exit(), kill the process with a signal, and permanently
/*	block the process.
/* .IP \(bu
/*	The signal handlers must call the above output routines not
/*	until after msg_output() completes initialization.
/* .IP \(bu
/*	Each msg_output() call-back function, and each Postfix or
/*	system function called by that call-back function, either
/*	must protect itself against recursive calls and re-entry
/*	by a terminating signal handler, or it must be called
/*	exclusively by functions in the msg_output(3) module.
/* .PP
/*	When re-entrancy is detected, the requested output operation
/*	is skipped. This prevents memory corruption of VSTREAM_ERR
/*	data structures, and prevents deadlock on Linux releases
/*	that use mutexes within system library routines such as
/*	syslog(). This protection exists under the condition that
/*	these specific resources are accessed exclusively via
/*	msg_output() call-back functions.
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

/* System library. */

#include <sys_defs.h>
#include <stdarg.h>
#include <errno.h>

/* Utility library. */

#include <mymalloc.h>
#include <vstring.h>
#include <vstream.h>
#include <msg_vstream.h>
#include <stringops.h>
#include <msg.h>
#include <msg_output.h>

 /*
  * Global scope, to discourage the compiler from doing smart things.
  */
volatile int msg_vprintf_level;

 /*
  * Private state. Allow one nested call, so that one logging error can be
  * reported to stderr before bailing out.
  */
typedef struct MSG_OUT_INFO {
    MSG_OUTPUT_FN output_fn;
    void   *context;
} MSG_OUT_INFO;

#define MSG_OUT_NESTING_LIMIT	2
static MSG_OUT_INFO *msg_out_info = 0;
static int msg_output_fn_count = 0;
static VSTRING *msg_buffers[MSG_OUT_NESTING_LIMIT];

/* msg_output_push - specify output handler and context */

void    msg_output_push(MSG_OUTPUT_FN output_fn, void *context)
{
    int     i;
    MSG_OUT_INFO *mp;

    /*
     * Allocate all resources during initialization. This may result in a
     * recursive call due to memory allocation error.
     */
    if (msg_buffers[MSG_OUT_NESTING_LIMIT - 1] == 0) {
	for (i = 0; i < MSG_OUT_NESTING_LIMIT; i++)
	    msg_buffers[i] = vstring_alloc(100);
    }

    /*
     * Deduplicate requests. This is a purely defensive measure for the case
     * that msg_vstream_init() etc. are called more than once.
     */
    for (mp = msg_out_info; mp < msg_out_info + msg_output_fn_count; mp++)
	if (mp->output_fn == output_fn && mp->context == context)
	    return;

    /*
     * We're not doing this often, so avoid complexity and allocate memory
     * for an exact fit.
     */
    if (msg_output_fn_count == 0)
	msg_out_info = (MSG_OUT_INFO *) mymalloc(sizeof(*msg_out_info));
    else
	msg_out_info = (MSG_OUT_INFO *) myrealloc((void *) msg_out_info,
			 (msg_output_fn_count + 1) * sizeof(*msg_out_info));
    msg_out_info[msg_output_fn_count++] = (MSG_OUT_INFO) {
	output_fn, context
    };
}

/* msg_printf - format text and log it */

void    msg_printf(int level, const char *format,...)
{
    va_list ap;

    va_start(ap, format);
    msg_vprintf(level, format, ap);
    va_end(ap);
}

/* msg_vprintf - format text and log it */

void    msg_vprintf(int level, const char *format, va_list ap)
{
    int     saved_errno = errno;
    VSTRING *vp;
    int     i;

    if (msg_vprintf_level < MSG_OUT_NESTING_LIMIT) {
	msg_vprintf_level += 1;
	/* On-the-fly initialization for test programs and startup errors.  */
	if (msg_output_fn_count == 0)
	    msg_vstream_init("unknown", VSTREAM_ERR);
	vp = msg_buffers[msg_vprintf_level - 1];
	/* OK if terminating signal handler hijacks control before next stmt. */
	vstring_vsprintf(vp, format, ap);
	printable(vstring_str(vp), '?');
	for (i = 0; i < msg_output_fn_count; i++)
	    msg_out_info[i].output_fn(level, vstring_str(vp),
				      msg_out_info[i].context);
	msg_vprintf_level -= 1;
    }
    errno = saved_errno;
}

/* msg_output_pop - pop output handler and context */

void    msg_output_pop(MSG_OUTPUT_FN output_fn, void *context)
{
    MSG_OUT_INFO *mp;

    for (mp = msg_out_info; /* See below */ ; mp++) {
	if (mp >= msg_out_info + msg_output_fn_count)
	    msg_panic("msg_output_pop: handler %p and context %p not found",
		      (void *) output_fn, (void *) context);
	if (mp->output_fn == output_fn && mp->context == context) {
	    msg_output_fn_count = mp - msg_out_info;
	    return;
	}
    }
}
