/*++
/* NAME
/*	smtpd_xclient 3
/* SUMMARY
/*	maintain XCLIENT information
/* SYNOPSIS
/*	#include "smtpd.h"
/*
/*	void	smtpd_xclient_init(state)
/*	SMTPD_STATE *state;
/*
/*	void	smtpd_xclient_reset(state)
/*	SMTPD_STATE *state;
/* DESCRIPTION
/*	smtpd_xclient_init() zeroes the attributes for storage of XCLIENT
/*	FORWARD command parameters.
/*
/*	smtpd_xclient_preset() takes the result from smtpd_xclient_init()
/*	and sets all fields to the same "unknown" value that regular
/*	client attributes would have.
/*
/*	smtpd_xclient_reset() restores the state from smtpd_xclient_init().
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

#include <mymalloc.h>
#include <msg.h>

/* Global library. */

#include <mail_proto.h>

/* Application-specific. */

#include <smtpd.h>

/* smtpd_xclient_init - initialize XCLIENT attributes */

void    smtpd_xclient_init(SMTPD_STATE *state)
{
    state->xclient.used = 0;
    state->xclient.name = 0;
    state->xclient.addr = 0;
    state->xclient.namaddr = 0;
    state->xclient.peer_code = 0;
    state->xclient.protocol = 0;
    state->xclient.helo_name = 0;
}

/* smtpd_xclient_preset - set xclient attributes to "unknown" */

void    smtpd_xclient_preset(SMTPD_STATE *state)
{

    /*
     * This is a temporary solution. Unknown forwarded attributes get the
     * same values as unknown normal attributes, so that we don't break
     * assumptions in pre-existing code.
     */
    state->xclient.used = 1;
    state->xclient.name = mystrdup(CLIENT_NAME_UNKNOWN);
    state->xclient.addr = mystrdup(CLIENT_ADDR_UNKNOWN);
    state->xclient.namaddr = mystrdup(CLIENT_NAMADDR_UNKNOWN);
    state->xclient.protocol = mystrdup(CLIENT_PROTO_UNKNOWN);
}

/* smtpd_xclient_reset - reset XCLIENT attributes */

void    smtpd_xclient_reset(SMTPD_STATE *state)
{
#define FREE_AND_WIPE(s) { if (s) myfree(s); s = 0; }

    state->xclient.used = 0;
    FREE_AND_WIPE(state->xclient.name);
    FREE_AND_WIPE(state->xclient.addr);
    FREE_AND_WIPE(state->xclient.namaddr);
    state->xclient.peer_code = 0;
    FREE_AND_WIPE(state->xclient.protocol);
    FREE_AND_WIPE(state->xclient.helo_name);
}
