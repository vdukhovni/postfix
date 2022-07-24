#ifndef _MOCK_MYADDRINFO_H_INCLUDED_
#define _MOCK_MYADDRINFO_H_INCLUDED_

/*++
/* NAME
/*	mock_myaddrinfo 3h
/* SUMMARY
/*	myaddrinfo mock for hermetic tests
/* SYNOPSIS
/*	#include <mock_myaddrinfo.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <myaddrinfo.h>

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
#define expect_hostname_to_sockaddr_pf(exp_calls, retval, hostname, pf, \
					service, socktype, res) \
	_expect_hostname_to_sockaddr_pf(__FILE__, __LINE__, (exp_calls), \
					(retval), (hostname), (pf), \
					(service), (socktype), (res))
#define expect_hostaddr_to_sockaddr(exp_calls, retval, hostaddr, service, \
					socktype, res) \
	_expect_hostaddr_to_sockaddr(__FILE__, __LINE__, (exp_calls), \
					(retval), \
					(hostaddr), (service), (socktype), \
					(res))
#define expect_sockaddr_to_hostaddr(exp_calls, retval, sa, salen, hostaddr, \
					portnum, socktype) \
	_expect_sockaddr_to_hostaddr(__FILE__, __LINE__, (exp_calls), \
					(retval), (sa), (salen),  (hostaddr), \
					(portnum), (socktype))
#define expect_sockaddr_to_hostname(exp_calls, retval, sa, salen, hostname, \
					service, socktype) \
	_expect_sockaddr_to_hostname(__FILE__, __LINE__, (exp_calls), \
					(retval), (sa), (salen),  (hostname), \
					(service), (socktype))

extern void _expect_hostname_to_sockaddr_pf(const char *, int, int, int,
				            const char *, int, const char *,
					            int, struct addrinfo *);
extern void _expect_hostaddr_to_sockaddr(const char *, int, int, int,
					         const char *, const char *,
					         int, struct addrinfo *);
extern void _expect_sockaddr_to_hostaddr(const char *, int, int, int,
					         const struct sockaddr *,
					         SOCKADDR_SIZE,
					         MAI_HOSTADDR_STR *,
					         MAI_SERVPORT_STR *, int);
extern void _expect_sockaddr_to_hostname(const char *, int, int, int,
					         const struct sockaddr *,
					         SOCKADDR_SIZE,
					         MAI_HOSTNAME_STR *,
					         MAI_SERVNAME_STR *, int);

#define expect_hostname_to_sockaddr(count, ret, host, serv, sock, res) \
	expect_hostname_to_sockaddr_pf((count), (ret), (host), PF_UNSPEC, \
					(serv), (sock), (res))

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
