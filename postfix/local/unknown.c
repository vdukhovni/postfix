/*++
/* NAME
/*	unknown 3
/* SUMMARY
/*	delivery of unknown recipients
/* SYNOPSIS
/*	#include "local.h"
/*
/*	int	deliver_unknown(state, usr_attr)
/*	LOCAL_STATE state;
/*	USER_ATTR usr_attr;
/* DESCRIPTION
/*	deliver_unknown() delivers a message for unknown recipients.
/* .IP \(bu
/*	If an alternative message transport is specified via the
/*	fallback_transport parameter, delivery is delegated to the
/*	named transport.
/* .IP \(bu
/*	If an alternative address is specified via the luser_relay 
/*	configuration parameter, mail is forwarded to that address.
/* .IP \(bu
/*	Otherwise the recipient is bounced.
/* .PP
/*	If the luser_relay parameter specifies a @domain, the entire
/*	original recipient localpart is prepended. For example: with
/*	"luser_relay = @some.where", unknown+foo becomes
/*	unknown+foo@some.where.
/*
/*	Otherwise, the luser_relay parameter can specify any number of
/*	destinations that are valid in an alias file or in a .forward file.
/*	For example, a destination could be an address, a "|command" or
/*	a /file/name. The luser_relay feature is treated as an alias, and
/*	the usual restrictions for command and file destinations apply.
/*
/*	If the luser_relay destination is a mail address, and the
/*	recipient delimiter has been defined, the entire original recipient
/*	localpart is appended as an address extension. For example: with
/*	"luser_relay = someone@some.where", unknown+foo becomes
/*	someone+unknown+foo@some.where.
/*
/*	Arguments:
/* .IP state
/*	Message delivery attributes (sender, recipient etc.).
/*	Attributes describing alias, include or forward expansion.
/*	A table with the results from expanding aliases or lists.
/*	A table with delivered-to: addresses taken from the message.
/* .IP usr_attr
/*	Attributes describing user rights and environment.
/* DIAGNOSTICS
/*	The result status is non-zero when delivery should be tried again.
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
#include <stringops.h>
#include <mymalloc.h>

/* Global library. */

#include <been_here.h>
#include <mail_params.h>
#include <mail_proto.h>
#include <bounce.h>

/* Application-specific. */

#include "local.h"

/* deliver_unknown - delivery for unknown recipients */

int     deliver_unknown(LOCAL_STATE state, USER_ATTR usr_attr)
{
    char   *myname = "deliver_unknown";
    int     status;
    char   *dest;
    char   *saved_extension;

    /*
     * Make verbose logging easier to understand.
     */
    state.level++;
    if (msg_verbose)
	MSG_LOG_STATE(myname, state);

    /*
     * DUPLICATE/LOOP ELIMINATION
     * 
     * Don't deliver the same user twice.
     */
    if (been_here(state.dup_filter, "%s %s", myname, state.msg_attr.local))
	return (0);

    /*
     * The fall-back transport specifies a delivery machanism that handles
     * users not found in the aliases or UNIX passwd databases.
     */
    if (*var_fallback_transport)
	return (deliver_pass(MAIL_CLASS_PRIVATE, var_fallback_transport,
			     state.request, state.msg_attr.recipient, -1L));

    /*
     * Bounce the message when no luser relay is specified.
     */
    if (*var_luser_relay == 0)
	return (bounce_append(BOUNCE_FLAG_KEEP, BOUNCE_ATTR(state.msg_attr),
			      "unknown user: \"%s\"", state.msg_attr.local));

    /*
     * EXTERNAL LOOP CONTROL
     * 
     * Set the delivered message attribute to the recipient, so that this
     * message will list the correct forwarding address.
     */
    state.msg_attr.delivered = state.msg_attr.recipient;

    /*
     * DELIVERY POLICY
     * 
     * The luser relay is just another alias. Update the expansion type
     * attribute, so we can decide if deliveries to |command and /file/name
     * are allowed at all.
     */
    state.msg_attr.exp_type = EXPAND_TYPE_ALIAS;

    /*
     * DELIVERY RIGHTS
     * 
     * What rights to use for |command and /file/name deliveries? The luser
     * relay is a root-owned alias, so we use default rights.
     */
    RESET_USER_ATTR(usr_attr, state.level);

    /*
     * If the luser destination is specified as @domain, prepend the
     * localpart. The local resolver will append the optional address
     * extension, so we don't do that here.
     */
    if (*var_luser_relay == '@') {		/* @domain */
	dest = concatenate(state.msg_attr.local, var_luser_relay, (char *) 0);
	status = deliver_token_string(state, usr_attr, dest, (int *) 0);
	myfree(dest);
    }

    /*
     * Otherwise, optionally arrange for the local resolver to append the
     * entire localpart, including the optional address extension, to the
     * destination localpart.
     */
    else {					/* other */
	if ((saved_extension = state.msg_attr.extension) != 0)
	    state.msg_attr.extension = concatenate(state.msg_attr.local,
						   var_rcpt_delim,
						   state.msg_attr.extension,
						   (char *) 0);
	else if (*var_rcpt_delim)
	    state.msg_attr.extension = state.msg_attr.local;
	status = deliver_token_string(state, usr_attr, var_luser_relay,
				      (int *) 0);
	if (saved_extension != 0)
	    myfree(state.msg_attr.extension);
	state.msg_attr.extension = saved_extension;
    }

    /*
     * Done.
     */
    return (status);
}
