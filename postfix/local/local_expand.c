/*++
/* NAME
/*	local_expand 3
/* SUMMARY
/*	set up attribute list for $name expansion
/* SYNOPSIS
/*	#include "local.h"
/*
/*	HTABLE *local_expand(state, usr_attr)
/*	LOCAL_STATE state;
/*	USER_ATTR usr_attr;
/* DESCRIPTION
/*	local_expand() instantiates an attribute table for $name
/* 	expansion.
/*
/*	Attributes:
/* .IP domain
/*	The recipient address domain.
/* .IP extension
/*	The recipient address extension.
/* .IP home
/*	The recipient home directory.
/* .IP mailbox
/*	The full recipient address localpart.
/* .IP recipient
/*	The full recipient address.
/* .IP recipient_delimiter
/*	The recipient delimiter.
/* .IP shell
/*	The recipient shell program.
/* .IP user
/*	The recipient user name.
/* .PP
/*	Arguments:
/* .IP state
/*	Message delivery attributes (sender, recipient etc.).
/*	Attributes describing alias, include or forward expansion.
/*	A table with the results from expanding aliases or lists.
/*	A table with delivered-to: addresses taken from the message.
/* .IP usr_attr
/*	Attributes describing user rights and environment.
/* DIAGNOSTICS
/*	Fatal errors: out of memory.
/* SEE ALSO
/*	mac_expand(3) macro expansion
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

#include <htable.h>

/* Global library */

#include <mail_params.h>

/* Application-specific. */

#include "local.h"

/* local_expand - set up macro expansion attributes */

HTABLE *local_expand(LOCAL_STATE state, USER_ATTR usr_attr)
{
    HTABLE *expand_attr;

    /*
     * Impedance matching between the local delivery agent data structures
     * and the mac_expand() interface. The CPU cycles wasted will be
     * negligible.
     */
    expand_attr = htable_create(0);
    htable_enter(expand_attr, "user", usr_attr.logname);
    htable_enter(expand_attr, "home", usr_attr.home);
    htable_enter(expand_attr, "shell", usr_attr.shell);
    htable_enter(expand_attr, "domain", state.msg_attr.domain);
    htable_enter(expand_attr, "mailbox", state.msg_attr.local);
    htable_enter(expand_attr, "recipient", state.msg_attr.recipient);
    htable_enter(expand_attr, "extension", state.msg_attr.extension);
    htable_enter(expand_attr, "recipient_delimiter", var_rcpt_delim);
    return (expand_attr);
}
