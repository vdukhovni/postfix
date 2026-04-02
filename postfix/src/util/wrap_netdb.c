/*++
/* NAME
/*	wrap_netdb 3
/* SUMMARY
/*	mockable netdb wrappers
/* SYNOPSIS
/*	#include <wrap_netdb.h>
/*
/*	int	wrap_getaddrinfo(
/*	const char *hostname,
/*	const char *servname,
/*	const struct addrinfo *hints,
/*	struct addrinfo **res)
/*
/*	void	wrap_freeaddrinfo(struct addrinfo *ai)
/*
/*	int	wrap_getnameinfo(
/*	const struct sockaddr *sa,
/*	socklen_t salen,
/*	char	*host,
/*	size_t	hostlen,
/*	char	*serv,
/*	size_t	servlen,
/*	int	flags)
/*
/*	struct servent *wrap_getservbyname(
/*	const char *name,
/*	const char *proto)
/*
/*	struct servent *wrap_getservbyport(
/*	int	port,
/*	const char *proto)
/* DESCRIPTION
/*	This module is a NOOP when the NO_MOCK_WRAPPERS macro is
/*	defined.
/*
/*	This code implements a workaround for inconsistencies in
/*	netdb.h header files, that can break test mock functions
/*	that have the same name as a system library function.
/*
/*	For example, BSD and older Linux getnameinfo() implementations
/*	use size_t for the hostlen and servlen arguments, but GLIBC
/*	2.2 and later use socklen_t instead; those two types have
/*	different sizes on LP64 systems. For a rationale why those
/*	types were changed, see
/*	https://pubs.opengroup.org/onlinepubs/9699919799/xrat/V4_xsh_chap02.html#tag_22_02_10_06
/*
/*	By default, 1) the header file of this module redirects
/*	netdb function calls by prepending a "wrap_" name prefix
/*	to netdb function names, 2) the code of this module implements
/*	functions with "wrap_" name prefixes that forward redirected
/*	calls to the real netdb functions, while taking care of any
/*	necessary type incompatibilities, and 3) test mock functions
/*	use the "wrap_" prefixed function names instead of the netdb
/*	function names. Build with -DNO_MOCK_WRAPPERS to avoid
/*	this workaround.
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

 /*
  * System library.
  */
#include <sys_defs.h>
#include <wrap_netdb.h>

#ifndef NO_MOCK_WRAPPERS

/* wrap_getaddrinfo - wrap getaddrinfo() with stable internal API */

int     wrap_getaddrinfo(const char *hostname, const char *servname,
			         const struct addrinfo *hints,
			         struct addrinfo **res)
{
#undef getaddrinfo
    return (getaddrinfo(hostname, servname, hints, res));
}

/* wrap_freeaddrinfo - wrap freeaddrinfo() with stable internal API */

void    wrap_freeaddrinfo(struct addrinfo *ai)
{
#undef freeaddrinfo
    freeaddrinfo(ai);
}

/* wrap_getnameinfo - wrap getnameinfo() with stable internal API */

int     wrap_getnameinfo(const struct sockaddr *sa, socklen_t salen,
			         char *host, size_t hostlen,
			         char *serv, size_t servlen, int flags)
{
#undef getnameinfo
    return (getnameinfo(sa, salen, host, hostlen, serv, servlen, flags));
}

/* wrap_getservbyname - wrap getservbyname() with stable internal API */

struct servent *wrap_getservbyname(const char *name, const char *proto)
{
#undef getservbyname
    return (getservbyname(name, proto));
}

/* wrap_getservbyport - wrap getservbyport() with stable internal API */

struct servent *wrap_getservbyport(int port, const char *proto)
{
#undef getservbyport
    return (getservbyport(port, proto));
}

/* wrap_setservent - wrap setservent() with stable internal API */

void    wrap_setservent(int stayopen)
{
#undef setservent
    return (setservent(stayopen));
}

/* wrap_endservent - wrap endservent() with stable internal API */

void    wrap_endservent(void)
{
#undef endservent
    return (endservent());
}

#endif
