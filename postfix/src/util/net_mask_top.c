/*++
/* NAME
/*	net_mask_top 3
/* SUMMARY
/*	convert net/mask to printable string
/* SYNOPSIS
/*	#include <mask_addr.h>
/*
/*	char	*net_mask_top(
/*	int	family,
/*	const void *src,
/*	int	prefix_len)
/* DESCRIPTION
/*	net_mask_top() prints the network portion of the specified
/*	IPv4 or IPv6 address, null bits for the host portion, and
/*	the prefix length if it is shorter than the address.
/*	The result should be passed to myfree(). The code can
/*	handle addresses of any length, and bytes of any width.
/*
/*	Arguments:
/* .IP af
/*	The address family, as with inet_ntop().
/* .IP src
/*	Pointer to storage for an IPv4 or IPv6 address, as with
/*	inet_ntop().
/* .IP prefix_len
/*	The number of most-significant bits in \fBsrc\fR that should
/*	not be cleared.
/* DIAGNOSTICS
/*	Panic: unexpected protocol family, bad prefix length. Fatal
/*	errors: address conversion error.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

 /*
  * Utility library.
  */
#include <mask_addr.h>
#include <msg.h>
#include <myaddrinfo.h>
#include <net_mask_top.h>
#include <vstring.h>

/*
 * XXX Factor out if we also need this in other places.
 */
struct addr_size {
    int     af;				/* address family (binary) */
    char    ipproto_str[5];		/* IP protocol version (string) */
    int     addr_bitcount;		/* bits per address */
    int     addr_bytecount;		/* bytes per address */
    int     addr_strlen;		/* string representation length */
    int     slashdigs_strlen;		/* length of /0-31, /0-127 */
};
static struct addr_size addr_sizes[] = {
    AF_INET, "IPv4", MAI_V4ADDR_BITS, MAI_V4ADDR_BYTES, INET_ADDRSTRLEN, 3,
#ifdef HAS_IPV6
    AF_INET6, "IPv6", MAI_V6ADDR_BITS, MAI_V6ADDR_BYTES, INET6_ADDRSTRLEN, 4,
#endif
};

/* get_addr_size - get bit-banging numbers for address family */

static struct addr_size *get_addr_size(int af)
{
    struct addr_size *ap;

    for (ap = addr_sizes; /* see below */ ; ap++) {
	if (ap >= addr_sizes + sizeof(addr_sizes) / sizeof(struct addr_size))
	    return (0);
	if (ap->af == af)
	    return (ap);
    }
}

/* net_mask_top - printable net/mask pattern */

char   *net_mask_top(int af, const void *src, int prefix_len)
{
    const char myname[] = "net_mask_top";
    union {
	struct in_addr in_addr;
	struct in6_addr in6_addr;
    }       u;
    VSTRING *buf;
    struct addr_size *ap;

    if ((ap = get_addr_size(af)) == 0)
	msg_panic("%s: unexpected address family: %d", myname, af);
    if (prefix_len > ap->addr_bitcount || prefix_len < 0)
	msg_fatal("%s: bad %s address prefix length: %d",
		  myname, ap->ipproto_str, prefix_len);
    memcpy((void *) &u, src, ap->addr_bytecount);
    if (prefix_len < ap->addr_bitcount) {
	mask_addr((unsigned char *) &u, ap->addr_bytecount, prefix_len);
	buf = vstring_alloc(ap->addr_strlen + ap->slashdigs_strlen);
    } else {
	buf = vstring_alloc(ap->addr_strlen);
    }
    if (inet_ntop(af, &u, vstring_str(buf), vstring_avail(buf)) == 0)
	msg_fatal("%s: inet_ntop: %m", myname);
    vstring_set_payload_size(buf, strlen(vstring_str(buf)));
    if (prefix_len < ap->addr_bitcount)
	vstring_sprintf_append(buf, "/%d", prefix_len);
    return (vstring_export(buf));
}
