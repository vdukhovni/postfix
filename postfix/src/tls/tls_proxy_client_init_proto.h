#ifndef _TLS_PROXY_CLIENT_INIT_PROTO_H_INCLUDED_
#define _TLS_PROXY_CLIENT_INIT_PROTO_H_INCLUDED_

/*++
/* NAME
/*	tls_proxy_client_init_proto 3h
/* SUMMARY
/*	TLS_CLIENT_START support
/* SYNOPSIS
/*	#include <tls_proxy_client_init_proto.h>
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

#ifdef USE_TLS

#define TLS_PROXY_CLIENT_INIT_PROPS(props, a1, a2, a3, a4, a5, a6, a7, a8, \
    a9, a10, a11, a12, a13, a14) \
    (((props)->a1), ((props)->a2), ((props)->a3), \
    ((props)->a4), ((props)->a5), ((props)->a6), ((props)->a7), \
    ((props)->a8), ((props)->a9), ((props)->a10), ((props)->a11), \
    ((props)->a12), ((props)->a13), ((props)->a14))

extern char *tls_proxy_client_init_serialize(ATTR_PRINT_COMMON_FN, VSTRING *, const TLS_CLIENT_INIT_PROPS *);
extern TLS_CLIENT_INIT_PROPS *tls_proxy_client_init_from_string(ATTR_SCAN_COMMON_FN, VSTRING *);
extern int tls_proxy_client_init_print(ATTR_PRINT_COMMON_FN, VSTREAM *, int, const void *);
extern void tls_proxy_client_init_free(TLS_CLIENT_INIT_PROPS *);
extern int tls_proxy_client_init_scan(ATTR_SCAN_COMMON_FN, VSTREAM *, int, void *);

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
