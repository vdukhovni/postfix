/*++
/* NAME
/*	msg_output 3
/* SUMMARY
/*	diagnostics output management
/* SYNOPSIS
/*	#include <msg_output.h>
/*
/*	typedef void (*MSG_OUTPUT3_FN)(int level, char *text, void *context)
/*
/*	void	msg_output_push(output_fn, context)
/*	MSG_OUTPUT3_FN output_fn;
/*	void	*context;
/*
/*	msg_output_pop(output_fn, context)
/*	MSG_OUTPUT3_FN output_fn
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
/* LEGACY API
/*	typedef void (*MSG_OUTPUT_FN)(int level, char *text)
/*
/*	void	msg_output(output_fn)
/*	MSG_OUTPUT_FN output_fn;
/* DESCRIPTION
/*	This module implements low-level output management for the
/*	msg(3) diagnostics interface.
/*
/*	msg_output_push() registers an output handler and call-back
/*	context, if that handler and context are not already
/*	registered. Output handlers are called in the specified
/*	order. The context pointer is passed as the third argument
/*	to an MSG_OUTPUT3_FN output handler. An output handler
/*	takes as arguments a severity level (MSG_INFO, MSG_WARN,
/*	MSG_ERROR, MSG_FATAL, MSG_PANIC, monotonically increasing
/*	integer values ranging from 0 to MSG_LAST) and pre-formatted,
/*	sanitized, text in the form of a null-terminated string.
/*
/*	msg_output_pop() unregisters the specified output handler
/*	and context, and all later registered (handler, context)
/*	pairs. It invokes a panic when the handler and context are
/*	not found.
/*
/*	msg_printf() and msg_vprintf() format their arguments,
/*	sanitize the result, and call the output handlers registered
/*	with msg_output().
/*
/*	msg_text() copies a pre-formatted text, sanitizes the result,
/*	and calls the output handlers registered with msg_output().
/* REENTRANCY
/* .ad
/* .fi
/*	The output routines are protected against ordinary recursive
/*	calls and against re-entry by signal handlers, with the
/*	following limitations:
/* .IP \(bu
/*	The signal handlers must never return. In other words, the
/*	signal handlers must do one or more of the following: call
/*	_exit(), kill the process with a signal, or permanently
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
typedef struct MSG_OUTPUT_INFO {
    int     argc;			/* 2 or 3 arguments */
    union {
	MSG_OUTPUT_FN output_fn;	/* two-argument form */
	MSG_OUTPUT3_FN output3_fn;	/* three-argument form */
    }       u;
    void   *context;			/* used for three-argument form */
} MSG_OUTPUT_INFO;

#define MSG_OUT_NESTING_LIMIT	2
static MSG_OUTPUT_INFO *msg_output_info = 0;
static int msg_output_info_count = 0;
static VSTRING *msg_buffers[MSG_OUT_NESTING_LIMIT];

/* do_msg_output - add output handler */

static void do_msg_output(MSG_OUTPUT_INFO *info)
{
    VSTRING **bp;
    MSG_OUTPUT_INFO *mp;

    /*
     * Allocate all resources during initialization. This may result in a
     * recursive call due to memory allocation error.
     */
    if (msg_buffers[MSG_OUT_NESTING_LIMIT - 1] == 0) {
	for (bp = msg_buffers; bp < msg_buffers + MSG_OUT_NESTING_LIMIT; bp++)
	    *bp = vstring_alloc(100);
    }

    /*
     * Deduplicate requests.
     */
    for (mp = msg_output_info; mp < msg_output_info + msg_output_info_count; mp++) {
	if (mp->argc == info->argc
	    && mp->context == info->context
	    && ((info->argc == 2 && mp->u.output_fn == info->u.output_fn)
	    || (info->argc == 3 && mp->u.output3_fn == info->u.output3_fn)))
	    return;
    }

    /*
     * We're not doing this often, so avoid complexity and allocate memory
     * for an exact fit.
     */
    if (msg_output_info_count == 0)
	msg_output_info = (MSG_OUTPUT_INFO *) mymalloc(sizeof(*msg_output_info));
    else
	msg_output_info = (MSG_OUTPUT_INFO *) myrealloc((void *) msg_output_info,
		    (msg_output_info_count + 1) * sizeof(*msg_output_info));
    msg_output_info[msg_output_info_count++] = *info;
}

/* msg_output - specify output handler */

void    msg_output(MSG_OUTPUT_FN output_fn)
{
    MSG_OUTPUT_INFO info;

    info.argc = 2;
    info.u.output_fn = output_fn;
    info.context = 0;
    do_msg_output(&info);
}

/* msg_output_push - specify three-argument output handler */

void    msg_output_push(MSG_OUTPUT3_FN output_fn, void *context)
{
    MSG_OUTPUT_INFO info;

    info.argc = 3;
    info.u.output3_fn = output_fn;
    info.context = context;
    do_msg_output(&info);
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
    MSG_OUTPUT_INFO *mp;
    VSTRING *vp;

    if (msg_vprintf_level < MSG_OUT_NESTING_LIMIT) {
	msg_vprintf_level += 1;
	/* On-the-fly initialization for test programs and startup errors. */
	if (msg_output_info_count == 0)
	    msg_vstream_init("unknown", VSTREAM_ERR);
	vp = msg_buffers[msg_vprintf_level - 1];
	/* OK if terminating signal handler hijacks control before next stmt. */
	vstring_vsprintf(vp, format, ap);
	printable(vstring_str(vp), '?');
	for (mp = msg_output_info; mp < msg_output_info + msg_output_info_count; mp++) {
	    switch (mp->argc) {
	    case 3:
		mp->u.output3_fn(level, vstring_str(vp), mp->context);
		break;
	    case 2:
		mp->u.output_fn(level, vstring_str(vp));
		break;
	    default:
		msg_panic("msg_vprintf: bad argument count: %d", mp->argc);
	    }
	}
	msg_vprintf_level -= 1;
    }
    errno = saved_errno;
}

/* msg_output_pop - pop output handler and context */

void    msg_output_pop(MSG_OUTPUT3_FN output_fn, void *context)
{
    MSG_OUTPUT_INFO *mp;

    for (;;) {
	if (msg_output_info_count <= 0)
	    msg_panic("msg_output_pop: handler and context not found");
	mp = msg_output_info + --msg_output_info_count;
	if (context == mp->context && mp->u.output3_fn == output_fn)
	    return;
    }
}
