/*++
/* NAME
/*	verp_sender 3
/* SUMMARY
/*	quote local part of mailbox
/* SYNOPSIS
/*	#include <verp_sender.h>
/*
/*	VSTRING	*verp_sender(dst, delims, sender, recipient)
/*	VSTRING	*dst;
/*	const char *delims;
/*	const char *sender;
/*	const char *recipient;
/* DESCRIPTION
/*	verp_sender() encodes the recipient address in the sender
/*	address, using the specified delimiters. For example,
/*	with delims +=, sender \fIprefix@origin\fR, and
/*	recipient \fIuser@domain\fR the result is
/*	\fIprefix+user=domain@origin\fR.
/*
/*	Arguments:
/* .IP dst
/*	The result. The buffer is null terminated.
/* .IP delims
/*	VERP formatting characters.
/* .IP sender
/*	Sender envelope address.
/* .IP recipient
/*	Recipient envelope address.
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
#include <string.h>

/* Utility library. */

#include <vstring.h>

/* Global library. */

#include <verp_sender.h>

/* verp_sender - encode recipient into envelope sender address */

VSTRING *verp_sender(VSTRING *buf, const char *delimiters,
		             const char *sender, const char *recipient)
{
    int     send_local_len;
    int     rcpt_local_len;
    const char *cp;

    /*
     * Change prefix@origin into prefix+user=domain@origin.
     */
    send_local_len = ((cp = strrchr(sender, '@')) ?
		      cp - sender : strlen(sender));
    rcpt_local_len = ((cp = strrchr(recipient, '@')) ?
		      cp - recipient : strlen(recipient));
    vstring_strncpy(buf, sender, send_local_len);
    VSTRING_ADDCH(buf, delimiters[0] & 0xff);
    vstring_strncat(buf, recipient, rcpt_local_len);
    if (recipient[rcpt_local_len] && recipient[rcpt_local_len + 1]) {
	VSTRING_ADDCH(buf, delimiters[1] & 0xff);
	vstring_strcat(buf, recipient + rcpt_local_len + 1);
    }
    if (sender[send_local_len] && sender[send_local_len + 1]) {
	VSTRING_ADDCH(buf, '@');
	vstring_strcat(buf, sender + send_local_len + 1);
    }
    VSTRING_TERMINATE(buf);
    return (buf);
}
