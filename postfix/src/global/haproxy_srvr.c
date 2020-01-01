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
/*	syntax (::ffff:1.2.3.4) is converted to IPV4 syntax, provided
/*	that IPv4 support is enabled. In case of success, the
/*	str_len argument is updated with the number of bytes parsed,
/*	and the non_proxy argument is true or false if the
/*	haproxy message specifies a non-proxied connection.
/*
/*	haproxy_srvr_receive() receives and parses a haproxy protocol
/*	handshake. This must be called before any I/O is done on
/*	the specified file descriptor. The result is 0 in case of
/*	success, -1 in case of error. All errors are logged.
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

/* Global library. */

#include <haproxy_srvr.h>

/* Application-specific. */

static INET_PROTO_INFO *proto_info;

#define STR_OR_NULL(str) ((str) ? (str) : "(null)")

/* haproxy_srvr_parse_lit - extract and validate string literal */

static int haproxy_srvr_parse_lit(const char *str,...)
{
    va_list ap;
    const char *cp;
    int     result = -1;

    if (msg_verbose)
	msg_info("haproxy_srvr_parse: %s", STR_OR_NULL(str));

    if (str != 0) {
	va_start(ap, str);
	while (result < 0 && (cp = va_arg(ap, const char *)) != 0)
	    if (strcmp(str, cp) == 0)
		result = 0;
	va_end(ap);
    }
    return (result);
}

/* haproxy_srvr_parse_proto - parse and validate the protocol type */

static int haproxy_srvr_parse_proto(const char *str, int *addr_family)
{
    if (msg_verbose)
	msg_info("haproxy_srvr_parse: proto=%s", STR_OR_NULL(str));

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

/* haproxy_srvr_parse - parse haproxy line */

const char *haproxy_srvr_parse(const char *str, ssize_t *str_len,
			               int *non_proxy,
			               MAI_HOSTADDR_STR *smtp_client_addr,
			               MAI_SERVPORT_STR *smtp_client_port,
			               MAI_HOSTADDR_STR *smtp_server_addr,
			               MAI_SERVPORT_STR *smtp_server_port)
{
    const char *err;
    int     addr_family;

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
	char   *saved_str = mystrdup(str);
	char   *cp = saved_str;
	char   *beyond_header = split_at(saved_str, '\n');

#define NEXT_TOKEN mystrtok(&cp, " \r")
	if (beyond_header == 0)
	    err = "missing protocol header terminator";
	else if (haproxy_srvr_parse_lit(NEXT_TOKEN, "PROXY", (char *) 0) < 0)
	    err = "unexpected protocol header";
	else if (haproxy_srvr_parse_proto(NEXT_TOKEN, &addr_family) < 0)
	    err = "unsupported protocol type";
	else if (haproxy_srvr_parse_addr(NEXT_TOKEN, smtp_client_addr,
					 addr_family) < 0)
	    err = "unexpected client address syntax";
	else if (haproxy_srvr_parse_addr(NEXT_TOKEN, smtp_server_addr,
					 addr_family) < 0)
	    err = "unexpected server address syntax";
	else if (haproxy_srvr_parse_port(NEXT_TOKEN, smtp_client_port) < 0)
	    err = "unexpected client port syntax";
	else if (haproxy_srvr_parse_port(NEXT_TOKEN, smtp_server_port) < 0)
	    err = "unexpected server port syntax";
	else {
	    err = 0;
	    *str_len = beyond_header - saved_str;
	}
	myfree(saved_str);
	return (err);
    }

    /*
     * Assume version 2 protocol.
     */
    else {
	return ("v2 protocol is not implemented");
    }
}

/* haproxy_srvr_receive - redceive and parse haproxy protocol handshake */

int     haproxy_srvr_receive(int fd, int *non_proxy,
			             MAI_HOSTADDR_STR *smtp_client_addr,
			             MAI_SERVPORT_STR *smtp_client_port,
			             MAI_HOSTADDR_STR *smtp_server_addr,
			             MAI_SERVPORT_STR *smtp_server_port)
{
    const char *err;
    VSTRING *escape_buf;
    char    read_buf[HAPROXY_MAX_LEN];
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
	escape_buf = vstring_alloc(HAPROXY_MAX_LEN + 2);
	escape(escape_buf, read_buf, read_len);
	msg_warn("haproxy read: %s: %s", err, vstring_str(escape_buf));
	vstring_free(escape_buf);
	return (-1);
    }

    /*
     * Try to pop the haproxy handshake off the input queue.
     */
    if (recv(fd, read_buf, read_len, 0) <= 0) {
	msg_warn("haproxy read: %m");
	return (-1);
    }
    return (0);
}

 /*
  * Test program.
  */
#ifdef TEST
int     main(int argc, char **argv)
{
    /* Test cases with inputs and expected outputs. */
    typedef struct TEST_CASE {
	const char *haproxy_request;
	ssize_t exp_str_len;
	int     exp_non_proxy;
	const char *exp_return;
	const char *exp_client_addr;
	const char *exp_server_addr;
	const char *exp_client_port;
	const char *exp_server_port;
    } TEST_CASE;
    static TEST_CASE test_cases[] = {
	/* IPv6. */
	{"PROXY TCP6 fc:00:00:00:1:2:3:4 fc:00:00:00:4:3:2:1 123 321", 0, 0, 0, "fc::1:2:3:4", "fc::4:3:2:1", "123", "321"},
	{"PROXY TCP6 FC:00:00:00:1:2:3:4 FC:00:00:00:4:3:2:1 123 321", 0, 0, 0, "fc::1:2:3:4", "fc::4:3:2:1", "123", "321"},
	{"PROXY TCP6 1.2.3.4 4.3.2.1 123 321", 0, 0, "unexpected client address syntax"},
	{"PROXY TCP6 fc:00:00:00:1:2:3:4 4.3.2.1 123 321", 0, 0, "unexpected server address syntax"},
	/* IPv4 in IPv6. */
	{"PROXY TCP6 ::ffff:1.2.3.4 ::ffff:4.3.2.1 123 321", 0, 0, 0, "1.2.3.4", "4.3.2.1", "123", "321"},
	{"PROXY TCP6 ::FFFF:1.2.3.4 ::FFFF:4.3.2.1 123 321", 0, 0, 0, "1.2.3.4", "4.3.2.1", "123", "321"},
	{"PROXY TCP4 ::ffff:1.2.3.4 ::ffff:4.3.2.1 123 321", 0, 0, "unexpected client address syntax"},
	{"PROXY TCP4 1.2.3.4 ::ffff:4.3.2.1 123 321", 0, 0, "unexpected server address syntax"},
	/* IPv4. */
	{"PROXY TCP4 1.2.3.4 4.3.2.1 123 321", 0, 0, 0, "1.2.3.4", "4.3.2.1", "123", "321"},
	{"PROXY TCP4 01.02.03.04 04.03.02.01 123 321", 0, 0, 0, "1.2.3.4", "4.3.2.1", "123", "321"},
	{"PROXY TCP4 1.2.3.4 4.3.2.1 123456 321", 0, 0, "unexpected client port syntax"},
	{"PROXY TCP4 1.2.3.4 4.3.2.1 123 654321", 0, 0, "unexpected server port syntax"},
	{"PROXY TCP4 1.2.3.4 4.3.2.1 0123 321", 0, 0, "unexpected client port syntax"},
	{"PROXY TCP4 1.2.3.4 4.3.2.1 123 0321", 0, 0, "unexpected server port syntax"},
	/* Missing fields. */
	{"PROXY TCP6 fc:00:00:00:1:2:3:4 fc:00:00:00:4:3:2:1 123", 0, 0, "unexpected server port syntax"},
	{"PROXY TCP6 fc:00:00:00:1:2:3:4 fc:00:00:00:4:3:2:1", 0, 0, "unexpected client port syntax"},
	{"PROXY TCP6 fc:00:00:00:1:2:3:4", 0, 0, "unexpected server address syntax"},
	{"PROXY TCP6", 0, 0, "unexpected client address syntax"},
	{"PROXY TCP4 1.2.3.4 4.3.2.1 123", 0, 0, "unexpected server port syntax"},
	{"PROXY TCP4 1.2.3.4 4.3.2.1", 0, 0, "unexpected client port syntax"},
	{"PROXY TCP4 1.2.3.4", 0, 0, "unexpected server address syntax"},
	{"PROXY TCP4", 0, 0, "unexpected client address syntax"},
	/* Other. */
	{"PROXY BLAH", 0, 0, "unsupported protocol type"},
	{"BLAH", 0, 0, "v2 protocol is not implemented"},
	0,
    };
    TEST_CASE *test_case;

    /* Actual results. */
    const char *act_return;
    ssize_t act_str_len;
    int     act_non_proxy;
    MAI_HOSTADDR_STR act_smtp_client_addr;
    MAI_HOSTADDR_STR act_smtp_server_addr;
    MAI_SERVPORT_STR act_smtp_client_port;
    MAI_SERVPORT_STR act_smtp_server_port;

    /* Findings. */
    int     tests_failed = 0;
    int     test_failed;

    for (tests_failed = 0, test_case = test_cases; test_case->haproxy_request;
	 tests_failed += test_failed, test_case++) {
	test_failed = 0;
	act_str_len = strlen(test_case->haproxy_request);
	if (test_case->exp_str_len == 0)
	    test_case->exp_str_len = act_str_len;
	act_return =
	    haproxy_srvr_parse(test_case->haproxy_request, &act_str_len,
			       &act_non_proxy,
			       &act_smtp_client_addr, &act_smtp_client_port,
			       &act_smtp_server_addr, &act_smtp_server_port);
	if (act_return != test_case->exp_return) {
	    msg_warn("test case %d return expected=%s actual=%s",
		     (int) (test_case - test_cases),
		     test_case->exp_return ?
		     test_case->exp_return : "(null)",
		     act_return ? act_return : "(null)");
	    test_failed = 1;
	    continue;
	}
	if (act_str_len != test_case->exp_str_len) {
	    msg_warn("test case %d str_len expected=%ld actual=%ld",
		     (int) (test_case - test_cases),
		     (long) test_case->exp_str_len, (long) act_str_len);
	    test_failed = 1;
	    continue;
	}
	if (act_non_proxy != test_case->exp_non_proxy) {
	    msg_warn("test case %d non_proxy expected=%d actual=%d",
		     (int) (test_case - test_cases),
		     test_case->exp_non_proxy, act_non_proxy);
	    test_failed = 1;
	    continue;
	}
	if (test_case->exp_return != 0)
	    continue;
	if (strcmp(test_case->exp_client_addr, act_smtp_client_addr.buf)) {
	    msg_warn("test case %d client_addr  expected=%s actual=%s",
		     (int) (test_case - test_cases),
		     test_case->exp_client_addr, act_smtp_client_addr.buf);
	    test_failed = 1;
	}
	if (strcmp(test_case->exp_server_addr, act_smtp_server_addr.buf)) {
	    msg_warn("test case %d server_addr  expected=%s actual=%s",
		     (int) (test_case - test_cases),
		     test_case->exp_server_addr, act_smtp_server_addr.buf);
	    test_failed = 1;
	}
	if (strcmp(test_case->exp_client_port, act_smtp_client_port.buf)) {
	    msg_warn("test case %d client_port  expected=%s actual=%s",
		     (int) (test_case - test_cases),
		     test_case->exp_client_port, act_smtp_client_port.buf);
	    test_failed = 1;
	}
	if (strcmp(test_case->exp_server_port, act_smtp_server_port.buf)) {
	    msg_warn("test case %d server_port  expected=%s actual=%s",
		     (int) (test_case - test_cases),
		     test_case->exp_server_port, act_smtp_server_port.buf);
	    test_failed = 1;
	}
    }
    if (tests_failed)
	msg_info("tests failed: %d", tests_failed);
    exit(tests_failed != 0);
}

#endif
