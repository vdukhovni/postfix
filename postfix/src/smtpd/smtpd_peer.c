/*++
/* NAME
/*	smtpd_peer 3
/* SUMMARY
/*	look up peer name/address information
/* SYNOPSIS
/*	#include "smtpd.h"
/*
/*	void	smtpd_peer_init(state)
/*	SMTPD_STATE *state;
/*
/*	void	smtpd_peer_reset(state)
/*	SMTPD_STATE *state;
/* DESCRIPTION
/*	The smtpd_peer_init() routine attempts to produce a printable
/*	version of the peer name and address of the specified socket.
/*	Where information is unavailable, the name and/or address
/*	are set to "unknown".
/*
/*	This module uses the local name service via getaddrinfo()
/*	and getnameinfo(). It does not query the DNS directly.
/*
/*	smtpd_peer_init() updates the following fields:
/* .IP name
/*	The verified client hostname. This name is represented by
/*	the string "unknown" when 1) the address->name lookup failed,
/*	2) the name->address mapping fails, or 3) the name->address
/*	does not produce the client IP address.
/* .IP reverse_name
/*	The unverified client hostname as found with address->name
/*	lookup; it is not verified for consistency with the client
/*	IP address result from name->address lookup.
/* .IP forward_name
/*	The unverified client hostname as found with address->name
/*	lookup followed by name->address lookup; it is not verified
/*	for consistency with the result from address->name lookup.
/*	For example, when the address->name lookup produces as
/*	hostname an alias, the name->address lookup will produce
/*	as hostname the expansion of that alias, so that the two
/*	lookups produce different names.
/* .IP addr
/*	Printable representation of the client address.
/* .IP namaddr
/*	String of the form: "name[addr]".
/* .IP rfc_addr
/*      String of the form "ipv4addr" or "ipv6:ipv6addr" for use
/*	in Received: message headers.
/* .IP name_status
/*	The name_status result field specifies how the name
/*	information should be interpreted:
/* .RS
/* .IP 2
/*	The address->name lookup and name->address lookup produced
/*	the client IP address.
/* .IP 4
/*	The address->name lookup or name->address lookup failed
/*	with a recoverable error.
/* .IP 5
/*	The address->name lookup or name->address lookup failed
/*	with an unrecoverable error, or the result did not match
/*	the client IP address.
/* .RE
/* .IP reverse_name_status
/*	The reverse_name_status result field specifies how the
/*	reverse_name information should be interpreted:
/* .RS .IP 2
/*	The address->name lookup succeeded.
/* .IP 4
/*	The address->name lookup failed with a recoverable error.
/* .IP 5
/*	The address->name lookup failed with an unrecoverable error.
/* .RE .IP forward_name_status
/*	The forward_name_status result field specifies how the
/*	forward_name information should be interpreted:
/* .RS .IP 2
/*	The address->name and name->address lookup succeeded.
/* .IP 4
/*	The address->name lookup or name->address failed with a
/*	recoverable error.
/* .IP 5
/*	The address->name lookup or name->address failed with an
/*	unrecoverable error.
/* .RE
/* .PP
/*	smtpd_peer_reset() releases memory allocated by smtpd_peer_init().
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>			/* strerror() */
#include <errno.h>
#include <netdb.h>
#include <string.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <stringops.h>
#include <myaddrinfo.h>
#include <sock_addr.h>
#include <inet_proto.h>

/* Global library. */

#include <mail_proto.h>
#include <valid_mailhost_addr.h>
#include <mail_params.h>

/* Application-specific. */

#include "smtpd.h"

/* smtpd_peer_init - initialize peer information */

void    smtpd_peer_init(SMTPD_STATE *state)
{
    char   *myname = "smtpd_peer_init";
    SOCKADDR_SIZE sa_len;
    struct sockaddr *sa;
    INET_PROTO_INFO *proto_info = inet_proto_info();

    sa = (struct sockaddr *) & (state->sockaddr);
    sa_len = sizeof(state->sockaddr);

    /*
     * Look up the peer address information.
     */
    if (getpeername(vstream_fileno(state->client), sa, &sa_len) >= 0) {
	errno = 0;
    }

    /*
     * If peer went away, give up.
     */
    if (errno == ECONNRESET || errno == ECONNABORTED) {
	state->name = mystrdup(CLIENT_NAME_UNKNOWN);
	state->reverse_name = mystrdup(CLIENT_NAME_UNKNOWN);
#ifdef FORWARD_CLIENT_NAME
	state->forward_name = mystrdup(CLIENT_NAME_UNKNOWN);
#endif
	state->addr = mystrdup(CLIENT_ADDR_UNKNOWN);
	state->rfc_addr = mystrdup(CLIENT_ADDR_UNKNOWN);
	state->name_status = SMTPD_PEER_CODE_PERM;
	state->reverse_name_status = SMTPD_PEER_CODE_PERM;
#ifdef FORWARD_CLIENT_NAME
	state->forward_name_status = SMTPD_PEER_CODE_PERM;
#endif
    }

    /*
     * Convert the client address to printable address and hostname.
     */
    else if (errno == 0
	     && strchr((char *) proto_info->sa_family_list, sa->sa_family)) {
	MAI_HOSTNAME_STR client_name;
	MAI_HOSTADDR_STR client_addr;
	int     aierr;
	char   *colonp;

	/*
	 * Convert the client address to printable form.
	 */
	if ((aierr = sockaddr_to_hostaddr(sa, sa_len, &client_addr,
					  (MAI_SERVPORT_STR *) 0, 0)) != 0)
	    msg_fatal("%s: cannot convert client address to string: %s",
		      myname, MAI_STRERROR(aierr));

	/*
	 * We convert IPv4-in-IPv6 address to 'true' IPv4 address early on,
	 * but only if IPv4 support is enabled (why would anyone want to turn
	 * it off)? With IPv4 support enabled we have no need for the IPv6
	 * form in logging, hostname verification and access checks.
	 */
#ifdef HAS_IPV6
	if (sa->sa_family == AF_INET6) {
	    if (strchr((char *) proto_info->sa_family_list, AF_INET) != 0
		&& IN6_IS_ADDR_V4MAPPED(&SOCK_ADDR_IN6_ADDR(sa))
		&& (colonp = strrchr(client_addr.buf, ':')) != 0) {
		struct addrinfo *res0;

		if (msg_verbose > 1)
		    msg_info("%s: rewriting V4-mapped address \"%s\" to \"%s\"",
			     myname, client_addr.buf, colonp + 1);

		state->addr = mystrdup(colonp + 1);
		state->rfc_addr = mystrdup(colonp + 1);
		aierr = hostaddr_to_sockaddr(state->addr, (char *) 0, 0, &res0);
		if (aierr)
		    msg_fatal("%s: cannot convert %s from string to binary: %s",
			      myname, state->addr, MAI_STRERROR(aierr));
		sa_len = res0->ai_addrlen;
		if (sa_len > sizeof(state->sockaddr))
		    sa_len = sizeof(state->sockaddr);
		memcpy((char *) sa, res0->ai_addr, sa_len);
		freeaddrinfo(res0);		/* 200412 */
	    }

	    /*
	     * Following RFC 2821 section 4.1.3, an IPv6 address literal gets
	     * a prefix of 'IPv6:'. We do this consistently for all IPv6
	     * addresses that that appear in headers or envelopes. The fact
	     * that valid_mailhost_addr() enforces the form helps of course.
	     * We use the form without IPV6: prefix when doing access
	     * control, or when accessing the connection cache.
	     */
	    else {
		state->addr = mystrdup(client_addr.buf);
		state->rfc_addr =
		    concatenate(IPV6_COL, client_addr.buf, (char *) 0);
	    }
	}

	/*
	 * An IPv4 address is in dotted quad decimal form.
	 */
	else
#endif
	{
	    state->addr = mystrdup(client_addr.buf);
	    state->rfc_addr = mystrdup(client_addr.buf);
	}

	/*
	 * Look up and sanity check the client hostname.
	 * 
	 * It is unsafe to allow numeric hostnames, especially because there
	 * exists pressure to turn off the name->addr double check. In that
	 * case an attacker could trivally bypass access restrictions.
	 * 
	 * sockaddr_to_hostname() already rejects malformed or numeric names.
	 */
#define TEMP_AI_ERROR(e) \
	((e) == EAI_AGAIN || (e) == EAI_MEMORY || (e) == EAI_SYSTEM)

#define REJECT_PEER_NAME(state, code) { \
	myfree(state->name); \
	state->name = mystrdup(CLIENT_NAME_UNKNOWN); \
	state->name_status = code; \
    }

	if (var_smtpd_peername_lookup == 0) {
	    state->name = mystrdup(CLIENT_NAME_UNKNOWN);
	    state->reverse_name = mystrdup(CLIENT_NAME_UNKNOWN);
#ifdef FORWARD_CLIENT_NAME
	    state->forward_name = mystrdup(CLIENT_NAME_UNKNOWN);
#endif
	    state->name_status = SMTPD_PEER_CODE_PERM;
	    state->reverse_name_status = SMTPD_PEER_CODE_PERM;
#ifdef FORWARD_CLIENT_NAME
	    state->forward_name_status = SMTPD_PEER_CODE_PERM;
#endif
	} else if ((aierr = sockaddr_to_hostname(sa, sa_len, &client_name,
					 (MAI_SERVNAME_STR *) 0, 0)) != 0) {
	    state->name = mystrdup(CLIENT_NAME_UNKNOWN);
	    state->reverse_name = mystrdup(CLIENT_NAME_UNKNOWN);
#ifdef FORWARD_CLIENT_NAME
	    state->forward_name = mystrdup(CLIENT_NAME_UNKNOWN);
#endif
	    state->name_status = (TEMP_AI_ERROR(aierr) ?
			       SMTPD_PEER_CODE_TEMP : SMTPD_PEER_CODE_PERM);
	    state->reverse_name_status = (TEMP_AI_ERROR(aierr) ?
			       SMTPD_PEER_CODE_TEMP : SMTPD_PEER_CODE_PERM);
#ifdef FORWARD_CLIENT_NAME
	    state->forward_name_status = (TEMP_AI_ERROR(aierr) ?
			       SMTPD_PEER_CODE_TEMP : SMTPD_PEER_CODE_PERM);
#endif
	} else {
	    struct addrinfo *res0;
	    struct addrinfo *res;

	    state->name = mystrdup(client_name.buf);
	    state->reverse_name = mystrdup(client_name.buf);
	    state->name_status = SMTPD_PEER_CODE_OK;
	    state->reverse_name_status = SMTPD_PEER_CODE_OK;

	    /*
	     * Reject the hostname if it does not list the peer address.
	     * Without further validation or qualification, such information
	     * must not be allowed to enter the audit trail, as people would
	     * draw false conclusions.
	     */
	    aierr = hostname_to_sockaddr(state->name, (char *) 0, 0, &res0);
	    if (aierr) {
		msg_warn("%s: hostname %s verification failed: %s",
			 state->addr, state->name, MAI_STRERROR(aierr));
#ifdef FORWARD_CLIENT_NAME
		state->forward_name = mystrdup(CLIENT_NAME_UNKNOWN);
		state->forward_name_status = (TEMP_AI_ERROR(aierr) ?
			       SMTPD_PEER_CODE_TEMP : SMTPD_PEER_CODE_PERM);
#endif
		REJECT_PEER_NAME(state, (TEMP_AI_ERROR(aierr) ?
			      SMTPD_PEER_CODE_TEMP : SMTPD_PEER_CODE_PERM));
	    } else {
#ifdef FORWARD_CLIENT_NAME
		if (res0) {
		    state->forward_name = mystrdup(res0->ai_canonname);
		    state->forward_name_status = SMTPD_PEER_CODE_OK;
		} else {
		    state->forward_name = mystrdup(CLIENT_NAME_UNKNOWN);
		    state->forward_name_status = SMTPD_PEER_CODE_PERM;
		}
#endif
		for (res = res0; /* void */ ; res = res->ai_next) {
		    if (res == 0) {
			msg_warn("%s: address not listed for hostname %s",
				 state->addr, state->name);
			REJECT_PEER_NAME(state, SMTPD_PEER_CODE_PERM);
			break;
		    }
		    if (strchr((char *) proto_info->sa_family_list, res->ai_family) == 0) {
			msg_info("skipping address family %d for host %s",
				 res->ai_family, state->name);
			continue;
		    }
		    if (sock_addr_cmp_addr(res->ai_addr, sa) == 0)
			break;			/* keep peer name */
		}
		freeaddrinfo(res0);
	    }
	}
    }

    /*
     * If it's not Internet, assume the client is local, and avoid using the
     * naming service because that can hang when the machine is disconnected.
     */
    else {
	state->name = mystrdup("localhost");
	state->reverse_name = mystrdup("localhost");
#ifdef FORWARD_CLIENT_NAME
	state->forward_name = mystrdup("localhost");
#endif
	state->addr = mystrdup("127.0.0.1");	/* XXX bogus. */
	state->rfc_addr = mystrdup("127.0.0.1");/* XXX bogus. */
	state->name_status = SMTPD_PEER_CODE_OK;
	state->reverse_name_status = SMTPD_PEER_CODE_OK;
#ifdef FORWARD_CLIENT_NAME
	state->forward_name_status = SMTPD_PEER_CODE_OK;
#endif
    }

    /*
     * Do the name[addr] formatting for pretty reports.
     */
    state->namaddr =
	concatenate(state->name, "[", state->addr, "]", (char *) 0);
}

/* smtpd_peer_reset - destroy peer information */

void    smtpd_peer_reset(SMTPD_STATE *state)
{
    myfree(state->name);
    myfree(state->reverse_name);
#ifdef FORWARD_CLIENT_NAME
    myfree(state->forward_name);
#endif
    myfree(state->addr);
    myfree(state->namaddr);
    myfree(state->rfc_addr);
}
