/*++
/* NAME
/*	biff_notify 3
/* SUMMARY
/*	send biff notification
/* SYNOPSIS
/*	#include <biff_notify.h>
/*
/*	void	biff_notify(text, len)
/*	const char *text;
/*	ssize_t	len;
/* DESCRIPTION
/*	biff_notify() sends a \fBBIFF\fR notification request to the
/*	\fBcomsat\fR daemon.
/*
/*	Arguments:
/* .IP text
/*	Null-terminated text (username@mailbox-offset).
/* .IP len
/*	Length of text, including null terminator.
/* BUGS
/*	The \fBBIFF\fR "service" can be a noticeable load for
/*	systems that have many logged-in users.
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
/*	Wietse Venema
/*	Google, Inc.
/*	111 8th Avenue
/*	New York, NY 10011, USA
/*--*/

/* System library. */

#include "sys_defs.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

/* Utility library. */

#include <msg.h>
#include <iostuff.h>
#include <myaddrinfo.h>
#include <inet_proto.h>

/* Application-specific. */

#include <biff_notify.h>

/* biff_notify - notify recipient via the biff "protocol" */

void    biff_notify(const char *text, ssize_t len)
{
    const char myname[] = "biff_notify";
    const char *hostname = "localhost";
    const char *servname = "biff";
    int     sock_type = SOCK_DGRAM;
    const INET_PROTO_INFO *proto_info;
    static struct sockaddr_storage sa;
    static SOCKADDR_SIZE sa_len;
    static int sa_family;
    static int sock = -1;
    struct addrinfo *res0, *res;
    int     aierr;
    int     found;

    /*
     * Initialize a socket address structure, or re-use an existing one.
     */
    if (sa_len == 0) {
	if ((aierr = hostname_to_sockaddr(hostname, servname, sock_type,
					  &res0)) != 0) {
	    msg_warn("lookup failed for host '%s' or service '%s': %s",
		     hostname, servname, MAI_STRERROR(aierr));
	    return;
	}
	proto_info = inet_proto_info();
	for (found = 0, res = res0; !found && res != 0; res = res->ai_next) {
	    if (strchr((char *) proto_info->sa_family_list, res->ai_family) == 0) {
		msg_info("skipping address family %d for host '%s' service '%s'",
			 res->ai_family, hostname, servname);
		continue;
	    }
	    if (res->ai_addrlen > sizeof(sa)) {
		msg_warn("skipping address size %d for host '%s' service '%s'",
			 res->ai_addrlen, hostname, servname);
		continue;
	    }
	    found++;
	    memcpy(&sa, res->ai_addr, res->ai_addrlen);
	    sa_len = res->ai_addrlen;
	    sa_family = res->ai_family;
	    if (msg_verbose) {
		MAI_HOSTADDR_STR hostaddr_str;
		MAI_SERVPORT_STR servport_str;

		SOCKADDR_TO_HOSTADDR((struct sockaddr *) &sa, sa_len,
				     &hostaddr_str, &servport_str, 0);
		msg_info("%s: sending to: {%s, %s}",
			 myname, hostaddr_str.buf, servport_str.buf);
	    }
	}
	freeaddrinfo(res0);
	if (!found)
	    return;
    }

    /*
     * Open a socket, or re-use an existing one.
     */
    if (sock < 0) {
	if ((sock = socket(sa_family, sock_type, 0)) < 0) {
	    msg_warn("socket: %m");
	    return;
	}
	close_on_exec(sock, CLOSE_ON_EXEC);
    }

    /*
     * Biff!
     */
    if (sendto(sock, text, len, 0, (struct sockaddr *) &sa, sa_len) != len)
	msg_warn("biff_notify: %m");
}
