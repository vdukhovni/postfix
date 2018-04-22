/*++
/* NAME
/*	tls_proxy_context_scan
/* SUMMARY
/*	read TLS session state from stream
/* SYNOPSIS
/*	#include <tls_proxy.h>
/*
/*	int     tls_proxy_context_scan(scan_fn, stream, flags, ptr)
/*	ATTR_SCAN_MASTER_FN scan_fn;
/*	VSTREAM *stream;
/*	int     flags;
/*	void    *ptr;
/*
/*	void	tls_proxy_context_free(tls_context)
/*	TLS_SESS_STATE *tls_context;
/* DESCRIPTION
/*	tls_proxy_context_scan() reads the public members of a
/*	TLS_ATTR_STATE structure from the named stream using the
/*	specified attribute scan routine.  tls_proxy_context_scan()
/*	is meant to be passed as a call-back to attr_scan() as shown
/*	below.
/*
/*	tls_proxy_context_free() destroys a TLS context object that
/*	was received with tls_proxy_context_scan().
/*
/*	TLS_ATTR_STATE *tls_context = 0;
/*	...
/*	... RECV_ATTR_FUNC(tls_proxy_context_scan, (void *) &tls_context), ...
/*	...
/*	if (tls_context)
/*	    tls_proxy_context_free(tls_context);
/* DIAGNOSTICS
/*	Fatal: out of memory.
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

#ifdef USE_TLS

/* System library. */

#include <sys_defs.h>

/* Utility library */

#include <attr.h>

/* TLS library. */

#include <tls.h>
#include <tls_proxy.h>

/* tls_proxy_context_scan - receive TLS session state from stream */

int     tls_proxy_context_scan(ATTR_SCAN_MASTER_FN scan_fn, VSTREAM *fp,
			               int flags, void *ptr)
{
    TLS_SESS_STATE *tls_context
    = (TLS_SESS_STATE *) mymalloc(sizeof(*tls_context));;
    int     ret;
    VSTRING *peer_CN = vstring_alloc(25);
    VSTRING *issuer_CN = vstring_alloc(25);
    VSTRING *peer_cert_fprint = vstring_alloc(60);	/* 60 for SHA-1 */
    VSTRING *peer_pkey_fprint = vstring_alloc(60);	/* 60 for SHA-1 */
    VSTRING *protocol = vstring_alloc(25);
    VSTRING *cipher_name = vstring_alloc(25);

    /*
     * Note: memset() is not a portable way to initialize non-integer types.
     */
    memset(tls_context, 0, sizeof(*tls_context));
    ret = scan_fn(fp, flags | ATTR_FLAG_MORE,
		  RECV_ATTR_STR(TLS_ATTR_PEER_CN, peer_CN),
		  RECV_ATTR_STR(TLS_ATTR_ISSUER_CN, issuer_CN),
		  RECV_ATTR_STR(TLS_ATTR_PEER_CERT_FPT, peer_cert_fprint),
		  RECV_ATTR_STR(TLS_ATTR_PEER_PKEY_FPT, peer_pkey_fprint),
		  RECV_ATTR_INT(TLS_ATTR_PEER_STATUS,
				&tls_context->peer_status),
		  RECV_ATTR_STR(TLS_ATTR_CIPHER_PROTOCOL, protocol),
		  RECV_ATTR_STR(TLS_ATTR_CIPHER_NAME, cipher_name),
		  RECV_ATTR_INT(TLS_ATTR_CIPHER_USEBITS,
				&tls_context->cipher_usebits),
		  RECV_ATTR_INT(TLS_ATTR_CIPHER_ALGBITS,
				&tls_context->cipher_algbits),
		  ATTR_TYPE_END);
    tls_context->peer_CN = vstring_export(peer_CN);
    tls_context->issuer_CN = vstring_export(issuer_CN);
    tls_context->peer_cert_fprint = vstring_export(peer_cert_fprint);
    tls_context->peer_pkey_fprint = vstring_export(peer_pkey_fprint);
    tls_context->protocol = vstring_export(protocol);
    tls_context->cipher_name = vstring_export(cipher_name);
    *(TLS_SESS_STATE **) ptr = tls_context;
    return (ret == 9 ? 1 : -1);
}

/* tls_proxy_context_free - destroy object from tls_proxy_context_receive() */

void    tls_proxy_context_free(TLS_SESS_STATE *tls_context)
{
    if (tls_context->peer_CN)
	myfree(tls_context->peer_CN);
    if (tls_context->issuer_CN)
	myfree(tls_context->issuer_CN);
    if (tls_context->peer_cert_fprint)
	myfree(tls_context->peer_cert_fprint);
    if (tls_context->peer_pkey_fprint)
	myfree(tls_context->peer_pkey_fprint);
    if (tls_context->protocol)
	myfree((void *) tls_context->protocol);
    if (tls_context->cipher_name)
	myfree((void *) tls_context->cipher_name);
    myfree((void *) tls_context);
}

#endif
