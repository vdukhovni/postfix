/*++
/* NAME
/*	smtp_connect 3
/* SUMMARY
/*	connect to SMTP server and deliver
/* SYNOPSIS
/*	#include "smtp.h"
/*
/*	int	smtp_connect(state)
/*	SMTP_STATE *state;
/* DESCRIPTION
/*	This module implements SMTP connection management and controls
/*	mail delivery.
/*
/*	smtp_connect() attempts to establish an SMTP session with a host
/*	that represents the destination domain, or with an optional fallback
/*	relay when the destination cannot be found, or when all the
/*	destination servers are unavailable. It skips over IP addresses
/*	that fail to complete the SMTP handshake and tries to find
/*	an alternate server when an SMTP session fails to deliver.
/*
/*	This layer also controls what sessions are retrieved from
/*	the session cache, and what sessions are saved to the cache.
/*
/*	The destination is either a host (or domain) name or a numeric
/*	address. Symbolic or numeric service port information may be
/*	appended, separated by a colon (":").
/*
/*	By default, the Internet domain name service is queried for mail
/*	exchanger hosts. Quote the domain name with `[' and `]' to
/*	suppress mail exchanger lookups.
/*
/*	Numerical address information should always be quoted with `[]'.
/* DIAGNOSTICS
/*	The delivery status is the result value.
/* SEE ALSO
/*	smtp_proto(3) SMTP client protocol
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*
/*	Connection caching in cooperation with:
/*	Victor Duchovni
/*	Morgan Stanley
/*--*/

/* System library. */

#include <sys_defs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

#ifndef IPPORT_SMTP
#define IPPORT_SMTP 25
#endif

/* Utility library. */

#include <msg.h>
#include <vstream.h>
#include <vstring.h>
#include <split_at.h>
#include <mymalloc.h>
#include <inet_addr_list.h>
#include <iostuff.h>
#include <timed_connect.h>
#include <stringops.h>
#include <host_port.h>
#include <sane_connect.h>

/* Global library. */

#include <mail_params.h>
#include <own_inet_addr.h>
#include <deliver_pass.h>
#include <mail_error.h>

/* DNS library. */

#include <dns.h>

/* Application-specific. */

#include <smtp.h>
#include <smtp_addr.h>
#include <smtp_reuse.h>

#define STR(x) vstring_str(x)

/* smtp_connect_addr - connect to explicit address */

static SMTP_SESSION *smtp_connect_addr(const char *dest, DNS_RR *addr,
				               unsigned port, VSTRING *why,
				               int sess_flags)
{
    char   *myname = "smtp_connect_addr";
    struct sockaddr_in sin;
    int     sock;
    INET_ADDR_LIST *addr_list;
    int     conn_stat;
    int     saved_errno;
    VSTREAM *stream;
    int     ch;
    unsigned long inaddr;

    smtp_errno = SMTP_ERR_NONE;			/* Paranoia */

    /*
     * Sanity checks.
     */
    if (addr->data_len > sizeof(sin.sin_addr)) {
	msg_warn("%s: skip address with length %d", myname, addr->data_len);
	smtp_errno = SMTP_ERR_RETRY;
	return (0);
    }

    /*
     * Initialize.
     */
    memset((char *) &sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;

    if ((sock = socket(sin.sin_family, SOCK_STREAM, 0)) < 0)
	msg_fatal("%s: socket: %m", myname);

    /*
     * Allow the sysadmin to specify the source address, for example, as "-o
     * smtp_bind_address=x.x.x.x" in the master.cf file.
     */
    if (*var_smtp_bind_addr) {
	sin.sin_addr.s_addr = inet_addr(var_smtp_bind_addr);
	if (sin.sin_addr.s_addr == INADDR_NONE)
	    msg_fatal("%s: bad %s parameter: %s",
		      myname, VAR_SMTP_BIND_ADDR, var_smtp_bind_addr);
	if (bind(sock, (struct sockaddr *) & sin, sizeof(sin)) < 0)
	    msg_warn("%s: bind %s: %m", myname, inet_ntoa(sin.sin_addr));
	if (msg_verbose)
	    msg_info("%s: bind %s", myname, inet_ntoa(sin.sin_addr));
    }

    /*
     * When running as a virtual host, bind to the virtual interface so that
     * the mail appears to come from the "right" machine address.
     */
    else if ((addr_list = own_inet_addr_list())->used == 1) {
	memcpy((char *) &sin.sin_addr, addr_list->addrs, sizeof(sin.sin_addr));
	inaddr = ntohl(sin.sin_addr.s_addr);
	if (!IN_CLASSA(inaddr)
	    || !(((inaddr & IN_CLASSA_NET) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET)) {
	    if (bind(sock, (struct sockaddr *) & sin, sizeof(sin)) < 0)
		msg_warn("%s: bind %s: %m", myname, inet_ntoa(sin.sin_addr));
	    if (msg_verbose)
		msg_info("%s: bind %s", myname, inet_ntoa(sin.sin_addr));
	}
    }

    /*
     * Connect to the SMTP server.
     */
    sin.sin_port = port;
    memcpy((char *) &sin.sin_addr, addr->data, sizeof(sin.sin_addr));

    if (msg_verbose)
	msg_info("%s: trying: %s[%s] port %d...",
		 myname, addr->name, inet_ntoa(sin.sin_addr), ntohs(port));
    if (var_smtp_conn_tmout > 0) {
	non_blocking(sock, NON_BLOCKING);
	conn_stat = timed_connect(sock, (struct sockaddr *) & sin,
				  sizeof(sin), var_smtp_conn_tmout);
	saved_errno = errno;
	non_blocking(sock, BLOCKING);
	errno = saved_errno;
    } else {
	conn_stat = sane_connect(sock, (struct sockaddr *) & sin, sizeof(sin));
    }
    if (conn_stat < 0) {
	vstring_sprintf(why, "connect to %s[%s]: %m",
			addr->name, inet_ntoa(sin.sin_addr));
	smtp_errno = SMTP_ERR_RETRY;
	close(sock);
	return (0);
    }

    /*
     * Skip this host if it takes no action within some time limit.
     */
    if (read_wait(sock, var_smtp_helo_tmout) < 0) {
	vstring_sprintf(why, "connect to %s[%s]: read timeout",
			addr->name, inet_ntoa(sin.sin_addr));
	smtp_errno = SMTP_ERR_RETRY;
	close(sock);
	return (0);
    }

    /*
     * Skip this host if it disconnects without talking to us.
     */
    stream = vstream_fdopen(sock, O_RDWR);
    if ((ch = VSTREAM_GETC(stream)) == VSTREAM_EOF) {
	vstring_sprintf(why, "connect to %s[%s]: server dropped connection without sending the initial SMTP greeting",
			addr->name, inet_ntoa(sin.sin_addr));
	smtp_errno = SMTP_ERR_RETRY;
	vstream_fclose(stream);
	return (0);
    }
    vstream_ungetc(stream, ch);
    return (smtp_session_alloc(stream, dest, addr->name,
			       inet_ntoa(sin.sin_addr), port, sess_flags));
}

/* smtp_parse_destination - parse destination */

static char *smtp_parse_destination(char *destination, char *def_service,
				            char **hostp, unsigned *portp)
{
    char   *buf = mystrdup(destination);
    char   *service;
    struct servent *sp;
    char   *protocol = "tcp";		/* XXX configurable? */
    unsigned port;
    const char *err;

    if (msg_verbose)
	msg_info("smtp_parse_destination: %s %s", destination, def_service);

    /*
     * Parse the host/port information. We're working with a copy of the
     * destination argument so the parsing can be destructive.
     */
    if ((err = host_port(buf, hostp, &service, def_service)) != 0)
	msg_fatal("%s in SMTP server description: %s", err, destination);

    /*
     * Convert service to port number, network byte order.
     */
    if (alldig(service) && (port = atoi(service)) != 0) {
	*portp = htons(port);
    } else {
	if ((sp = getservbyname(service, protocol)) == 0)
	    msg_fatal("unknown service: %s/%s", service, protocol);
	*portp = sp->s_port;
    }
    return (buf);
}

/* smtp_cleanup_session - clean up after using a session */

static void smtp_cleanup_session(SMTP_STATE *state)
{
    SMTP_SESSION *session = state->session;

    /*
     * Inform the postmaster of trouble.
     */
    if (session->history != 0
	&& (session->error_mask & name_mask(VAR_NOTIFY_CLASSES,
					    mail_error_masks,
					    var_notify_classes)) != 0)
	smtp_chat_notify(session);

    /*
     * When session caching is enabled, cache the first good session for this
     * delivery request under the next-hop destination, and cache all good
     * sessions under their server network address (destroying the session in
     * the process).
     * 
     * Caching under the next-hop destination name (rather than the fall-back
     * destination) allows us to skip over non-responding primary or backup
     * hosts. In fact, this is the only benefit of caching logical to
     * physical bindings; caching a session under its own hostname provides
     * no performance benefit, given the way smtp_connect() works.
     * 
     * XXX Should not cache TLS sessions unless we are using a single-session,
     * in-process, cache. And if we did, we should passivate VSTREAM objects
     * in addition to passivating SMTP_SESSION objects.
     */
    if (session->reuse_count > 0) {
	smtp_save_session(state);
	if (HAVE_NEXTHOP_STATE(state))
	    FREE_NEXTHOP_STATE(state);
    } else {
	smtp_session_free(session);
    }
    state->session = 0;

    /*
     * Clean up the lists with todo and dropped recipients.
     */
    smtp_rcpt_cleanup(state);
}

/* smtp_scrub_address_list - delete all cached addresses from list */

static void smtp_scrub_addr_list(HTABLE *cached_addr, DNS_RR **addr_list)
{
    DNS_RR *addr;
    DNS_RR *next;

#define INADDRP(x) ((struct in_addr *) (x))

    for (addr = *addr_list; addr; addr = next) {
	next = addr->next;
	if (addr->type == T_A) {
	    if (addr->data_len > sizeof(struct in_addr))
		continue;
	    if (htable_locate(cached_addr, inet_ntoa(*INADDRP(addr->data))))
		*addr_list = dns_rr_remove(*addr_list, addr);
	}
    }
}

/* smtp_update_addr_list - common address list update */

static void smtp_update_addr_list(DNS_RR **addr_list, const char *server_addr,
				          int session_count)
{
    struct in_addr server_in_addr;
    DNS_RR *addr;
    DNS_RR *next;

    if (*addr_list == 0)
	return;

    /*
     * Truncate the address list if we are not going to use it anyway.
     */
    if (session_count == var_smtp_mxsess_limit
	|| session_count == var_smtp_mxaddr_limit) {
	dns_rr_free(*addr_list);
	*addr_list = 0;
	return;
    }

    /*
     * Convert server address to internal form, and look it up in the address
     * list.
     * 
     * XXX smtp_reuse_session() breaks if we remove two or more adjacent list
     * elements but do not truncate the list to zero length.
     */
    server_in_addr.s_addr = inet_addr(server_addr);
    for (addr = *addr_list; addr; addr = next) {
	next = addr->next;
	if (addr->type == T_A) {		/* NOT: switch */
	    if (addr->data_len > sizeof(server_in_addr))
		continue;
	    if (INADDRP(addr->data)->s_addr == server_in_addr.s_addr) {
		*addr_list = dns_rr_remove(*addr_list, addr);
		break;
	    }
	}
    }
}

/* smtp_reuse_session - try to use existing connection, return session count */

static int smtp_reuse_session(SMTP_STATE *state, int lookup_mx,
			              const char *domain, unsigned port,
			           DNS_RR **addr_list, int domain_best_pref)
{
    int     session_count = 0;
    DNS_RR *addr;
    DNS_RR *next;
    int     saved_final_server = state->final_server;
    SMTP_SESSION *session;

    /*
     * First, search the cache by logical destination. We truncate the server
     * address list when all the sessions for this destination are used up,
     * to reduce the number of variables that need to be checked later.
     * 
     * Note: lookup by logical destination restores the "best MX" bit.
     */
    if (*addr_list && SMTP_RCPT_LEFT(state) > 0
    && (session = smtp_reuse_domain(state, lookup_mx, domain, port)) != 0) {
	session_count = 1;
	smtp_update_addr_list(addr_list, session->addr, session_count);
	state->final_server = (saved_final_server && *addr_list == 0);
	smtp_xfer(state);
	smtp_cleanup_session(state);
    }

    /*
     * Second, search the cache by primary MX address. Again, we use address
     * list truncation so that we have to check fewer variables later.
     * 
     * XXX This loop is safe because smtp_update_addr_list() either truncates
     * the list to zero length, or removes at most one list element.
     */
    for (addr = *addr_list; SMTP_RCPT_LEFT(state) > 0 && addr; addr = next) {
	if (addr->pref != domain_best_pref)
	    break;
	next = addr->next;
	if ((session = smtp_reuse_addr(state, addr, port)) != 0) {
	    session->features |= SMTP_FEATURE_BEST_MX;
	    session_count += 1;
	    smtp_update_addr_list(addr_list, session->addr, session_count);
	    if (*addr_list == 0)
		next = 0;
	    state->final_server = (saved_final_server && next == 0);
	    smtp_xfer(state);
	    smtp_cleanup_session(state);
	}
    }
    return (session_count);
}

/* smtp_connect - establish SMTP connection */

int     smtp_connect(SMTP_STATE *state)
{
    DELIVER_REQUEST *request = state->request;
    VSTRING *why = vstring_alloc(10);
    char   *dest_buf;
    char   *domain;
    unsigned port;
    char   *def_service = "smtp";	/* XXX ##IPPORT_SMTP? */
    ARGV   *sites;
    char   *dest;
    char  **cpp;
    DNS_RR *addr_list;
    DNS_RR *addr;
    DNS_RR *next;
    int     addr_count;
    int     sess_count;
    int     misc_flags = SMTP_MISC_FLAG_DEFAULT;
    SMTP_SESSION *session;
    int     lookup_mx;
    unsigned domain_best_pref;
    int     sess_flags = SMTP_SESS_FLAG_NONE;

    /*
     * First try to deliver to the indicated destination, then try to deliver
     * to the optional fall-back relays.
     * 
     * Future proofing: do a null destination sanity check in case we allow the
     * primary destination to be a list (it could be just separators).
     */
    sites = argv_alloc(1);
    argv_add(sites, request->nexthop, (char *) 0);
    if (sites->argc == 0)
	msg_panic("null destination: \"%s\"", request->nexthop);
    argv_split_append(sites, var_fallback_relay, ", \t\r\n");

    /*
     * Don't give up after a hard host lookup error until we have tried the
     * fallback relay servers.
     * 
     * Don't bounce mail after a host lookup problem with a relayhost or with a
     * fallback relay.
     * 
     * Don't give up after a qualifying soft error until we have tried all
     * qualifying backup mail servers.
     * 
     * All this means that error handling and error reporting depends on whether
     * the error qualifies for trying to deliver to a backup mail server, or
     * whether we're looking up a relayhost or fallback relay. The challenge
     * then is to build this into the pre-existing SMTP client without
     * getting lost in the complexity.
     */
    for (cpp = sites->argv; SMTP_RCPT_LEFT(state) > 0 && (dest = *cpp) != 0; cpp++) {
	state->final_server = (cpp[1] == 0);

	/*
	 * Parse the destination. Default is to use the SMTP port. Look up
	 * the address instead of the mail exchanger when a quoted host is
	 * specified, or when DNS lookups are disabled.
	 */
	dest_buf = smtp_parse_destination(dest, def_service, &domain, &port);

	/*
	 * Resolve an SMTP server. Skip mail exchanger lookups when a quoted
	 * host is specified, or when DNS lookups are disabled.
	 */
	if (msg_verbose)
	    msg_info("connecting to %s port %d", domain, ntohs(port));
	if (ntohs(port) != IPPORT_SMTP)
	    misc_flags &= ~SMTP_MISC_FLAG_LOOP_DETECT;
	else
	    misc_flags |= SMTP_MISC_FLAG_LOOP_DETECT;
	lookup_mx = (var_disable_dns == 0 && *dest != '[');
	if (!lookup_mx) {
	    addr_list = smtp_host_addr(domain, misc_flags, why);
	} else {
	    addr_list = smtp_domain_addr(domain, misc_flags, why);
	}

	/*
	 * When session caching is enabled, store the first good session for
	 * this delivery request under the next-hop destination name. All
	 * good sessions will be stored under their specific server IP
	 * address.
	 * 
	 * XXX Replace sites->argv by (lookup_mx, domain, port) triples so we
	 * don't have to make clumsy ad-hoc copies and keep track of who
	 * free()s the memory.
	 * 
	 * XXX smtp_session_cache_destinations specifies domain names without
	 * :port, because : is already used for maptype:mapname. Because of
	 * this limitation we use the bare domain without the optional [] or
	 * non-default TCP port.
	 * 
	 * Opportunistic (a.k.a. on-demand) session caching on request by the
	 * queue manager. This is turned temporarily when a destination has a
	 * high volume of mail in the active queue.
	 */
	if (cpp == sites->argv
	    && ((request->flags & DEL_REQ_FLAG_SCACHE) != 0
		|| (smtp_cache_dest && string_list_match(smtp_cache_dest, domain)))) {
	    sess_flags |= SMTP_SESS_FLAG_CACHE;
	    SET_NEXTHOP_STATE(state, lookup_mx, domain, port);
	}

	/*
	 * Don't try any backup host if mail loops to myself. That would just
	 * make the problem worse.
	 */
	if (addr_list == 0 && smtp_errno == SMTP_ERR_LOOP) {
	    myfree(dest_buf);
	    break;
	}

	/*
	 * No early loop exit or we have a memory leak with dest_buf.
	 */
	if (addr_list)
	    domain_best_pref = addr_list->pref;

	/*
	 * Delete visited cached hosts from the address list.
	 * 
	 * Optionally search the connection cache by domain name or by primary
	 * MX address.
	 * 
	 * Enforce the MX session and MX address counts per next-hop or
	 * fall-back destination. smtp_reuse_session() will truncate the
	 * address list when either limit is reached.
	 */
	if (addr_list && (sess_flags & SMTP_SESS_FLAG_CACHE) != 0) {
	    if (state->cache_used->used > 0)
		smtp_scrub_addr_list(state->cache_used, &addr_list);
	    sess_count = addr_count =
		smtp_reuse_session(state, lookup_mx, domain, port,
				   &addr_list, domain_best_pref);
	} else
	    sess_count = addr_count = 0;

	/*
	 * Connect to an SMTP server.
	 * 
	 * At the start of an SMTP session, all recipients are unmarked. In the
	 * course of an SMTP session, recipients are marked as KEEP (deliver
	 * to alternate mail server) or DROP (remove from recipient list). At
	 * the end of an SMTP session, weed out the recipient list. Unmark
	 * any left-over recipients and try to deliver them to a backup mail
	 * server.
	 * 
	 * Cache the first good session under the next-hop destination name.
	 * Cache all good sessions under their physical endpoint.
	 * 
	 * Don't query the session cache for primary MX hosts. We already did
	 * that in smtp_reuse_session(), and if any were found in the cache,
	 * they were already deleted from the address list.
	 */
	for (addr = addr_list; SMTP_RCPT_LEFT(state) > 0 && addr; addr = next) {
	    next = addr->next;
	    if (++addr_count == var_smtp_mxaddr_limit)
		next = 0;
	    if ((sess_flags & SMTP_SESS_FLAG_CACHE) == 0
		|| addr->pref == domain_best_pref
		|| (session = smtp_reuse_addr(state, addr, port)) == 0)
		session = smtp_connect_addr(dest, addr, port, why, sess_flags);
	    if ((state->session = session) != 0) {
		if (++sess_count == var_smtp_mxsess_limit)
		    next = 0;
		state->final_server = (cpp[1] == 0 && next == 0);
		if (addr->pref == domain_best_pref)
		    session->features |= SMTP_FEATURE_BEST_MX;
		if ((session->features & SMTP_FEATURE_FROM_CACHE) != 0
		    || smtp_helo(state, misc_flags) == 0)
		    smtp_xfer(state);
		smtp_cleanup_session(state);
	    } else {
		msg_info("%s (port %d)", vstring_str(why), ntohs(port));
	    }
	}
	dns_rr_free(addr_list);
	myfree(dest_buf);
    }

    /*
     * We still need to deliver, bounce or defer some left-over recipients:
     * either mail loops or some backup mail server was unavailable.
     * 
     * Pay attention to what could be configuration problems, and pretend that
     * these are recoverable rather than bouncing the mail.
     */
    if (SMTP_RCPT_LEFT(state) > 0) {
	switch (smtp_errno) {

	default:
	    msg_panic("smtp_connect: bad error indication %d", smtp_errno);

	case SMTP_ERR_LOOP:
	case SMTP_ERR_FAIL:

	    /*
	     * The fall-back destination did not resolve as expected, or it
	     * is refusing to talk to us, or mail for it loops back to us.
	     */
	    if (sites->argc > 1 && cpp > sites->argv) {
		msg_warn("%s configuration problem", VAR_FALLBACK_RELAY);
		smtp_errno = SMTP_ERR_RETRY;
	    }

	    /*
	     * The next-hop relayhost did not resolve as expected, or it is
	     * refusing to talk to us, or mail for it loops back to us.
	     */
	    else if (strcmp(sites->argv[0], var_relayhost) == 0) {
		msg_warn("%s configuration problem", VAR_RELAYHOST);
		smtp_errno = SMTP_ERR_RETRY;
	    }

	    /*
	     * Mail for the next-hop destination loops back to myself. Pass
	     * the mail to the best_mx_transport or bounce it.
	     */
	    else if (smtp_errno == SMTP_ERR_LOOP && *var_bestmx_transp) {
		state->status = deliver_pass_all(MAIL_CLASS_PRIVATE,
						 var_bestmx_transp,
						 request);
		SMTP_RCPT_LEFT(state) = 0;	/* XXX */
		break;
	    }
	    /* FALLTHROUGH */

	case SMTP_ERR_RETRY:

	    /*
	     * We still need to bounce or defer some left-over recipients:
	     * either mail loops or some backup mail server was unavailable.
	     */
	    state->final_server = 1;		/* XXX */
	    smtp_site_fail(state, smtp_errno == SMTP_ERR_RETRY ? 450 : 550,
			   "%s", vstring_str(why));

	    /*
	     * Sanity check. Don't silently lose recipients.
	     */
	    smtp_rcpt_cleanup(state);
	    if (SMTP_RCPT_LEFT(state) > 0)
		msg_panic("smtp_connect: left-over recipients");
	}
    }

    /*
     * Cleanup.
     */
    if (HAVE_NEXTHOP_STATE(state))
	FREE_NEXTHOP_STATE(state);
    argv_free(sites);
    vstring_free(why);
    return (state->status);
}
