/*++
/* NAME
/*	lmtp_connect 3
/* SUMMARY
/*	connect to LMTP server
/* SYNOPSIS
/*	#include "lmtp.h"
/*
/*	LMTP_SESSION *lmtp_connect(destination, why)
/*	char	*destination;
/*	VSTRING	*why;
/* DESCRIPTION
/*	This module implements LMTP connection management.
/*
/*	lmtp_connect() attempts to establish an LMTP session with a host.
/*
/*	The destination is either a host name or a numeric address.
/*	Symbolic or numeric service port information may be appended,
/*	separated by a colon (":").
/*
/*	Numerical address information should always be quoted with `[]'.
/*
/* DIAGNOSTICS
/*	This routine either returns an LMTP_SESSION pointer, or
/*	returns a null pointer and set the \fIlmtp_errno\fR
/*	global variable accordingly:
/* .IP LMTP_RETRY
/*	The connection attempt failed, but should be retried later.
/* .IP LMTP_FAIL
/*	The connection attempt failed.
/* .PP
/*	In addition, a textual description of the error is made available
/*	via the \fIwhy\fR argument.
/* SEE ALSO
/*	lmtp_proto(3) LMTP client protocol
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
/*      Alterations for LMTP by:
/*      Philip A. Prindeville
/*      Mirapoint, Inc.
/*      USA.
/*
/*      Additional work on LMTP by:
/*      Amos Gouaux
/*      University of Texas at Dallas
/*      P.O. Box 830688, MC34
/*      Richardson, TX 75083, USA
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

/* Global library. */

#include <mail_params.h>
#include <own_inet_addr.h>

/* DNS library. */

#include <dns.h>

/* Application-specific. */

#include "lmtp.h"
#include "lmtp_addr.h"

/* lmtp_connect_addr - connect to explicit address */

static LMTP_SESSION *lmtp_connect_addr(DNS_RR *addr, unsigned port,
				               VSTRING *why)
{
    char   *myname = "lmtp_connect_addr";
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
    if (addr->data_len > sizeof(sin.sin_addr)) {
	msg_warn("%s: skip address with length %d", myname, addr->data_len);
	lmtp_errno = LMTP_RETRY;
	return (0);
    }

    /*
     * Initialize.
     */
    memset((char *) &sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;

    if ((sock = socket(sin.sin_family, SOCK_STREAM, 0)) < 0)
	msg_fatal("%s: socket: %m", myname);

    /* do we still need this if? */
    addr_list = own_inet_addr_list();
    if (addr_list->used == 1) {
	sin.sin_port = 0;
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
     * Connect to the LMTP server.
     */
    sin.sin_port = port;
    memcpy((char *) &sin.sin_addr, addr->data, sizeof(sin.sin_addr));

    if (msg_verbose)
	msg_info("%s: trying: %s[%s] port %d...",
		 myname, addr->name, inet_ntoa(sin.sin_addr), ntohs(port));
    if (var_lmtp_conn_tmout > 0) {
	non_blocking(sock, NON_BLOCKING);
	conn_stat = timed_connect(sock, (struct sockaddr *) & sin,
				  sizeof(sin), var_lmtp_conn_tmout);
	saved_errno = errno;
	non_blocking(sock, BLOCKING);
	errno = saved_errno;
    } else {
	conn_stat = connect(sock, (struct sockaddr *) & sin, sizeof(sin));
    }
    if (conn_stat < 0) {
	vstring_sprintf(why, "connect to %s[%s]: %m",
			addr->name, inet_ntoa(sin.sin_addr));
	lmtp_errno = LMTP_RETRY;
	close(sock);
	return (0);
    }

    /*
     * Skip this host if it takes no action within some time limit.
     */
    if (read_wait(sock, var_lmtp_lhlo_tmout) < 0) {
	vstring_sprintf(why, "connect to %s[%s]: read timeout",
			addr->name, inet_ntoa(sin.sin_addr));
	lmtp_errno = LMTP_RETRY;
	close(sock);
	return (0);
    }

    /*
     * Skip this host if it disconnects without talking to us.
     */
    stream = vstream_fdopen(sock, O_RDWR);
    if ((ch = VSTREAM_GETC(stream)) == VSTREAM_EOF) {
	vstring_sprintf(why, "connect to %s[%s]: server dropped connection",
			addr->name, inet_ntoa(sin.sin_addr));
	lmtp_errno = LMTP_RETRY;
	vstream_fclose(stream);
	return (0);
    }

    /*
     * Skip this host if it sends a 4xx greeting.
     */
    if (ch == '4') {
	vstring_sprintf(why, "connect to %s[%s]: server refused mail service",
			addr->name, inet_ntoa(sin.sin_addr));
	lmtp_errno = LMTP_RETRY;
	vstream_fclose(stream);
	return (0);
    }
    vstream_ungetc(stream, ch);
    return (lmtp_session_alloc(stream, addr->name, inet_ntoa(sin.sin_addr)));
}

/* lmtp_connect_host - direct connection to host */

LMTP_SESSION *lmtp_connect_host(char *host, unsigned port, VSTRING *why)
{
    LMTP_SESSION *session = 0;
    DNS_RR *addr_list;
    DNS_RR *addr;

    /*
     * Try each address in the specified order until we find one that works.
     * The addresses belong to the same A record, so we have no information
     * on what address is "best".
     */
    addr_list = lmtp_host_addr(host, why);
    for (addr = addr_list; addr; addr = addr->next) {
	if ((session = lmtp_connect_addr(addr, port, why)) != 0) {
	    break;
	}
    }
    dns_rr_free(addr_list);
    return (session);
}

/* lmtp_parse_destination - parse destination */

static char *lmtp_parse_destination(char *destination, char *def_service,
				            char **hostp, unsigned *portp)
{
    char   *myname = "lmtp_parse_destination";
    char   *buf = mystrdup(destination);
    char   *host = buf;
    char   *service;
    struct servent *sp;
    char   *protocol = "tcp";		/* XXX configurable? */
    unsigned port;

    if (msg_verbose)
	msg_info("%s: %s %s", myname, destination, def_service);

    /*
     * Strip quoting. We're working with a copy of the destination argument
     * so the stripping can be destructive.
     */
    if (*host == '[') {
	host++;
	host[strcspn(host, "]")] = 0;
    }

    /*
     * Separate host and service information, or use the default service
     * specified by the caller. XXX the ":" character is used in the IPV6
     * address notation, so using split_at_right() is not sufficient. We'd
     * have to count the number of ":" instances.
     */
    if ((service = split_at_right(host, ':')) == 0)
	service = def_service;
    if (*service == 0)
	msg_fatal("%s: empty service name: %s", myname, destination);
    *hostp = host;

    /*
     * Convert service to port number, network byte order.
     */
    if ((port = atoi(service)) != 0) {
	*portp = htons(port);
    } else {
        /* 
         * Since most folks aren't going to have lmtp defined as a service,
         * use a default value instead of just blowing up.
         */
	if ((sp = getservbyname(service, protocol)) == 0)
            *portp = htons(var_lmtp_tcp_port);
        else 
            *portp = sp->s_port;
    }
    return (buf);
}

/* lmtp_connect_local - local connect to unix domain socket */

LMTP_SESSION *lmtp_connect_local(const char *class, const char *name, VSTRING *why)
{
    char   *myname = "lmtp_connect_local";
    VSTREAM *stream;
    int     ch;

    /*
     * Connect to the LMTP server.
     */
    if (msg_verbose)
	msg_info("%s: trying: %s/%s...", myname, class, name);
    if ((stream = mail_connect_wait(class, name)) == 0) {
        vstring_sprintf(why, "connect to %s: connection failed.", name);
        lmtp_errno = LMTP_RETRY;
        return (0);
    }

    /*
     * Skip this process if it takes no action within some time limit.
     */
    if (read_wait(vstream_fileno(stream), var_lmtp_lhlo_tmout) < 0) {
        vstring_sprintf(why, "connect to %s: read timeout", name);
        lmtp_errno = LMTP_RETRY;
        vstream_fclose(stream);
        return (0);
    }

    /*
     * Skip this process if it disconnects without talking to us.
     */
    if ((ch = VSTREAM_GETC(stream)) == VSTREAM_EOF) {
	vstring_sprintf(why, "connect to %s: server dropped connection", name);
	lmtp_errno = LMTP_RETRY;
	vstream_fclose(stream);
	return (0);
    }

    /*
     * Skip this host if it sends a 4xx greeting.
     */
    if (ch == '4') {
	vstring_sprintf(why, "connect to %s: server refused mail service", name);
	lmtp_errno = LMTP_RETRY;
	vstream_fclose(stream);
	return (0);
    }
    vstream_ungetc(stream, ch);
    return (lmtp_session_alloc(stream, name, ""));
}

/* lmtp_connect - establish LMTP connection */

LMTP_SESSION *lmtp_connect(LMTP_ATTR *attr, DELIVER_REQUEST *request, VSTRING *why)
{
    char   *myname = "lmtp_connect";
    LMTP_SESSION *session;
    char   *dest_buf;
    char   *host;
    unsigned port;
    char   *def_service = "lmtp";	/* XXX configurable? */

    /*
     * Are we connecting to a local or inet socket?
     */
    if (attr->type == LMTP_SERV_TYPE_UNIX) {
        /*
         * Connect to local LMTP server.
         */
        if (msg_verbose)
            msg_info("%s: connecting to %s", myname, attr->name);
        session = lmtp_connect_local(attr->class, attr->name, why);
        if (session != 0) {
            session->destination = mystrdup(attr->name);
            session->type = attr->type;
        }
    } else {
        /*
         * Connect to LMTP server via inet socket, but where?
         */
        if (!*(attr)->name) {
            if (msg_verbose)
                msg_info("%s: attr->name not set; using request->nexthop", myname);
            attr->name = request->nexthop;
        }
        dest_buf = lmtp_parse_destination(attr->name, def_service, 
                                          &host, &port);
        
        /*
         * Now that the inet LMTP server has been determined, connect to it.
         */
        if (msg_verbose)
            msg_info("%s: connecting to %s port %d", myname, host, ntohs(port));
        session = lmtp_connect_host(host, port, why);
        if (session != 0) {
            session->destination = mystrdup(attr->name);
            session->type = attr->type;
        }
        myfree(dest_buf);
    }
    return (session);
}

