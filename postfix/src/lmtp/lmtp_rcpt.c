/*++
/* NAME
/*	lmtp_rcpt 3
/* SUMMARY
/*	application-specific recipient list operations
/* SYNOPSIS
/*	#include <lmtp.h>
/*
/*	void	lmtp_rcpt_done(state, reply, rcpt)
/*	LMTP_STATE *state;
/*	LMTP_RESP *reply;
/*	RECIPIENT *rcpt;
/* DESCRIPTION
/*	lmtp_rcpt_done() logs that a recipient is completed and upon
/*	success it marks the recipient as done in the queue file.
/* DIAGNOSTICS
/*	Panic: interface violation.
/*
/*	When a recipient can't be logged as completed, the recipient is
/*	logged as deferred instead.
/* BUGS
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

/* System  library. */

#include <sys_defs.h>
#include <stdlib.h>			/* lmtp_rcpt_cleanup  */
#include <string.h>

/* Utility  library. */

#include <msg.h>

/* Global library. */

#include <deliver_request.h>
#include <deliver_completed.h>
#include <sent.h>
#include <dsn_mask.h>

/* Application-specific. */

#include <lmtp.h>

/* lmtp_rcpt_done - mark recipient as done or else */

void    lmtp_rcpt_done(LMTP_STATE *state, LMTP_RESP *resp, RECIPIENT *rcpt)
{
    DELIVER_REQUEST *request = state->request;
    LMTP_SESSION *session = state->session;
    DSN     dsn;
    int     status;

    /*
     * Report success and delete the recipient from the delivery request.
     * Don't send a DSN "SUCCESS" notification if the receiving site
     * announced DSN support (however unlikely that may be).
     */
    if (state->features & LMTP_FEATURE_DSN)
	rcpt->dsn_notify &= ~DSN_NOTIFY_SUCCESS;

    (void) LMTP_DSN_ASSIGN(&dsn, session->host, resp->dsn,
			   resp->str, resp->str);

    status = sent(DEL_REQ_TRACE_FLAGS(request->flags),
		  request->queue_id, &request->msg_stats, rcpt,
		  session->namaddr, &dsn);
    if (status == 0) {
	if (request->flags & DEL_REQ_FLAG_SUCCESS)
	    deliver_completed(state->src, rcpt->offset);
	rcpt->offset = 0;			/* in case deferred */
    }
    state->status |= status;
}
