/*++
/* NAME
/*	normalize_mailhost_addr 3
/* SUMMARY
/*	normalize mailhost address string representation
/* SYNOPSIS
/*	#include <normalize_mailhost_addr.h>
/*
/*	int	normalize_mailhost_addr(
/*	const char *string,
/*	char	**mailhost_addr,
/*	char	**bare_addr,
/*	int	*addr_family)
/* DESCRIPTION
/*	normalize_mailhost_addr() takes the RFC 2821 string
/*	representation of an IPv4 or IPv6 network address, and
/*	normalizes the "IPv6:" prefix and numeric form. An IPv6 or
/*	IPv4 form is rejected if supposed for that protocol is
/*	disabled or non-existent. If both IPv6 and IPv4 support are
/*	enabled, a V4-in-V6 address is replaced with the IPv4 form.
/*
/*	Arguments:
/* .IP string
/*	Null-terminated string with the RFC 2821 string representation
/*	of an IPv4 or IPv6 network address.
/* .IP mailhost_addr
/*	Null pointer, or pointer to null-terminated string with the
/*	normalized RFC 2821 string representation of an IPv4 or
/*	IPv6 network address. Storage must be freed with myfree().
/* .IP bare_addr
/*	Null pointer, or pointer to null-terminated string with the
/*	numeric address without prefix, such as "IPv6:". Storage
/*	must be freed with myfree().
/* .IP addr_family
/*	Null pointer, or pointer to integer for storing the address
/*	family.
/* DIAGNOSTICS
/*	normalize_mailhost_addr() returns -1 if the input is malformed,
/*	zero otherwise.
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
#include <string.h>

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

 /*
  * Utility library.
  */
#include <inet_proto.h>
#include <msg.h>
#include <myaddrinfo.h>
#include <mymalloc.h>
#include <stringops.h>

 /*
  * Global library.
  */
#include <normalize_mailhost_addr.h>
#include <valid_mailhost_addr.h>

/* normalize_mailhost_addr - parse and normalize mailhost IP address */

int     normalize_mailhost_addr(const char *string, char **mailhost_addr,
				        char **bare_addr, int *addr_family)
{
    const char myname[] = "normalize_mailhost_addr";
    const INET_PROTO_INFO *proto_info = inet_proto_info();
    struct addrinfo *res = 0;
    MAI_HOSTADDR_STR hostaddr;
    const char *valid_addr;		/* IPv6:fc00::1 */
    const char *normal_addr;		/* 192.168.0.1 */
    int     normal_family;

#define UPDATE_BARE_ADDR(s, v) do { \
	if (s) myfree(s); \
	(s) = mystrdup(v); \
    } while(0)
#define UPDATE_MAILHOST_ADDR(s, prefix, addr) do { \
	if (s) myfree(s); \
	(s) = concatenate((prefix), (addr), (char *) 0); \
    } while (0)

    /*
     * Parse and normalize the input.
     */
    if ((valid_addr = valid_mailhost_addr(string, DONT_GRIPE)) == 0
	|| hostaddr_to_sockaddr(valid_addr, (char *) 0, 0, &res) != 0
	|| sockaddr_to_hostaddr(res->ai_addr, res->ai_addrlen,
				&hostaddr, (MAI_SERVPORT_STR *) 0, 0) != 0) {
	normal_addr = 0;
#ifdef HAS_IPV6
    } else if (res->ai_family == AF_INET6
	       && strncasecmp("::ffff:", hostaddr.buf, 7) == 0
	       && strchr((char *) proto_info->sa_family_list, AF_INET)) {
	normal_addr = hostaddr.buf + 7;
	normal_family = AF_INET;
#endif
    } else if (strchr((char *) proto_info->sa_family_list, res->ai_family)) {
	normal_addr = hostaddr.buf;
	normal_family = res->ai_family;
    } else {
	normal_addr = 0;
    }
    if (res)
	freeaddrinfo(res);
    if (normal_addr == 0)
	return (-1);

    /*
     * Write the applicable outputs.
     */
    if (bare_addr) {
	UPDATE_BARE_ADDR(*bare_addr, normal_addr);
	if (msg_verbose)
	    msg_info("%s: bare_addr=%s", myname, *bare_addr);
    }
    if (mailhost_addr) {
#ifdef HAS_IPV6
	if (normal_family == AF_INET6)
	    UPDATE_MAILHOST_ADDR(*mailhost_addr, IPV6_COL, normal_addr);
	else
#endif
	    UPDATE_BARE_ADDR(*mailhost_addr, normal_addr);
	if (msg_verbose)
	    msg_info("%s: mailhost_addr=%s", myname, *mailhost_addr);
    }
    if (addr_family) {
	*addr_family = normal_family;
	if (msg_verbose)
	    msg_info("%s: addr_family=%s", myname,
		     *addr_family == AF_INET6 ? "AF_INET6"
		     : *addr_family == AF_INET ? "AF_INET"
		     : "unknown");
    }
    return (0);
}
