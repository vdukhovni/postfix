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
/*	smtpd_xclient_init() initializes state variables that are
/*	used for storage of XCLIENT command parameters.
/*	These variables override specific members of the global state
/*	structure for access control or logging purposes.
/*
/*	smtpd_xclient_reset() releases memory allocated after the return
/*	from smtpd_xclient_init().
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
    state->xclient.name = mystrdup(CLIENT_NAME_UNKNOWN);
    state->xclient.addr = mystrdup(CLIENT_ADDR_UNKNOWN);
    state->xclient.namaddr = mystrdup(CLIENT_NAMADDR_UNKNOWN);
    state->xclient.peer_code = 0;
    state->xclient.protocol = mystrdup(PROTOCOL_UNKNOWN);
    state->xclient.helo_name = mystrdup(HELO_NAME_UNKNOWN);
}

/* smtpd_xclient_reset - reset XCLIENT attributes */

void    smtpd_xclient_reset(SMTPD_STATE *state)
{
    myfree(state->xclient.name);
    myfree(state->xclient.addr);
    myfree(state->xclient.namaddr);
    myfree(state->xclient.protocol);
    myfree(state->xclient.helo_name);
}
