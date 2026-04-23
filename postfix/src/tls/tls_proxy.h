#ifndef _TLS_PROXY_H_INCLUDED_
#define _TLS_PROXY_H_INCLUDED_

/*++
/* NAME
/*	tls_proxy 3h
/* SUMMARY
/*	postscreen TLS proxy support
/* SYNOPSIS
/*	#include <tls_proxy.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <vstream.h>
#include <attr.h>

 /*
  * TLS library.
  */
#include <tls.h>

 /*
  * External interface.
  */
#define TLS_PROXY_FLAG_ROLE_SERVER	(1<<0)	/* request server role */
#define TLS_PROXY_FLAG_ROLE_CLIENT	(1<<1)	/* request client role */
#define TLS_PROXY_FLAG_SEND_CONTEXT	(1<<2)	/* send TLS context */
#define TLS_PROXY_FLAG_PROBE_ONLY	(1<<3)	/* what-if */

#include <tls_proxy_attr.h>

#include <tls_proxy_client_param_proto.h>
#include <tls_proxy_client_init_proto.h>
#include <tls_proxy_client_start_proto.h>

#include <tls_proxy_server_param_proto.h>
#include <tls_proxy_server_init_proto.h>
#include <tls_proxy_server_start_proto.h>

#ifdef USE_TLS

 /*
  * Functions that handle TLS_XXX_INIT_PROPS and TLS_XXX_START_PROPS. These
  * data structures are defined elsewhere, because they are also used in
  * non-proxied requests.
  */
#define tls_proxy_legacy_open(service, flags, peer_stream, peer_addr, \
                                          peer_port, timeout, serverid) \
    tls_proxy_open((service), (flags), (peer_stream), (peer_addr), \
	(peer_port), (timeout), (timeout), (serverid), \
	(void *) 0, (void *) 0, (void *) 0)

extern VSTREAM *tls_proxy_open(const char *, int, VSTREAM *, const char *,
			               const char *, int, int, const char *,
			               void *, void *, void *);
extern bool tls_proxy_probe(const char *, int, const char *, const char *);

extern TLS_SESS_STATE *tls_proxy_context_receive(VSTREAM *);
extern void tls_proxy_context_free(TLS_SESS_STATE *);
extern int tls_proxy_context_print(ATTR_PRINT_COMMON_FN, VSTREAM *, int, const void *);
extern int tls_proxy_context_scan(ATTR_SCAN_COMMON_FN, VSTREAM *, int, void *);

#endif					/* USE_TLS */

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
/*
/*	Wietse Venema
/*	porcupine.org
/*--*/

#endif
