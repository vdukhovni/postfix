/*++
/* NAME
/*	log_adhoc 3
/* SUMMARY
/*	ad-hoc delivery event logging
/* SYNOPSIS
/*	#include <log_adhoc.h>
/*
/*	void	log_adhoc(id, entry, recipient, relay, dsn, status)
/*	const char *id;
/*	time_t	entry;
/*	RECIPIENT *recipient;
/*	const char *relay;
/*	DSN *dsn;
/*	const char *status;
/* DESCRIPTION
/*	This module logs delivery events in an ad-hoc manner.
/*
/*	log_adhoc() appends a record to the mail logfile
/*
/*	Arguments:
/* .IP queue
/*	The message queue name of the original message file.
/* .IP id
/*	The queue id of the original message file.
/* .IP entry
/*	Message arrival time.
/* .IP recipient
/*	Recipient information. See recipient_list(3).
/* .IP sender
/*	The sender envelope address.
/* .IP relay
/*	Host we could (not) talk to.
/* .IP status
/*	bounced, deferred, sent, and so on.
/* .IP dsn
/*	Delivery status information. See dsn(3).
/* BUGS
/*	Should be replaced by routines with an attribute-value based
/*	interface instead of an interface that uses a rigid argument list.
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

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

/* Utility library. */

#include <msg.h>
#include <vstring.h>

/* Global library. */

#include <log_adhoc.h>

/* log_adhoc - defer message delivery */

void    log_adhoc(const char *id, time_t entry, RECIPIENT *recipient,
		          const char *relay, DSN *dsn,
		          const char *status)
{
    int     delay = time((time_t *) 0) - entry;

    if (recipient->orig_addr && *recipient->orig_addr
	&& strcasecmp(recipient->address, recipient->orig_addr) != 0)
	msg_info("%s: to=<%s>, orig_to=<%s>, relay=%s, delay=%d, dsn=%s, status=%s (%s)",
		 id, recipient->address, recipient->orig_addr, relay, delay,
		 dsn->status, status, dsn->reason);
    else
	msg_info("%s: to=<%s>, relay=%s, delay=%d, dsn=%s, status=%s (%s)",
		 id, recipient->address, relay, delay, dsn->status,
		 status, dsn->reason);
}
