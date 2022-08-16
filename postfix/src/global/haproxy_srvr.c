/*++
/* NAME
/*	haproxy_srvr 3
/* SUMMARY
/*	server-side haproxy protocol support
/* SYNOPSIS
/*	#include <haproxy_srvr.h>
/*
/*	const char *haproxy_srvr_parse(str, str_len, non_proxy,
/*			smtp_client_addr, smtp_client_port,
/*			smtp_server_addr, smtp_server_port)
/*	const char *str;
/*	ssize_t	*str_len;
/*	int	*non_proxy;
/*	MAI_HOSTADDR_STR *smtp_client_addr,
/*	MAI_SERVPORT_STR *smtp_client_port,
/*	MAI_HOSTADDR_STR *smtp_server_addr,
/*	MAI_SERVPORT_STR *smtp_server_port;
/*
/*	const char *haproxy_srvr_receive(fd, non_proxy,
/*			smtp_client_addr, smtp_client_port,
/*			smtp_server_addr, smtp_server_port)
/*	int	fd;
/*	int	*non_proxy;
/*	MAI_HOSTADDR_STR *smtp_client_addr,
/*	MAI_SERVPORT_STR *smtp_client_port,
/*	MAI_HOSTADDR_STR *smtp_server_addr,
/*	MAI_SERVPORT_STR *smtp_server_port;
/* DESCRIPTION
/*	haproxy_srvr_parse() parses a haproxy v1 or v2 protocol
/*	message. The result is null in case of success, a pointer
/*	to text (with the error type) in case of error. If both
/*	IPv6 and IPv4 support are enabled, IPV4_IN_IPV6 address
/*	form (::ffff:1.2.3.4) is converted to IPV4 form. In case
/*	of success, the str_len argument is updated with the number
/*	of bytes parsed, and the non_proxy argument is true or false
/*	if the haproxy message specifies a non-proxied connection.
/*
/*	haproxy_srvr_receive() receives and parses a haproxy protocol
/*	handshake. This must be called before any I/O is done on
/*	the specified file descriptor. The result is 0 in case of
/*	success, -1 in case of error. All errors are logged.
/*
/*	The haproxy v2 protocol support is limited to TCP over IPv4,
/*	TCP over IPv6, and non-proxied connections. In the latter
/*	case, the caller is responsible for any local or remote
/*	address/port lookup.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*
/*	Wietse Venema
/*	Google, Inc.
/*	111 8th Avenue
/*	New York, NY 10011, USA
/*--*/

/* System library. */

#include <sys_defs.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

/* Utility library. */

#include <msg.h>
#include <myaddrinfo.h>
#include <valid_hostname.h>
#include <stringops.h>
#include <mymalloc.h>
#include <inet_proto.h>
#include <split_at.h>
#include <sock_addr.h>

/* Global library. */

#define HAPROXY_SRVR_INTERNAL
#include <haproxy_srvr.h>

/* Application-specific. */

 /*
  * The haproxy protocol assumes that a haproxy header will normally not
  * exceed the default IPv4 TCP MSS, i.e. 576-40=536 bytes (the IPv6 default
  * is larger: 1280-60=1220). With a proxy header that contains IPv6
  * addresses, that leaves room for 536-52=484 bytes of TLVs. The Postfix
  * implementation does not support headers with UNIX-domain addresses.
  */
#define HAPROXY_HEADER_MAX_LEN		536

static const INET_PROTO_INFO *proto_info;

#define STR_OR_NULL(str) ((str) ? (str) : "(null)")

/* haproxy_srvr_parse_lit - extract and validate string literal */

static int haproxy_srvr_parse_lit(const char *str,...)
{
    va_list ap;
    const char *cp;
    int     result = -1;
    int     count;

    if (msg_verbose)
	msg_info("haproxy_srvr_parse: %s", STR_OR_NULL(str));

    if (str != 0) {
	va_start(ap, str);
	for (count = 0; (cp = va_arg(ap, const char *)) != 0; count++) {
	    if (strcmp(str, cp) == 0) {
		result = count;
		break;
	    }
	}
	va_end(ap);
    }
    return (result);
}

/* haproxy_srvr_parse_proto - parse and validate the protocol type */

static int haproxy_srvr_parse_proto(const char *str, int *addr_family)
{
    if (msg_verbose)
	msg_info("haproxy_srvr_parse: proto=%s", STR_OR_NULL(str));

    if (str == 0)
	return (-1);
#ifdef AF_INET6
    if (strcasecmp(str, "TCP6") == 0) {
	if (strchr((char *) proto_info->sa_family_list, AF_INET6) != 0) {
	    *addr_family = AF_INET6;
	    return (0);
	}
    } else
#endif
    if (strcasecmp(str, "TCP4") == 0) {
	if (strchr((char *) proto_info->sa_family_list, AF_INET) != 0) {
	    *addr_family = AF_INET;
	    return (0);
	}
    }
    return (-1);
}

/* haproxy_srvr_parse_addr - extract and validate IP address */

static int haproxy_srvr_parse_addr(const char *str, MAI_HOSTADDR_STR *addr,
				           int addr_family)
{
    struct addrinfo *res = 0;
    int     err;

    if (msg_verbose)
	msg_info("haproxy_srvr_parse: addr=%s proto=%d",
		 STR_OR_NULL(str), addr_family);

    if (str == 0 || strlen(str) >= sizeof(MAI_HOSTADDR_STR))
	return (-1);

    switch (addr_family) {
#ifdef AF_INET6
    case AF_INET6:
	err = !valid_ipv6_hostaddr(str, DONT_GRIPE);
	break;
#endif
    case AF_INET:
	err = !valid_ipv4_hostaddr(str, DONT_GRIPE);
	break;
    default:
	msg_panic("haproxy_srvr_parse: unexpected address family: %d",
		  addr_family);
    }
    if (err == 0)
	err = (hostaddr_to_sockaddr(str, (char *) 0, 0, &res)
	       || sockaddr_to_hostaddr(res->ai_addr, res->ai_addrlen,
				       addr, (MAI_SERVPORT_STR *) 0, 0));
    if (res)
	freeaddrinfo(res);
    if (err)
	return (-1);
    if (addr->buf[0] == ':' && strncasecmp("::ffff:", addr->buf, 7) == 0
	&& strchr((char *) proto_info->sa_family_list, AF_INET) != 0)
	memmove(addr->buf, addr->buf + 7, strlen(addr->buf) + 1 - 7);
    return (0);
}

/* haproxy_srvr_parse_port - extract and validate TCP port */

static int haproxy_srvr_parse_port(const char *str, MAI_SERVPORT_STR *port)
{
    if (msg_verbose)
	msg_info("haproxy_srvr_parse: port=%s", STR_OR_NULL(str));
    if (str == 0 || strlen(str) >= sizeof(MAI_SERVPORT_STR)
	|| !valid_hostport(str, DONT_GRIPE)) {
	return (-1);
    } else {
	memcpy(port->buf, str, strlen(str) + 1);
	return (0);
    }
}

/* haproxy_srvr_parse_v2_addr_v4 - parse IPv4 info from v2 header */

static int haproxy_srvr_parse_v2_addr_v4(uint32_t sin_addr,
					         unsigned sin_port,
					         MAI_HOSTADDR_STR *addr,
					         MAI_SERVPORT_STR *port)
{
    struct sockaddr_in sin;

    memset((void *) &sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = sin_addr;
    sin.sin_port = sin_port;
    if (sockaddr_to_hostaddr((struct sockaddr *) &sin, sizeof(sin),
			     addr, port, 0) < 0)
	return (-1);
    return (0);
}

#ifdef AF_INET6

/* haproxy_srvr_parse_v2_addr_v6 - parse IPv6 info from v2 header */

static int haproxy_srvr_parse_v2_addr_v6(uint8_t *sin6_addr,
					         unsigned sin6_port,
					         MAI_HOSTADDR_STR *addr,
					         MAI_SERVPORT_STR *port)
{
    struct sockaddr_in6 sin6;

    memset((void *) &sin6, 0, sizeof(sin6));
    sin6.sin6_family = AF_INET6;
    memcpy(&sin6.sin6_addr, sin6_addr, 16);
    sin6.sin6_port = sin6_port;
    if (sockaddr_to_hostaddr((struct sockaddr *) &sin6,
			     sizeof(sin6), addr, port, 0) < 0)
	return (-1);
    if (addr->buf[0] == ':'
	&& strncasecmp("::ffff:", addr->buf, 7) == 0
	&& strchr((char *) proto_info->sa_family_list, AF_INET) != 0)
	memmove(addr->buf, addr->buf + 7,
		strlen(addr->buf) + 1 - 7);
    return (0);
}

#endif

/* haproxy_srvr_parse_v2_hdr - parse v2 header address info */

static const char *haproxy_srvr_parse_v2_hdr(const char *str, ssize_t *str_len,
					             int *non_proxy,
				         MAI_HOSTADDR_STR *smtp_client_addr,
				         MAI_SERVPORT_STR *smtp_client_port,
				         MAI_HOSTADDR_STR *smtp_server_addr,
				         MAI_SERVPORT_STR *smtp_server_port)
{
    const char myname[] = "haproxy_srvr_parse_v2_hdr";
    struct proxy_hdr_v2 *hdr_v2;

    if (*str_len < PP2_HEADER_LEN)
	return ("short protocol header");
    hdr_v2 = (struct proxy_hdr_v2 *) str;
    if (memcmp(hdr_v2->sig, PP2_SIGNATURE, PP2_SIGNATURE_LEN) != 0)
	return ("unrecognized protocol header");
    if ((hdr_v2->ver_cmd & PP2_VERSION_MASK) != PP2_VERSION)
	return ("unrecognized protocol version");
    if (*str_len < PP2_HEADER_LEN + ntohs(hdr_v2->len))
	return ("short version 2 protocol header");

    switch (hdr_v2->ver_cmd & PP2_CMD_MASK) {

	/*
	 * Proxied connection, use the proxy-provided connection info.
	 */
    case PP2_CMD_PROXY:
	switch (hdr_v2->fam) {
	case PP2_FAM_INET | PP2_TRANS_STREAM:{	/* TCP4 */
		if (strchr((char *) proto_info->sa_family_list, AF_INET) == 0)
		    return ("Postfix IPv4 support is disabled");
		if (ntohs(hdr_v2->len) < PP2_ADDR_LEN_INET)
		    return ("short address field");
		if (haproxy_srvr_parse_v2_addr_v4(hdr_v2->addr.ip4.src_addr,
						  hdr_v2->addr.ip4.src_port,
				    smtp_client_addr, smtp_client_port) < 0)
		    return ("client network address conversion error");
		if (msg_verbose)
		    msg_info("%s: smtp_client_addr=%s smtp_client_port=%s",
		      myname, smtp_client_addr->buf, smtp_client_port->buf);
		if (haproxy_srvr_parse_v2_addr_v4(hdr_v2->addr.ip4.dst_addr,
						  hdr_v2->addr.ip4.dst_port,
				    smtp_server_addr, smtp_server_port) < 0)
		    return ("server network address conversion error");
		if (msg_verbose)
		    msg_info("%s: smtp_server_addr=%s smtp_server_port=%s",
		      myname, smtp_server_addr->buf, smtp_server_port->buf);
		break;
	    }
	case PP2_FAM_INET6 | PP2_TRANS_STREAM:{/* TCP6 */
#ifdef AF_INET6
		if (strchr((char *) proto_info->sa_family_list, AF_INET6) == 0)
		    return ("Postfix IPv6 support is disabled");
		if (ntohs(hdr_v2->len) < PP2_ADDR_LEN_INET6)
		    return ("short address field");
		if (haproxy_srvr_parse_v2_addr_v6(hdr_v2->addr.ip6.src_addr,
						  hdr_v2->addr.ip6.src_port,
						  smtp_client_addr,
						  smtp_client_port) < 0)
		    return ("client network address conversion error");
		if (msg_verbose)
		    msg_info("%s: smtp_client_addr=%s smtp_client_port=%s",
		      myname, smtp_client_addr->buf, smtp_client_port->buf);
		if (haproxy_srvr_parse_v2_addr_v6(hdr_v2->addr.ip6.dst_addr,
						  hdr_v2->addr.ip6.dst_port,
						  smtp_server_addr,
						  smtp_server_port) < 0)
		    return ("server network address conversion error");
		if (msg_verbose)
		    msg_info("%s: smtp_server_addr=%s smtp_server_port=%s",
		      myname, smtp_server_addr->buf, smtp_server_port->buf);
		break;
#else
		return ("Postfix IPv6 support is not compiled in");
#endif
	    }
	default:
	    return ("unsupported network protocol");
	}
	/* For now, skip and ignore TLVs. */
	*str_len = PP2_HEADER_LEN + ntohs(hdr_v2->len);
	return (0);

	/*
	 * Non-proxied connection, use the proxy-to-server connection info.
	 */
    case PP2_CMD_LOCAL:
	/* For now, skip and ignore TLVs. */
	*non_proxy = 1;
	*str_len = PP2_HEADER_LEN + ntohs(hdr_v2->len);
	return (0);
    default:
	return ("bad command in proxy header");
    }
}

/* haproxy_srvr_parse - parse haproxy line */

const char *haproxy_srvr_parse(const char *str, ssize_t *str_len,
			               int *non_proxy,
			               MAI_HOSTADDR_STR *smtp_client_addr,
			               MAI_SERVPORT_STR *smtp_client_port,
			               MAI_HOSTADDR_STR *smtp_server_addr,
			               MAI_SERVPORT_STR *smtp_server_port)
{
    const char *err;

    if (proto_info == 0)
	proto_info = inet_proto_info();

    *non_proxy = 0;

    /*
     * XXX We don't accept connections with the "UNKNOWN" protocol type,
     * because those would sidestep address-based access control mechanisms.
     */

    /*
     * Try version 1 protocol.
     */
    if (strncmp(str, "PROXY ", 6) == 0) {
	char   *saved_str = mystrndup(str, *str_len);
	char   *cp = saved_str;
	char   *beyond_header = split_at(saved_str, '\n');
	int     addr_family;

#define NEXT_TOKEN mystrtok(&cp, " \r")
	if (beyond_header == 0)
	    err = "missing protocol header terminator";
	else if (haproxy_srvr_parse_lit(NEXT_TOKEN, "PROXY", (char *) 0) < 0)
	    err = "bad or missing protocol header";
	else if (haproxy_srvr_parse_proto(NEXT_TOKEN, &addr_family) < 0)
	    err = "bad or missing protocol type";
	else if (haproxy_srvr_parse_addr(NEXT_TOKEN, smtp_client_addr,
					 addr_family) < 0)
	    err = "bad or missing client address";
	else if (haproxy_srvr_parse_addr(NEXT_TOKEN, smtp_server_addr,
					 addr_family) < 0)
	    err = "bad or missing server address";
	else if (haproxy_srvr_parse_port(NEXT_TOKEN, smtp_client_port) < 0)
	    err = "bad or missing client port";
	else if (haproxy_srvr_parse_port(NEXT_TOKEN, smtp_server_port) < 0)
	    err = "bad or missing server port";
	else {
	    err = 0;
	    *str_len = beyond_header - saved_str;
	}
	myfree(saved_str);

	return (err);
    }

    /*
     * Try version 2 protocol.
     */
    else {
	return (haproxy_srvr_parse_v2_hdr(str, str_len, non_proxy,
					  smtp_client_addr, smtp_client_port,
				       smtp_server_addr, smtp_server_port));
    }
}

/* haproxy_srvr_receive - receive and parse haproxy protocol handshake */

int     haproxy_srvr_receive(int fd, int *non_proxy,
			             MAI_HOSTADDR_STR *smtp_client_addr,
			             MAI_SERVPORT_STR *smtp_client_port,
			             MAI_HOSTADDR_STR *smtp_server_addr,
			             MAI_SERVPORT_STR *smtp_server_port)
{
    const char *err;
    VSTRING *escape_buf;
    char    read_buf[HAPROXY_HEADER_MAX_LEN + 1];
    ssize_t read_len;

    /*
     * We must not read(2) past the end of the HaProxy handshake. The v2
     * protocol assumes that the handshake will never be fragmented,
     * therefore we peek, parse the entire input, then read(2) only the
     * number of bytes parsed.
     */
    if ((read_len = recv(fd, read_buf, sizeof(read_buf) - 1, MSG_PEEK)) <= 0) {
	msg_warn("haproxy read: EOF");
	return (-1);
    }

    /*
     * Parse the haproxy handshake, and determine the handshake length.
     */
    read_buf[read_len] = 0;

    if ((err = haproxy_srvr_parse(read_buf, &read_len, non_proxy,
				  smtp_client_addr, smtp_client_port,
				smtp_server_addr, smtp_server_port)) != 0) {
	escape_buf = vstring_alloc(read_len * 2);
	escape(escape_buf, read_buf, read_len);
	msg_warn("haproxy read: %s: %s", err, vstring_str(escape_buf));
	vstring_free(escape_buf);
	return (-1);
    }

    /*
     * Try to pop the haproxy handshake off the input queue.
     */
    if (recv(fd, read_buf, read_len, 0) != read_len) {
	msg_warn("haproxy read: %m");
	return (-1);
    }
    return (0);
}
