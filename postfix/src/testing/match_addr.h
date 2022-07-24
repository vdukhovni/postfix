#ifndef _MATCH_ADDR_H_INCLUDED_
#define _MATCH_ADDR_H_INCLUDED_

/*++
/* NAME
/*	match_addr 3h
/* SUMMARY
/*	network address matcher
/* SYNOPSIS
/*	#include <match_addr.h>
/* DESCRIPTION
/* .nf

 /*
  * System library.
  */
#include <sys_defs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <wrap_netdb.h>

 /*
  * Test library.
  */
#include <ptest.h>

 /*
  * Matchers.
  */
#define eq_addrinfo(t, what, got, want) \
	_eq_addrinfo(__FILE__, __LINE__, (t), (what), (got), (want))

extern int _eq_addrinfo(const char *, int, PTEST_CTX *, const char *,
			        const struct addrinfo *,
			        const struct addrinfo *);

#define eq_sockaddr(t, what, got, gotlen, want, wantlen) \
	_eq_sockaddr(__FILE__, __LINE__, (t), (what), (got), (gotlen), (want), (wantlen))

extern int _eq_sockaddr(const char *, int, PTEST_CTX *, const char *,
			        const struct sockaddr *, size_t,
			        const struct sockaddr *, size_t);

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
