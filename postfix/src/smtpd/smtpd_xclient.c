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
/*	void	smtpd_xclient_reset(state, mode)
/*	SMTPD_STATE *state;
/*	int	mode;
/* DESCRIPTION
/*	smtpd_xclient_init() initializes state variables that are
/*	used for storage of XCLIENT command parameters.
/*	These variables override specific members of the global state
/*	structure for access control or logging purposes.
/*
/*	smtpd_xclient_reset() releases memory allocated after the return
/*	from smtpd_xclient_init() and optionally presets the state variables 
/*	to defaults that are suitable for the specified mode:
/* .IP XCLIENT_OVER_NONE
/*	This should be used after the XCLIENT RST request.
/* .IP XCLIENT_OVER_ACL|XCLIENT_OVER_LOG
/*	This should be used after the XCLIENT ACL request.
/* .IP XCLIENT_OVER_LOG
/*	This should be used after the XCLIENT LOG request.
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
    state->xclient.mode = 0;
    state->xclient.name = 0;
    state->xclient.addr = 0;
    state->xclient.namaddr = 0;
    state->xclient.peer_code = 0;
    state->xclient.protocol = 0;
    state->xclient.helo_name = 0;
}

/* smtpd_xclient_reset - reset XCLIENT attributes */

void    smtpd_xclient_reset(SMTPD_STATE *state, int mode)
{
    switch (mode) {

	/*
	 * Can't happen.
	 */
    default:
	msg_panic("smtpd_xclient_reset: unknown mode 0x%x", mode);

	/*
	 * Restore smtpd_xclient_init() result. Allow selective override.
	 * This is desirable for access rule testing.
	 */
#define FREE_AND_WIPE(s) { if (s) { myfree(s); s = 0; } }

    case XCLIENT_OVER_NONE:
    case XCLIENT_OVER_ACL | XCLIENT_OVER_LOG:
	FREE_AND_WIPE(state->xclient.name);
	FREE_AND_WIPE(state->xclient.addr);
	FREE_AND_WIPE(state->xclient.namaddr);
	state->xclient.peer_code = 0;
	FREE_AND_WIPE(state->xclient.protocol);
	FREE_AND_WIPE(state->xclient.helo_name);
	break;

	/*
	 * Non-selective override. Set all attributes to "unknown". This is
	 * desirable to avoid polluting the audit trail with data from mixed
	 * origins.
	 */
#define FREE_AND_DUP(s,v) { if (s) { myfree(s); s = mystrdup(v); } }

    case XCLIENT_OVER_LOG:
	FREE_AND_DUP(state->xclient.name, CLIENT_NAME_UNKNOWN);
	FREE_AND_DUP(state->xclient.addr, CLIENT_ADDR_UNKNOWN);
	FREE_AND_DUP(state->xclient.namaddr, CLIENT_NAMADDR_UNKNOWN);
	state->xclient.peer_code = 0;
	FREE_AND_DUP(state->xclient.protocol, PROTOCOL_UNKNOWN);
	FREE_AND_DUP(state->xclient.helo_name, HELO_NAME_UNKNOWN);
	break;
    }
    state->xclient.mode = mode;
}
