/*++
/* NAME
/*	smtp_key 3
/* SUMMARY
/*	cache/table lookup key management
/* SYNOPSIS
/*	#include "smtp.h"
/*
/*	char	*smtp_key_prefix(buffer, iterator, context_flags)
/*	VSTRING	*buffer;
/*	SMTP_ITERATOR *iterator;
/*	int	context_flags;
/* DESCRIPTION
/*	The Postfix SMTP server accesses caches and lookup tables,
/*	using lookup keys that contain information from various
/*	contexts: per-server configuration, per-request envelope,
/*	and results from DNS queries.
/*
/*	These lookup keys sometimes share the same context information.
/*	The primary purpose of this API is to ensure that this
/*	shared context is used consistently, and that its use is
/*	made explicit (both are needed to verify that there is no
/*	false cache sharing).
/*
/*	smtp_key_prefix() constructs a lookup key prefix from context
/*	that may be shared with other lookup keys. The user is free
/*	to append additional application-specific context. The result
/*	value is a pointer to the result text.
/*
/*	Arguments:
/* .IP buffer
/*	Storage for the result.
/* .IP iterator
/*	Information that will be selected by the specified flags.
/* .IP context_flags
/*	Bit-wise OR of one or more of the following.
/* .RS
/* .IP SMTP_KEY_FLAG_SERVICE
/*	The global service name. This is a proxy for
/*	destination-independent and request-independent context.
/* .IP SMTP_KEY_FLAG_SENDER
/*	The envelope sender address. This is a proxy for sender-dependent
/*	context, such as per-sender SASL authentication. This flag
/*	is ignored when all sender-dependent context is disabled.
/* .IP SMTP_KEY_FLAG_REQ_NEXTHOP
/*	The request nexthop destination. This is a proxy for
/*	destination-dependent, but host-independent context.
/* .IP SMTP_KEY_FLAG_NEXTHOP
/*	The current iterator's nexthop destination (request nexthop
/*	or fallback nexthop, including optional [] and :port). This
/*	is the form that users specify in a SASL or TLS lookup
/*	tables.
/* .IP SMTP_KEY_FLAG_HOSTNAME
/*	The current iterator's remote hostname.
/* .IP SMTP_KEY_FLAG_ADDR
/*	The current iterator's remote address.
/* .IP SMTP_KEY_FLAG_PORT
/*	The current iterator's remote port.
/* .RE
/* DIAGNOSTICS
/*	Panic: undefined flag or zero flags. Fatal: out of memory.
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

 /*
  * System library.
  */
#include <sys_defs.h>
#include <netinet/in.h>			/* ntohs() for Solaris or BSD */
#include <arpa/inet.h>			/* ntohs() for Linux or BSD */

 /*
  * Utility library.
  */
#include <msg.h>
#include <vstring.h>

 /*
  * Global library.
  */
#include <mail_params.h>

 /*
  * Application-specific.
  */
#include <smtp.h>

/* smtp_key_prefix - format common elements in lookup key */

char   *smtp_key_prefix(VSTRING *buffer, SMTP_ITERATOR *iter, int flags)
{
    const char myname[] = "smtp_key_prefix";
    SMTP_STATE *state = iter->parent;	/* private member */

    /*
     * Sanity checks.
     */
    if (state == 0)
	msg_panic("%s: no parent state :-)", myname);
    if (flags & ~SMTP_KEY_MASK_ALL)
	msg_panic("%s: unknown key flags 0x%x",
		  myname, flags & ~SMTP_KEY_MASK_ALL);
    if (flags == 0)
	msg_panic("%s: zero flags", myname);

    /*
     * Initialize.
     */
    VSTRING_RESET(buffer);

    /*
     * per-service and per-request context.
     */
    if (flags & SMTP_KEY_FLAG_SERVICE)
	vstring_sprintf_append(buffer, "%s:", state->service);
    if (flags & SMTP_KEY_FLAG_SENDER)
	vstring_sprintf_append(buffer, "%s:",
			       var_smtp_sender_auth
			       && *var_smtp_sasl_passwd ?
			       state->request->sender : "*");

    /*
     * Per-destination context, non-canonicalized form.
     */
    if (flags & SMTP_KEY_FLAG_REQ_NEXTHOP)
	vstring_sprintf_append(buffer, "%s:", STR(iter->request_nexthop));
    if (flags & SMTP_KEY_FLAG_NEXTHOP)
	vstring_sprintf_append(buffer, "%s:", STR(iter->dest));

    /*
     * Per-host context, caonicalized form.
     */
    if (flags & SMTP_KEY_FLAG_HOSTNAME)
	vstring_sprintf_append(buffer, "%s:", STR(iter->host));
    if (flags & SMTP_KEY_FLAG_ADDR)
	vstring_sprintf_append(buffer, "%s:", STR(iter->addr));
    if (flags & SMTP_KEY_FLAG_PORT)
	vstring_sprintf_append(buffer, "%u:", ntohs(iter->port));

    return STR(buffer);
}
