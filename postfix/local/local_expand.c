/*++
/* NAME
/*	local_expand 3
/* SUMMARY
/*	expand $name based on delivery attributes
/* SYNOPSIS
/*	#include "local.h"
/*
/*	int	local_expand(result, pattern, state, usr_attr, filter)
/*	VSTRING	*result;
/*	const char *pattern;
/*	LOCAL_STATE state;
/*	USER_ATTR usr_attr;
/*	const char *filter;
/* DESCRIPTION
/*	local_expand() expands $name instances on the basis of message
/*	delivery attributes.
/*
/*	Macros:
/* .IP $domain
/*	The recipient address domain.
/* .IP $extension
/*	The recipient address extension.
/* .IP $home
/*	The recipient home directory.
/* .IP $recipient
/*	The full recipient address.
/* .IP $recipient_delimiter
/*	The recipient delimiter.
/* .IP $shell
/*	The recipient shell program.
/* .IP $user
/*	The recipient user name.
/* .PP
/*	Arguments:
/* .IP result
/*	Storage for the result. The result is truncated upon entry.
/* .IP pattern
/*	The input with zero or more $name references.
/* .IP state
/*	Message delivery attributes (sender, recipient etc.).
/*	Attributes describing alias, include or forward expansion.
/*	A table with the results from expanding aliases or lists.
/*	A table with delivered-to: addresses taken from the message.
/* .IP usr_attr
/*	Attributes describing user rights and environment.
/* .IP filter
/*	A null pointer, or a null-terminated list of characters that
/*	are allowed to appear in the result if a $name expansion.
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

#include <vstring.h>
#include <mac_expand.h>

/* Global library. */

#include <mail_params.h>

/* Application-specific. */

#include "local.h"

/* local_expand - expand contents of .forward file */

int     local_expand(VSTRING *result, const char *pattern,
	          LOCAL_STATE state, USER_ATTR usr_attr, const char *filter)
{
    char   *domain;

    /*
     * Impedance matching between the local delivery agent data structures
     * and the mac_expand() interface. The CPU cycles wasted will be
     * negligible.
     */
    if ((domain = strrchr(state.msg_attr.recipient, '@')) != 0)
	domain++;

    return (mac_expand(result, pattern, MAC_EXP_FLAG_NONE,
		       MAC_EXP_ARG_FILTER, filter,
		       MAC_EXP_ARG_ATTR, "user", usr_attr.logname,
		       MAC_EXP_ARG_ATTR, "home", usr_attr.home,
		       MAC_EXP_ARG_ATTR, "shell", usr_attr.shell,
		       MAC_EXP_ARG_ATTR, "domain", domain,
		    MAC_EXP_ARG_ATTR, "recipient", state.msg_attr.recipient,
		    MAC_EXP_ARG_ATTR, "extension", state.msg_attr.extension,
		       MAC_EXP_ARG_ATTR, "recipient_delimiter",
		       *var_rcpt_delim ? var_rcpt_delim : 0,
		       0));
}
