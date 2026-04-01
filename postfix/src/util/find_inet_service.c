/*++
/* NAME
/*	find_inet_service 3
/* SUMMARY
/*	TCP service lookup
/* SYNOPSIS
/*	#include <find_inet_service.h>
/*
/*	int	find_inet_service(service, proto)
/*	const char *service;
/*	const char *proto;
/* DESCRIPTION
/*	find_inet_service() looks up the numerical TCP/IP port (in
/*	host byte order) for the specified service. If the service
/*	is in numerical form, then that is converted instead.
/*
/*	TCP services are mapped with known_tcp_ports(3).
/* DIAGNOSTICS
/*	find_inet_service() returns -1 when the service is not
/*	found, or when a numerical service is out of range.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	Google, Inc.
/*	111 8th Avenue
/*	New York, NY 10011, USA
/*--*/

/* System libraries. */

#include <sys_defs.h>
#include <wrap_netdb.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Application-specific. */

#include <find_inet_service.h>
#include <known_tcp_ports.h>
#include <msg.h>
#include <sane_strtol.h>
#include <stringops.h>

/* find_inet_service - translate numerical or symbolic service name */

int     find_inet_service(const char *service, const char *protocol)
{
    struct servent *sp;
    unsigned long lport;
    char   *cp;

    if (strcmp(protocol, "tcp") == 0)
	service = filter_known_tcp_port(service);
    if (alldig(service)) {
	lport = sane_strtoul(service, &cp, 10);
	if (*cp != '\0' || errno == ERANGE || lport > 65535)
	    return (-1);			/* bad numerical service */
	return ((int) lport);
    } else {
	if ((sp = getservbyname(service, protocol)) == 0)
	    return (-1);			/* bad symbolic service */
	return (ntohs(sp->s_port));
    }
}
