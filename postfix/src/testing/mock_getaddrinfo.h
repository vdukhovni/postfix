#ifndef _MOCK_GETADDRINFO_H_INCLUDED_
#define _MOCK_GETADDRINFO_H_INCLUDED_

/*++
/* NAME
/*	mock_getaddrinfo 3h
/* SUMMARY
/*	getaddrinfo/getnameinfo mock for hermetic tests
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
  * Utility library.
  */
#include <myaddrinfo.h>			/* MAI_HOSTNAME_STR, etc. */

 /*
  * Test library.
  */
#include <addrinfo_to_string.h>
#include <make_addr.h>
#include <match_addr.h>
#include <match_basic.h>
#include <ptest.h>

 /*
  * Manage expectations and responses. Capture the source file name and line
  * number for better diagnostics.
  */
#define expect_getaddrinfo(exp_calls, retval, node, service, \
					hints, res) \
	_expect_getaddrinfo(__FILE__, __LINE__, (exp_calls), (retval), \
					(node), (service), \
					(hints), (res))

extern void _expect_getaddrinfo(const char *, int, int, int,
				        const char *, const char *,
				        const struct addrinfo *,
				        struct addrinfo *);

#define expect_getnameinfo(exp_calls, retval, sa, salen, \
					host, hostlen, \
					serv, servlen, flags) \
	_expect_getnameinfo(__FILE__, __LINE__, (exp_calls), (retval), \
					(sa), (salen), \
					(host), (hostlen), \
					(serv), (servlen), (flags))

extern void _expect_getnameinfo(const char *, int, int, int,
				        const struct sockaddr *, size_t,
				        const char *, size_t,
				        const char *, size_t, int);

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
