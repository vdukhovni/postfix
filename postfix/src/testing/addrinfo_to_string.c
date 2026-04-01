/*++
/* NAME
/*      addrinfo_to_string 3
/* SUMMARY
/*	address info to string conversion
/* SYNOPSIS
/*      #include <addrinfo_to_string.h>
/*
/*      const char *pf_to_string(int);
/*
/*      const char *af_to_string(int);
/*
/*      const char *socktype_to_string(int);
/*
/*      const char *ipprotocol_to_string(int);
/*
/*      const char *ai_flags_to_string(
/*      VSTRING *buf,
/*      int     flags);
/*
/*      const char *ni_flags_to_string(
/*      VSTRING *buf,
/*      int     flags);
/*      char    *append_addrinfo_to_string(
/*      VSTRING *buf,
/*      const struct addrinfo *ai)
/*
/*      char    *addrinfo_hints_to_string(
/*      VSTRING *buf,
/*      const struct addrinfo *hints)
/*
/*      char    *sockaddr_to_string(
/*      VSTRING *buf,
/*      const struct sockaddr *sa,
/*	size_t	sa_len);
/* DESCRIPTION
/*	The functions in this module convert address information
/*	to textual form, for use in test error messages. They
/*	implement only the necessary subsets.
/*
/*      pf_to_string(), af_to_string(), socktype_to_string,
/*      ipprotocol_to_string, ai_flags_to_string(), ni_flags_to_string()
/*      produce a textual representation of addrinfo properties or
/*      getnameinfo() flags.
/*
/*      append_addrinfo_to_string() appends a textual representation
/*      of the referenced addrinfo object (only one) to the specified
/*      buffer.
/*
/*      addrinfo_hints_to_string() writes a textual representation of
/*      the referenced getaddrinfo() hints object.
/*
/*      sockaddr_to_string() writes a textual representation of the
/*      referenced sockaddr object.
/* LICENSE
/* .ad
/* .fi
/*      The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*      Wietse Venema
/*      Google, Inc.
/*      111 8th Avenue
/*      New York, NY 10011, USA
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <wrap_netdb.h>
#include <stdio.h>			/* sprintf/snprintf */

 /*
  * Utility library.
  */
#include <msg.h>
#include <myaddrinfo.h>
#include <name_code.h>
#include <name_mask.h>

 /*
  * Test library.
  */
#include <addrinfo_to_string.h>

#define STR_OR_NULL(s)          ((s) ? (s) : "(null)")

#ifdef NO_SNPRINTF
#define MY_SNPRINTF(bp, sz, fmt, ...) \
        sprintf((bp), (fmt), __VA_ARGS__)
#else
#define MY_SNPRINTF(bp, sz, fmt, ...) \
        snprintf((bp), (sz), (fmt), __VA_ARGS__)
#endif

#define STR     vstring_str

/* pf_to_string - convert protocol family to human-readable form */

const char *pf_to_string(int af)
{
    static const NAME_CODE pf_codes[] = {
	"PF_INET", PF_INET,
	"PF_INET6", PF_INET6,
	0,
    };
    const char *name;

    name = str_name_code(pf_codes, af);
    return (name ? name : "unknown-protocol-family");
}

/* af_to_string - convert address family to human-readable form */

const char *af_to_string(int af)
{
    static const NAME_CODE af_codes[] = {
	"AF_INET", AF_INET,
	"AF_INET6", AF_INET6,
	0,
    };
    const char *name;

    name = str_name_code(af_codes, af);
    return (name ? name : "unknown-address-family");
}

/* socktype_to_string - convert socket type to human-readable form */

const char *socktype_to_string(int socktype)
{
    static const NAME_CODE socktypes[] = {
	"SOCK_STREAM", SOCK_STREAM,
	"SOCK_DGRAM", SOCK_DGRAM,
	"SOCK_RAW", SOCK_RAW,
	"0", 0,
	0,
    };
    const char *name;

    name = str_name_code(socktypes, socktype);
    return (name ? name : "unknown-socket-type");
}

/* ipprotocol_to_string - convert protocol to human-readable form */

const char *ipprotocol_to_string(int proto)
{
    static const NAME_CODE protocols[] = {
	"IPPROTO_UDP", IPPROTO_UDP,
	"IPPROTO_TCP", IPPROTO_TCP,
	"0", 0,
	0,
    };
    const char *name;

    name = str_name_code(protocols, proto);
    return (name ? name : "unknown-protocol");
}

/* ai_flags_to_string - convert addrinfo flags to human-readable form */

const char *ai_flags_to_string(VSTRING *buf, int flags)
{
    static const NAME_MASK ai_flags[] = {
#ifdef AI_IDN
	"AI_IDN", AI_IDN,
#endif
#ifdef AI_CANONIDN
	"AI_CANONIDN", AI_CANONIDN,
#endif
#ifdef AI_IDN_ALLOW_UNASSIGNED
	"AI_IDN_ALLOW_UNASSIGNED", AI_IDN_ALLOW_UNASSIGNED,
#endif
#ifdef AI_IDN_USE_STD3_ASCII_RULES
	"AI_IDN_USE_STD3_ASCII_RULES", AI_IDN_USE_STD3_ASCII_RULES,
#endif
	"AI_ADDRCONFIG", AI_ADDRCONFIG,
	"AI_CANONNAME", AI_CANONNAME,
	"AI_NUMERICHOST", AI_NUMERICHOST,
	"AI_NUMERICSERV", AI_NUMERICSERV,
	"AI_PASSIVE", AI_PASSIVE,
	0,
    };

    return (str_name_mask_opt(buf, "ai_flags_to_string", ai_flags, flags,
		       NAME_MASK_NUMBER | NAME_MASK_PIPE | NAME_MASK_NULL));
}

/* ni_flags_to_string - convert getnameinfo flags to human-readable form */

const char *ni_flags_to_string(VSTRING *buf, int flags)
{
    static const NAME_MASK ni_flags[] = {
	"NI_NAMEREQD", NI_NAMEREQD,
	"NI_DGRAM", NI_DGRAM,
	"NI_NOFQDN", NI_NOFQDN,
	"NI_NUMERICHOST", NI_NUMERICHOST,
	"NI_NUMERICSERV", NI_NUMERICSERV,
	0,
    };

    return (str_name_mask_opt(buf, "ni_flags_to_string", ni_flags, flags,
		       NAME_MASK_NUMBER | NAME_MASK_PIPE | NAME_MASK_NULL));
}

/* append_addrinfo_to_string - print human-readable addrinfo */

char   *append_addrinfo_to_string(VSTRING *buf, const struct addrinfo *ai)
{
    if (ai == 0) {
	vstring_sprintf_append(buf, "(null)");
    } else {
	VSTRING *sockaddr_buf = vstring_alloc(100);
	VSTRING *flags_buf = vstring_alloc(100);

	vstring_sprintf_append(buf, "{%s, %s, %s, %s, %d, %s, %s}",
			       ai_flags_to_string(flags_buf, ai->ai_flags),
			       pf_to_string(ai->ai_family),
			       socktype_to_string(ai->ai_socktype),
			       ipprotocol_to_string(ai->ai_protocol),
			       ai->ai_addrlen,
			       sockaddr_to_string(sockaddr_buf, ai->ai_addr,
						  ai->ai_addrlen),
			       STR_OR_NULL(ai->ai_canonname));
	vstring_free(sockaddr_buf);
	vstring_free(flags_buf);
    }
    return (STR(buf));
}

/* addrinfo_hints_to_string - append human-readable hints */

char   *addrinfo_hints_to_string(VSTRING *buf, const struct addrinfo *ai)
{
    if (ai == 0) {
	vstring_sprintf(buf, "(null)");
    } else {
	VSTRING *flags_buf = vstring_alloc(100);

	vstring_sprintf(buf, "{%s, %s, %s, %s}",
			ai_flags_to_string(flags_buf, ai->ai_flags),
			pf_to_string(ai->ai_family),
			socktype_to_string(ai->ai_socktype),
			ipprotocol_to_string(ai->ai_protocol));
	vstring_free(flags_buf);
    }
    return (STR(buf));
}

/* sockaddr_to_strings - convert to printable host address and port */

static void sockaddr_to_strings(const struct sockaddr *sa, size_t salen,
				        MAI_HOSTADDR_STR *hostaddr,
				        MAI_SERVPORT_STR *portnum)
{
    if (sa->sa_family == AF_INET) {
	struct sockaddr_in *sin = (struct sockaddr_in *) sa;

	if (salen < sizeof(*sin))
	    msg_fatal("sockaddr_to_strings: bad sockaddr length %ld",
		      (long) salen);
	MY_SNPRINTF(portnum->buf, sizeof(*portnum), "%d", ntohs(sin->sin_port));
	if (inet_ntop(AF_INET, &sin->sin_addr, hostaddr->buf,
		      sizeof(*hostaddr)) == 0)
	    msg_fatal("inet_ntop(AF_INET,...): %m");
#ifdef HAS_IPV6
    } else if (sa->sa_family == AF_INET6) {
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) sa;

	if (salen < sizeof(*sin6))
	    msg_fatal("sockaddr_to_strings: bad sockaddr length %ld",
		      (long) salen);
	MY_SNPRINTF(portnum->buf, sizeof(*portnum), "%d",
		    ntohs(sin6->sin6_port));
	if (inet_ntop(AF_INET6, &sin6->sin6_addr, hostaddr->buf,
		      sizeof(*hostaddr)) == 0)
	    msg_fatal("inet_ntop(AF_INET,...): %m");
#endif
    } else {
	errno = EAFNOSUPPORT;
	msg_panic("sockaddr_to_strings: protocol familiy %d: %m",
		  sa->sa_family);
    }
}

/* sockaddr_to_string - render human-readable sockaddr */

char   *sockaddr_to_string(VSTRING *buf, const struct sockaddr *sa,
			           size_t salen)
{
    MAI_HOSTADDR_STR hostaddr;
    MAI_SERVPORT_STR portnum;

    sockaddr_to_strings(sa, salen, &hostaddr, &portnum);
    vstring_sprintf(buf, "{%s, %s, %s}",
		    af_to_string(sa->sa_family),
		    hostaddr.buf, portnum.buf);
    return (STR(buf));
}
