#ifndef _HAPROXY_SRVR_H_INCLUDED_
#define _HAPROXY_SRVR_H_INCLUDED_

/*++
/* NAME
/*	haproxy_srvr 3h
/* SUMMARY
/*	server-side haproxy protocol support
/* SYNOPSIS
/*	#include <haproxy_srvr.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <myaddrinfo.h>

 /*
  * External interface.
  */
extern const char *haproxy_srvr_parse(const char *, ssize_t *, int *,
			             MAI_HOSTADDR_STR *, MAI_SERVPORT_STR *,
			            MAI_HOSTADDR_STR *, MAI_SERVPORT_STR *);
extern int haproxy_srvr_receive(int, int *,
			             MAI_HOSTADDR_STR *, MAI_SERVPORT_STR *,
			            MAI_HOSTADDR_STR *, MAI_SERVPORT_STR *);

#define HAPROXY_PROTO_NAME	"haproxy"

#ifndef DO_GRIPE
#define DO_GRIPE 	1
#define DONT_GRIPE	0
#endif

 /*
  * Binary V2 protocol structure, also needed to create test data.
  */
#ifdef HAPROXY_SRVR_INTERNAL

 /*
  * Begin protocol v2 definitions from haproxy/include/types/connection.h.
  */
#define PP2_SIGNATURE		"\r\n\r\n\0\r\nQUIT\n"
#define PP2_SIGNATURE_LEN	12
#define PP2_HEADER_LEN		16

/* ver_cmd byte */
#define PP2_CMD_LOCAL		0x00
#define PP2_CMD_PROXY		0x01
#define PP2_CMD_MASK		0x0F

#define PP2_VERSION		0x20
#define PP2_VERSION_MASK	0xF0

/* fam byte */
#define PP2_TRANS_UNSPEC	0x00
#define PP2_TRANS_STREAM	0x01
#define PP2_TRANS_DGRAM		0x02
#define PP2_TRANS_MASK		0x0F

#define PP2_FAM_UNSPEC		0x00
#define PP2_FAM_INET		0x10
#define PP2_FAM_INET6		0x20
#define PP2_FAM_UNIX		0x30
#define PP2_FAM_MASK		0xF0

/* len field (2 bytes) */
#define PP2_ADDR_LEN_UNSPEC	(0)
#define PP2_ADDR_LEN_INET	(4 + 4 + 2 + 2)
#define PP2_ADDR_LEN_INET6	(16 + 16 + 2 + 2)
#define PP2_ADDR_LEN_UNIX	(108 + 108)

#define PP2_HDR_LEN_UNSPEC	(PP2_HEADER_LEN + PP2_ADDR_LEN_UNSPEC)
#define PP2_HDR_LEN_INET	(PP2_HEADER_LEN + PP2_ADDR_LEN_INET)
#define PP2_HDR_LEN_INET6	(PP2_HEADER_LEN + PP2_ADDR_LEN_INET6)
#define PP2_HDR_LEN_UNIX	(PP2_HEADER_LEN + PP2_ADDR_LEN_UNIX)

struct proxy_hdr_v2 {
    uint8_t sig[PP2_SIGNATURE_LEN];	/* PP2_SIGNATURE */
    uint8_t ver_cmd;			/* protocol version | command */
    uint8_t fam;			/* protocol family and transport */
    uint16_t len;			/* length of remainder */
    union {
	struct {			/* for TCP/UDP over IPv4, len = 12 */
	    uint32_t src_addr;
	    uint32_t dst_addr;
	    uint16_t src_port;
	    uint16_t dst_port;
	}       ip4;
	struct {			/* for TCP/UDP over IPv6, len = 36 */
	    uint8_t src_addr[16];
	    uint8_t dst_addr[16];
	    uint16_t src_port;
	    uint16_t dst_port;
	}       ip6;
	struct {			/* for AF_UNIX sockets, len = 216 */
	    uint8_t src_addr[108];
	    uint8_t dst_addr[108];
	}       unx;
    }       addr;
};

 /*
  * End protocol v2 definitions from haproxy/include/types/connection.h.
  */
#endif /* HAPROXY_SRVR_INTERNAL */

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

#endif
