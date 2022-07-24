/*++
/* NAME
/*	make_addr 3
/* SUMMARY
/*	make_addrinfo(), freeaddrinfo(), make_sockaddr() for hermetic tests
/* SYNOPSIS
/*	#include <make_addrinfo.h>
/*
/*	struct addrinfo *make_addrinfo(
/*	const struct addrinfo *hints,
/*	const char *name,
/*	const char *addr,
/*	int	port)
/*
/*	struct addrinfo *copy_addrinfo(const struct addrinfo *ai)
/*
/*	void	freeaddrinfo(struct addrinfo *ai)
/*
/*	struct sockaddr *make_sockaddr(
/*	int	family,
/*	const char *addr,
/*	int	port)
/*
/*	void	free_sockaddr(struct sockaddr *sa)
/* DESCRIPTION
/*	This module contains helper functions to set up mock
/*	expectations.
/*
/*	make_addrinfo() creates one addrinfo structure. The hints
/*	argument must specify the protocol family for the addr
/*	argument (i.e. not PF_UNSPEC). To create a linked list,
/*	manually append make_addrinfo() results. The port is in
/*	host byte order.
/*
/*	copy_addrinfo() makes a deep copy of a linked list of
/*	addrinfo structures.
/*
/*	freeaddrinfo() deletes a linked list of addrinfo structures.
/*	This function must be used for addrinfo structures created
/*	with make_addrinfo() and copy_addrinfo().
/*
/*	make_sockaddr() creates a sockaddr structure from the string
/*	representation of an IP address, and a port in host byte
/*	order.
/*
/*	free_sockaddr() exists to make program code more explicit.
/* DIAGNOSTICS
/*	make_sockaddr() and make_addrinfo() terminate with a fatal
/*	error when an unknown address family is specified, or when
/*	the string representation of an IP address does not match
/*	the address family.
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <wrap_netdb.h>
#include <string.h>
#include <errno.h>

 /*
  * Utility library.
  */
#include <msg.h>
#include <mymalloc.h>

 /*
  * Test library.
  */
#include <make_addr.h>

#define MYSTRDUP_OR_NULL(x)	((x) ? mystrdup(x) : 0)

/* make_sockaddr - helper to create mock sockaddr instances */

struct sockaddr *make_sockaddr(int family, const char *addr, int port)
{
    if (family == AF_INET) {
	struct sockaddr_in *sa;

	sa = (struct sockaddr_in *) mymalloc(sizeof(*sa));
	memset(sa, 0, sizeof(*sa));
	switch (inet_pton(AF_INET, addr, (void *) &sa->sin_addr)) {
	case 1:
	    break;
	case 0:
	    msg_fatal("bad address syntax: '%s'", addr);
	case -1:
	    msg_fatal("inet_pton(AF_INET, %s, (ptr)): %m", addr);
	}
	sa->sin_family = AF_INET;
	sa->sin_port = htons(port);
	return ((struct sockaddr *) sa);
#ifdef HAS_IPV6
    } else if (family == AF_INET6) {
	struct sockaddr_in6 *sa;

	sa = (struct sockaddr_in6 *) mymalloc(sizeof(*sa));
	memset(sa, 0, sizeof(*sa));
	switch (inet_pton(AF_INET6, addr, (void *) &sa->sin6_addr)) {
	case 1:
	    break;
	case 0:
	    msg_fatal("bad address syntax: '%s'", addr);
	case -1:
	    msg_fatal("inet_pton(AF_INET6, %s, (ptr)): %m", addr);
	}
	sa->sin6_family = AF_INET6;
	sa->sin6_port = htons(port);
	return ((struct sockaddr *) sa);
#endif
    } else {
	errno = EAFNOSUPPORT;
	msg_panic("make_sockaddr: address familiy %d: %m", family);
    }
}

/* free_sockaddr - destructor for make_sockaddr()  and copy_addrinfo() */

void    free_sockaddr(struct sockaddr *sa)
{
    myfree(sa);
}

/* copy_addrinfo - expectation helper */

struct addrinfo *copy_addrinfo(const struct addrinfo *in)
{
    struct addrinfo *out;

    /*
     * First a shallow copy, followed by deep copies for pointer fields.
     */
    if (in == 0)
	return (0);
    out = (struct addrinfo *) mymalloc(sizeof(*out));
    *out = *in;
    if (in->ai_addr != 0) {
	out->ai_addr = (struct sockaddr *) mymalloc(in->ai_addrlen);
	memcpy(out->ai_addr, in->ai_addr, in->ai_addrlen);
    }
    if (in->ai_canonname != 0)
	out->ai_canonname = mystrdup(in->ai_canonname);
    if (in->ai_next)
	out->ai_next = copy_addrinfo(in->ai_next);
    return (out);
}

/* make_addrinfo - helper to create mock addrinfo input or result */

struct addrinfo *make_addrinfo(const struct addrinfo *hints,
			               const char *name, const char *addr,
			               int port)
{
    struct addrinfo *out;

    out = (struct addrinfo *) mymalloc(sizeof(*out));
    memset(out, 0, sizeof(*out));
    out->ai_canonname = MYSTRDUP_OR_NULL(name);
    switch (hints->ai_family) {
    case PF_INET6:
	out->ai_addr = make_sockaddr(AF_INET6, addr, port);
	out->ai_addrlen = sizeof(struct sockaddr_in6);
	out->ai_family = hints->ai_family;
	out->ai_socktype = hints->ai_socktype;
	out->ai_protocol = hints->ai_protocol;
	break;
    case PF_INET:
	out->ai_addr = make_sockaddr(AF_INET, addr, port);
	out->ai_addrlen = sizeof(struct sockaddr_in);
	out->ai_family = hints->ai_family;
	out->ai_socktype = hints->ai_socktype;
	out->ai_protocol = hints->ai_protocol;
	break;
    default:
	msg_fatal("make_addrinfo: hints->ai_family: %d", hints->ai_family);
    }
    out->ai_next = 0;
    return (out);
}

/* freeaddrinfo - free the mock-generated addrinfo structures */

void    freeaddrinfo(struct addrinfo *res)
{
    if (res) {
	if (res->ai_next)
	    freeaddrinfo(res->ai_next);
	if (res->ai_addr)
	    myfree(res->ai_addr);
	if (res->ai_canonname)
	    myfree(res->ai_canonname);
	myfree(res);
    }
}
