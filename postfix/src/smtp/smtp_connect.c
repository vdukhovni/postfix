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
/*	This module implements SMTP connection management and mail
/*	delivery.
/*
/*	smtp_connect() attempts to establish an SMTP session with a host
/*	that represents the destination domain, or with an optional fallback
/*	relay when the destination cannot be found, or when all the
/*	destination servers are unavailable.
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
#define INADDR_NONE 0xffffff
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
#include <debug_peer.h>
#include <mail_error.h>

/* DNS library. */

#include <dns.h>

/* Application-specific. */

#include "smtp.h"
#include "smtp_addr.h"

/* smtp_connect_addr - connect to explicit address */

static SMTP_SESSION *smtp_connect_addr(SMTP_STATE *state, DNS_RR *addr,
				               unsigned port)
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

    /*
     * Sanity checks.
     */
    if (addr->data_len > sizeof(sin.sin_addr))
	msg_panic("%s: unexpected address length %d", myname, addr->data_len);

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
	smtp_site_fail(state, 450, "connect to %s[%s] port %u: %m",
		       addr->name, inet_ntoa(sin.sin_addr), ntohs(port));
	close(sock);
	return (0);
    }

    /*
     * Skip this host if it takes no action within some time limit.
     */
    if (read_wait(sock, var_smtp_helo_tmout) < 0) {
	smtp_site_fail(state, 450, "connect to %s[%s] port %u: read timeout",
		       addr->name, inet_ntoa(sin.sin_addr), ntohs(port));
	close(sock);
	return (0);
    }

    /*
     * Skip this host if it disconnects without talking to us.
     */
    stream = vstream_fdopen(sock, O_RDWR);
    if ((ch = VSTREAM_GETC(stream)) == VSTREAM_EOF) {
	smtp_site_fail(state, 450, "connect to %s[%s] port %u: "
		       "server dropped connection without sending the initial SMTP greeting",
		       addr->name, inet_ntoa(sin.sin_addr), ntohs(port));
	vstream_fclose(stream);
	return (0);
    }
    vstream_ungetc(stream, ch);

    return (smtp_session_alloc(stream, addr->name, inet_ntoa(sin.sin_addr)));
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

/* smtp_connect - establish SMTP connection */

int     smtp_connect(SMTP_STATE *state)
{
    DELIVER_REQUEST *request = state->request;
    char   *dest_buf;
    char   *host;
    unsigned port;
    char   *def_service = "smtp";	/* XXX configurable? */
    ARGV   *sites;
    char   *dest;
    char  **cpp;
    DNS_RR *addr_list;
    DNS_RR *addr;

    /*
     * First try to deliver to the indicated destination, then try to deliver
     * to the optional fall-back relays.
     * 
     * Future proofing: do a null destination sanity check in case we allow the
     * primary destination to be a list, as it could be just a bunch of
     * separators.
     */
    sites = argv_alloc(1);
    argv_add(sites, request->nexthop, (char *) 0);
    if (sites->argc == 0)
	msg_panic("null destination: \"%s\"", request->nexthop);
    if (*var_fallback_relay)
	argv_split_append(sites, var_fallback_relay, ", \t\r\n");

    /*
     * Don't give up after any soft error until we have tried all servers.
     * 
     * Don't give up after a hard host lookup error until we have tried the
     * fallback relay servers.
     * 
     * Don't bounce mail after host lookup problems with a relayhost or with
     * fallback relays.
     * 
     * All this means that error handling and error reporting depends on whether
     * there are more mail servers (state->final_server), or whether we're
     * looking up a relayhost or fallback relay (state->backup_server).
     */
    for (cpp = sites->argv; (dest = *cpp) != 0; cpp++) {
	state->final_server = (cpp[1] == 0);
	state->backup_server =
	    (cpp > sites->argv || strcmp(request->nexthop, var_relayhost) == 0);

	/*
	 * Parse the destination. Default is to use the SMTP port. Look up
	 * the address instead of the mail exchanger when a quoted host is
	 * specified, or when DNS lookups are disabled.
	 */
	dest_buf = smtp_parse_destination(dest, def_service, &host, &port);
	if (msg_verbose)
	    msg_info("connecting to \"%s\" port \"%d\"", host, ntohs(port));
	if (var_disable_dns || *dest == '[') {
	    addr_list = smtp_host_addr(state, host);
	} else {
	    addr_list = smtp_domain_addr(state, host);
	}
	myfree(dest_buf);

	/*
	 * No address list. The mail has / has not been delivered. What to do
	 * next (skip remaining hosts / try to deliver) is recorded in the
	 * state->final_server attribute.
	 */
	if (addr_list == 0) {
	    if (state->backup_server)
		msg_warn("%s or %s configuration problem",
			 VAR_RELAYHOST, VAR_FALLBACK_RELAY);
	}

	/*
	 * Connect to an SMTP server. XXX Limit the number of addresses that
	 * we're willing to try for a non-fallback destination.
	 * 
	 * After a soft error, log deferrals and update delivery status values
	 * only when there are no further attempts.
	 */
	for (addr = addr_list; addr; addr = addr->next) {
	    if ((state->session = smtp_connect_addr(state, addr, port)) != 0) {
		state->final_server = (cpp[1] == 0 && addr->next == 0);
		state->status = 0;
		state->session->best = (addr->pref == addr_list->pref);
		debug_peer_check(state->session->host, state->session->addr);
		if (smtp_helo(state) == 0)
		    smtp_xfer(state);
		if (state->history != 0
		    && (state->error_mask & name_mask(VAR_NOTIFY_CLASSES,
				     mail_error_masks, var_notify_classes)))
		    smtp_chat_notify(state);
		/* XXX smtp_xfer() may abort in the middle of DATA. */
		smtp_session_free(state->session);
		debug_peer_restore();
		if (state->status == 0 || state->final_server)
		    break;
	    }
	}
	dns_rr_free(addr_list);

	/*
	 * In case someone has raised the "final server" flag before we have
	 * tried all fallback servers.
	 */
	if (state->final_server)
	    break;
    }

    /*
     * Cleanup.
     */
    argv_free(sites);
    return (state->status);
}
