/*++
/* NAME
/*	cleanup_api 3
/* SUMMARY
/*	callable interface
/* SYNOPSIS
/*	#include "cleanup.h"
/*
/*	char	*cleanup_path;
/*
/*	void	cleanup_all()
/*
/*	CLEANUP_STATE *cleanup_open()
/*
/*	void	cleanup_control(state, flags)
/*	CLEANUP_STATE *state;
/*	int	flags;
/*
/*	void	CLEANUP_RECORD(state, type, buf, len)
/*	CLEANUP_STATE *state;
/*	int	type;
/*	char	*buf;
/*	int	len;
/*
/*	int	cleanup_close(state)
/*	CLEANUP_STATE *state;
/* DESCRIPTION
/*	This module implements a callable interface to the cleanup service.
/*	For a description of the cleanup service, see cleanup(8).
/*
/*	cleanup_path is a null pointer or it is the name of the queue
/*	file that currently is being written. This information is used
/*	by cleanup_all() to clean up in case of fatal errors.
/*
/*	cleanup_open() creates a new queue file and performs other
/*	initialization. The result is a handle that should be given
/*	to the cleanup_control(), cleanup_record() and cleanup_close()
/*	routines. The name of the queue file is in the queue_id result
/*	structure member.
/*
/*	cleanup_control() processes flags specified by the caller.
/*	These flags control what happens in case of data errors.
/*
/*	CLEANUP_RECORD() processes one queue file record and maintains
/*	a little state machine. CLEANUP_RECORD() is a macro that calls
/*	the appropriate routine depending on what section of a queue file
/*	is being processed. In order to find out if a file is corrupted,
/*	the caller can test the CLEANUP_OUT_OK(state) macro. The result is
/*	false when further message processing is futile.
/*
/*	cleanup_close() finishes a queue file. In case of any errors,
/*	the file is removed. The result status is non-zero in case of
/*	problems. use cleanup_strerror() to translate the result into
/*	human_readable text.
/*
/*	cleanup_all() should be called in case of fatal error, in order
/*	to remove an incomplete queue file. Typically one registers a 
/*	msg_cleanup() handler and a signal() handler that call
/*	cleanup_all() before terminating the process.
/* STANDARDS
/*	RFC 822 (ARPA Internet Text Messages)
/* DIAGNOSTICS
/*	Problems and transactions are logged to \fBsyslogd\fR(8).
/* SEE ALSO
/*	cleanup(8) cleanup service description.
/* FILES
/*	/etc/postfix/canonical*, canonical mapping table
/*	/etc/postfix/virtual*, virtual mapping table
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
#include <errno.h>

/* Utility library. */

#include <msg.h>
#include <vstring.h>
#include <mymalloc.h>

/* Global library. */

#include <cleanup_user.h>
#include <mail_queue.h>
#include <mail_proto.h>
#include <opened.h>
#include <bounce.h>
#include <mail_params.h>
#include <mail_stream.h>
#include <mail_addr.h>

/* Application-specific. */

#include "cleanup.h"

 /*
  * Global state: any queue files that we have open, so that the error
  * handler can clean up in case of trouble.
  */
char   *cleanup_path;			/* queue file name */

/* cleanup_open - open queue file and initialize */

CLEANUP_STATE * cleanup_open(void)
{
    CLEANUP_STATE *state;
    static char *log_queues[] = {
	MAIL_QUEUE_DEFER,
	MAIL_QUEUE_BOUNCE,
	0,
    };
    char  **cpp;

    /*
     * Initialize.
     */
    state = cleanup_state_alloc();

    /*
     * Open the queue file. Send the queue ID to the client so they can use
     * it for logging purposes. For example, the SMTP server sends the queue
     * id to the SMTP client after completion of the DATA command; and when
     * the local delivery agent forwards a message, it logs the new queue id
     * together with the old one. All this is done to make it easier for mail
     * admins to follow a message while it hops from machine to machine.
     * 
     * Save the queue file name, so that the runtime error handler can clean up
     * in case of problems.
     */
    state->handle = mail_stream_file(MAIL_QUEUE_INCOMING,
				     MAIL_CLASS_PUBLIC, MAIL_SERVICE_QUEUE);
    state->dst = state->handle->stream;
    cleanup_path = mystrdup(VSTREAM_PATH(state->dst));
    state->queue_id = mystrdup(state->handle->id);
    if (msg_verbose)
	msg_info("cleanup_open: open %s", cleanup_path);

    /*
     * If there is a time to get rid of spurious bounce/defer log files, this
     * is it. The down side is that this costs performance for every message,
     * while the probability of spurious bounce/defer log files is quite low.
     * Perhaps we should put the queue file ID inside the defer and bounce
     * files, so that the bounce and defer daemons can figure out if a file
     * is a left-over from a previous message instance. For now, we play safe
     * and check each time a new queue file is created.
     */
    for (cpp = log_queues; *cpp; cpp++) {
	if (mail_queue_remove(*cpp, state->queue_id) == 0)
	    msg_warn("%s: removed spurious %s log", *cpp, state->queue_id);
	else if (errno != ENOENT)
	    msg_fatal("%s: remove %s log: %m", *cpp, state->queue_id);
    }
    return (state);
}

/* cleanup_control - process client options */

void    cleanup_control(CLEANUP_STATE *state, int flags)
{

    /*
     * If the client requests us to do the bouncing in case of problems,
     * throw away the input only in case of real show-stopper errors, such as
     * unrecognizable data (which should never happen) or insufficient space
     * for the queue file (which will happen occasionally). Otherwise,
     * discard input after any lethal error. See the CLEANUP_OUT_OK()
     * definition.
     */
    if ((state->flags = flags) & CLEANUP_FLAG_BOUNCE) {
	state->err_mask =
	(CLEANUP_STAT_BAD | CLEANUP_STAT_WRITE | CLEANUP_STAT_SIZE);
    } else {
	state->err_mask = CLEANUP_STAT_LETHAL;
    }
}

/* cleanup_close - finish queue file */

int     cleanup_close(CLEANUP_STATE *state)
{
    char   *junk;
    int     status;

    /*
     * Now that we have captured the entire message, see if there are any
     * other errors. For example, if the message needs to be bounced for lack
     * of recipients. We want to turn on the execute bits on a file only when
     * we want the queue manager to process it.
     */
    if (state->recip == 0)
	state->errs |= CLEANUP_STAT_RCPT;

    /*
     * If there are no errors, be very picky about queue file write errors
     * because we are about to tell the sender that it can throw away its
     * copy of the message.
     */
    if (state->errs == 0)
	state->errs |= mail_stream_finish(state->handle);
    else
	mail_stream_cleanup(state->handle);
    state->handle = 0;
    state->dst = 0;

    /*
     * If there was an error, remove the queue file, after optionally
     * bouncing it. An incomplete message should never be bounced: it was
     * canceled by the client, and may not even have an address to bounce to.
     * That last test is redundant but we keep it just for robustness.
     * 
     * If we are responsible for bouncing a message, we must must report success
     * to the client unless the bounce message file could not be written
     * (which is just as bad as not being able to write the message queue
     * file in the first place).
     * 
     * Do not log the arrival of a message that will be bounced by the client.
     * 
     * XXX CLEANUP_STAT_LETHAL masks errors that are not directly fatal (e.g.,
     * header buffer overflow is normally allowed to happen), but that can
     * indirectly become a problem (e.g., no recipients were extracted from
     * message headers because we could not process all the message headers).
     * However, cleanup_strerror() prioritizes errors so that it can report
     * the cause (e.g., header buffer overflow), which is more useful.
     * Amazing.
     */
#define CAN_BOUNCE() \
	((state->errs & (CLEANUP_STAT_BAD | CLEANUP_STAT_WRITE)) == 0 \
	    && state->sender != 0 \
	    && (state->flags & CLEANUP_FLAG_BOUNCE) != 0)

    if (state->errs & CLEANUP_STAT_LETHAL) {
	if (CAN_BOUNCE()) {
	    if (bounce_recip(BOUNCE_FLAG_CLEAN,
			     MAIL_QUEUE_INCOMING, state->queue_id,
			     state->sender, state->recip ?
			     state->recip : "", "cleanup", state->time,
			     "Message rejected: %s",
			     cleanup_strerror(state->errs)) == 0) {
		state->errs = 0;
	    } else {
		msg_warn("%s: bounce message failure", state->queue_id);
		state->errs = CLEANUP_STAT_WRITE;
	    }
	}
	if (REMOVE(cleanup_path))
	    msg_warn("remove %s: %m", cleanup_path);
    }

    /*
     * Make sure that our queue file will not be deleted by the error handler
     * AFTER we have taken responsibility for delivery. Better to deliver
     * twice than to lose mail.
     */
    junk = cleanup_path;
    cleanup_path = 0;				/* don't delete upon error */
    myfree(junk);

    /*
     * Cleanup internal state. This is simply complementary to the
     * initializations at the beginning of cleanup_open().
     */
    if (msg_verbose)
	msg_info("cleanup_close: status %d", state->errs);
    status = state->errs & CLEANUP_STAT_LETHAL;
    cleanup_state_free(state);
    return (status);
}

/* cleanup_all - callback for the runtime error handler */

void cleanup_all(void)
{
    if (cleanup_path && REMOVE(cleanup_path))
	msg_warn("cleanup_all: remove %s: %m", cleanup_path);
}
