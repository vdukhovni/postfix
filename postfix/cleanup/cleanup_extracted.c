/*++
/* NAME
/*	cleanup_extracted 3
/* SUMMARY
/*	process extracted segment
/* SYNOPSIS
/*	#include "cleanup.h"
/*
/*	void	cleanup_extracted(void)
/* DESCRIPTION
/*	This module processes message segments for information
/*	extracted from message content. It requires that the input
/*	contains no extracted information, and writes extracted
/*	information records to the output.
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

/* Utility library. */

#include <msg.h>
#include <vstring.h>
#include <vstream.h>
#include <argv.h>
#include <mymalloc.h>

/* Global library. */

#include <cleanup_user.h>
#include <record.h>
#include <rec_type.h>

/* Application-specific. */

#include "cleanup.h"

/* cleanup_extracted - generate segment with header-extracted information */

void    cleanup_extracted(void)
{
    ARGV   *rcpt;
    char  **cpp;
    int     type;

    /*
     * Do not complain in case of premature EOF - most likely the client
     * aborted the operation.
     * 
     * XXX Rely on the front-end programs to enforce record size limits.
     */
    if ((type = rec_get(cleanup_src, cleanup_inbuf, 0)) < 0) {
	cleanup_errs |= CLEANUP_STAT_BAD;
	return;
    }

    /*
     * Require that the client sends an empty record group. It is OUR job to
     * extract information from message headers. Clients can specify options
     * via special message header lines.
     * 
     * XXX Keep reading in case of trouble, so that the sender is ready for our
     * status report.
     */
    if (type != REC_TYPE_END) {
	msg_warn("unexpected record type %d in extracted segment", type);
	cleanup_errs |= CLEANUP_STAT_BAD;
	if (type >= 0)
	    cleanup_skip();
	return;
    }

    /*
     * Start the extracted segment.
     */
    cleanup_out_string(REC_TYPE_XTRA, "");

    /*
     * Always emit Return-Receipt-To and Errors-To records, and always emit
     * them ahead of extracted recipients, so that the queue manager does not
     * waste lots of time searching through large numbers of recipient
     * addresses.
     */
    cleanup_out_string(REC_TYPE_RRTO, cleanup_return_receipt ?
		       cleanup_return_receipt : "");

    cleanup_out_string(REC_TYPE_ERTO, cleanup_errors_to ?
		       cleanup_errors_to : cleanup_sender);

    /*
     * Optionally account for missing recipient envelope records.
     */
    if (cleanup_recip == 0) {
	rcpt = (cleanup_resent[0] ? cleanup_resent_recip : cleanup_recipients);
	argv_terminate(rcpt);
	for (cpp = rcpt->argv; CLEANUP_OUT_OK() && *cpp; cpp++)
	    cleanup_out_recipient(*cpp);
	if (rcpt->argv[0])
	    cleanup_recip = mystrdup(rcpt->argv[0]);
    }

    /*
     * Terminate the extracted segment.
     */
    cleanup_out_string(REC_TYPE_END, "");
}
