/*++
/* NAME
/*	smtpd_state 3
/* SUMMARY
/*	Postfix SMTP server
/* SYNOPSIS
/*	#include "smtpd.h"
/*
/*	void	smtpd_state_init(state, stream, name, addr)
/*	SMTPD_STATE *state;
/*	VSTREAM *stream;
/*	const char *name;
/*	const char *addr;
/*
/*	void	smtpd_state_reset(state)
/*	SMTPD_STATE *state;
/* DESCRIPTION
/*	smtpd_state_init() initializes session context.
/*
/*	smtpd_state_reset() cleans up session context.
/*
/*	Arguments:
/* .IP state
/*	Session context.
/* .IP stream
/*	Stream connected to peer. The stream is not copied.
/* .IP name
/*	Printable representation of the peer host name. The
/*	name is copied.
/* .IP addr
/*	Printable representation of the peer host address. The
/*	address is copied.
/* DIAGNOSTICS
/*	All errors are fatal.
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

#include <events.h>
#include <mymalloc.h>
#include <vstream.h>
#include <name_mask.h>
#include <stringops.h>

/* Global library. */

#include <cleanup_user.h>
#include <mail_params.h>
#include <mail_error.h>

/* Application-specific. */

#include "smtpd.h"
#include "smtpd_chat.h"

/* smtpd_state_init - initialize after connection establishment */

void    smtpd_state_init(SMTPD_STATE *state, VSTREAM *stream,
			         const char *name, const char *addr)
{

    /*
     * Initialize the state information for this connection, and fill in the
     * connection-specific fields.
     */
    state->err = CLEANUP_STAT_OK;
    state->client = stream;
    state->buffer = vstring_alloc(100);
    state->name = mystrdup(name);
    state->addr = mystrdup(addr);
    state->namaddr = concatenate(name, "[", addr, "]", (char *) 0);
    state->error_count = 0;
    state->error_mask = 0;
    state->notify_mask = name_mask(mail_error_masks, var_notify_classes);
    state->helo_name = 0;
    state->queue_id = 0;
    state->cleanup = 0;
    state->dest = 0;
    state->rcpt_count = 0;
    state->access_denied = 0;
    state->history = 0;
    state->reason = 0;
    state->sender = 0;
    state->recipient = 0;
    state->protocol = "SMTP";
    state->where = SMTPD_AFTER_CONNECT;

    /*
     * Initialize the conversation history.
     */
    smtpd_chat_reset(state);
}

/* smtpd_state_reset - cleanup after disconnect */

void    smtpd_state_reset(SMTPD_STATE *state)
{

    /*
     * When cleaning up, touch only those fields that smtpd_state_init()
     * filled in. The other fields are taken care of by their own
     * "destructor" functions.
     */
    if (state->buffer)
	vstring_free(state->buffer);
    if (state->name)
	myfree(state->name);
    if (state->addr)
	myfree(state->addr);
    if (state->namaddr)
	myfree(state->namaddr);
}
