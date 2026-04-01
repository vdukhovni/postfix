#ifndef _WRAP_NETDB_H_INCLUDED_
#define _WRAP_NETDB_H_INCLUDED_

/*++
/* NAME
/*	wrap_netdb 3h
/* SUMMARY
/*	mockable netdb wrappers
/* SYNOPSIS
/*	#include <wrap_netdb.h>
/* DESCRIPTION
/* .nf

 /*
  * System library
  */
#include <sys_defs.h>
#include <netdb.h>

 /*
  * External interface.
  */
#ifndef NO_MOCK_WRAPPERS
extern int wrap_getaddrinfo(const char *, const char *,
				             const struct addrinfo *,
				             struct addrinfo **);
extern void wrap_freeaddrinfo(struct addrinfo *);
extern int wrap_getnameinfo(const struct sockaddr *, socklen_t, char *,
				             size_t, char *, size_t, int);

#define	getaddrinfo     wrap_getaddrinfo
#define	freeaddrinfo    wrap_freeaddrinfo
#define getnameinfo     wrap_getnameinfo

extern struct servent *wrap_getservbyname(const char *, const char *);
extern struct servent *wrap_getservbyport(int, const char *);
extern void wrap_setservent(int);
extern void wrap_endservent(void);

#define	getservbyname   wrap_getservbyname
#define getservbyport   wrap_getservbyport
#define setservent	wrap_setservent
#define endservent	wrap_endservent
#endif

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
