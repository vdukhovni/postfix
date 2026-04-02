#ifndef _MAKE_ADDR_H_INCLUDED_
#define _MAKE_ADDR_H_INCLUDED_

/*++
/* NAME
/*	make_addr 3h
/* SUMMARY
/*	make_addrinfo(), freeaddrinfo(), make_sockaddr() for  hermetic tests
/* SYNOPSIS
/*	#include <mock_getaddrinfo.h>
/* DESCRIPTION
/* .nf

 /*
  * System library.
  */
#include <sys_defs.h>
#include <wrap_netdb.h>

 /*
  * External interface.
  */
extern struct addrinfo *make_addrinfo(const struct addrinfo *, const char *,
				              const char *, int);
extern struct addrinfo *copy_addrinfo(const struct addrinfo *);
extern struct sockaddr *make_sockaddr(int, const char *, int);
extern void free_sockaddr(struct sockaddr *);

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

#endif
