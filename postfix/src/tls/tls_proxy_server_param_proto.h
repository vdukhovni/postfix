#ifndef _TLS_PROXY_SERVER_PARAM_PROTO_H_INCLUDED_
#define _TLS_PROXY_SERVER_PARAM_PROTO_H_INCLUDED_

/*++
/* NAME
/*	tls_proxy_server_param_proto 3h
/* SUMMARY
/*	TLS proxy protocol support
/* SYNOPSIS
/*	#include <tls_proxy_server_param_proto.h>
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

 /*
  * TLS_SERVER_PARAMS structure, to communicate global TLS library settings
  * that are the same for all TLS server contexts. This information is used
  * in tlsproxy(8) to detect inconsistencies. If this structure is changed,
  * update all TLS_SERVER_PARAMS related functions in tls_proxy_server_*.c.
  * 
  * In the serialization these attributes are identified by their configuration
  * parameter names.
  * 
  * NOTE: this does not include openssl_path.
  */
typedef struct TLS_SERVER_PARAMS {
    char   *tls_cnf_file;
    char   *tls_cnf_name;
    char   *tls_high_clist;
    char   *tls_medium_clist;
    char   *tls_null_clist;
    char   *tls_eecdh_auto;
    char   *tls_eecdh_strong;
    char   *tls_eecdh_ultra;
    char   *tls_ffdhe_auto;
    char   *tls_bug_tweaks;
    char   *tls_ssl_options;
    char   *tls_mgr_service;
    char   *tls_tkt_cipher;
    int     tls_daemon_rand_bytes;
    int     tls_append_def_CA;
    int     tls_preempt_clist;
    int     tls_multi_wildcard;
    char   *tls_server_sni_maps;
    int     tls_fast_shutdown;
    int     tls_srvr_ccerts;
} TLS_SERVER_PARAMS;

#define TLS_PROXY_SERVER_PARAMS(params, a1, a2, a3, a4, a5, a6, a7, a8, \
    a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20) \
    (((params)->a1), ((params)->a2), ((params)->a3), \
    ((params)->a4), ((params)->a5), ((params)->a6), ((params)->a7), \
    ((params)->a8), ((params)->a9), ((params)->a10), ((params)->a11), \
    ((params)->a12), ((params)->a13), ((params)->a14), ((params)->a15), \
    ((params)->a16), ((params)->a17), ((params)->a18), ((params)->a19), \
    ((params)->a20))

extern TLS_SERVER_PARAMS *tls_proxy_server_param_from_config(TLS_SERVER_PARAMS *);
extern char *tls_proxy_server_param_serialize(ATTR_PRINT_COMMON_FN, VSTRING *, const TLS_SERVER_PARAMS *);
extern TLS_SERVER_PARAMS *tls_proxy_server_param_from_string(ATTR_SCAN_COMMON_FN, VSTRING *);
extern int tls_proxy_server_param_print(ATTR_PRINT_COMMON_FN, VSTREAM *, int, const void *);
extern void tls_proxy_server_param_free(TLS_SERVER_PARAMS *);
extern int tls_proxy_server_param_scan(ATTR_SCAN_COMMON_FN, VSTREAM *, int, void *);

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
