/*++
/* NAME
/*	tls_proxy_params_scan 3
/* SUMMARY
/*	read TLS_PARAMS structure from stream
/* SYNOPSIS
/*	#include <tls_proxy.h>
/*
/*	int	tls_proxy_params_scan(scan_fn, stream, flags, ptr)
/*	ATTR_SCAN_MASTER_FN scan_fn;
/*	VSTREAM *stream;
/*	int     flags;
/*	void    *ptr;
/*
/*	void	tls_proxy_params_free(params)
/*	TLS_PARAMS *params;
/* DESCRIPTION
/*	tls_proxy_params_scan() reads a TLS_PARAMS structure from
/*	the named stream using the specified attribute scan routine.
/*	tls_proxy_params_scan() is meant to be passed as a call-back
/*	function to attr_scan(), as shown below.
/*
/*	tls_proxy_params_free() destroys a TLS_PARAMS structure
/*	that was created by tls_proxy_params_scan().
/*
/*	TLS_PARAMS *params = 0;
/*	...
/*	... RECV_ATTR_FUNC(tls_proxy_params_scan, (void *) &params)
/*	...
/*	if (params != 0)
/*	    tls_proxy_params_free(params);
/* DIAGNOSTICS
/*	Fatal: out of memory.
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

#ifdef USE_TLS

/* System library. */

#include <sys_defs.h>

/* Utility library */

#include <argv_attr.h>
#include <attr.h>
#include <msg.h>
#include <vstring.h>

/* Global library. */

#include <mail_params.h>

/* TLS library. */

#include <tls.h>
#include <tls_proxy.h>

#define STR(x) vstring_str(x)
#define LEN(x) VSTRING_LEN(x)

/* tls_proxy_params_free - destroy TLS_PARAMS structure */

void    tls_proxy_params_free(TLS_PARAMS * params)
{
    myfree(params->tls_high_clist);
    myfree(params->tls_medium_clist);
    myfree(params->tls_low_clist);
    myfree(params->tls_export_clist);
    myfree(params->tls_null_clist);
    myfree(params->tls_eecdh_auto);
    myfree(params->tls_eecdh_strong);
    myfree(params->tls_eecdh_ultra);
    myfree(params->tls_bug_tweaks);
    myfree(params->tls_ssl_options);
    myfree(params->tls_dane_agility);
    myfree(params->tls_dane_digests);
    myfree(params->tls_mgr_service);
    myfree(params->tls_tkt_cipher);
    myfree(params->openssl_path);
    myfree((void *) params);
}

/* tls_proxy_params_scan - receive TLS_PARAMS from stream */

int     tls_proxy_params_scan(ATTR_SCAN_MASTER_FN scan_fn, VSTREAM * fp,
			              int flags, void *ptr)
{
    TLS_PARAMS *params
    = (TLS_PARAMS *) mymalloc(sizeof(*params));
    int     ret;
    VSTRING *tls_high_clist = vstring_alloc(25);
    VSTRING *tls_medium_clist = vstring_alloc(25);
    VSTRING *tls_low_clist = vstring_alloc(25);
    VSTRING *tls_export_clist = vstring_alloc(25);
    VSTRING *tls_null_clist = vstring_alloc(25);
    VSTRING *tls_eecdh_auto = vstring_alloc(25);
    VSTRING *tls_eecdh_strong = vstring_alloc(25);
    VSTRING *tls_eecdh_ultra = vstring_alloc(25);
    VSTRING *tls_bug_tweaks = vstring_alloc(25);
    VSTRING *tls_ssl_options = vstring_alloc(25);
    VSTRING *tls_dane_agility = vstring_alloc(25);
    VSTRING *tls_dane_digests = vstring_alloc(25);
    VSTRING *tls_mgr_service = vstring_alloc(25);
    VSTRING *tls_tkt_cipher = vstring_alloc(25);
    VSTRING *openssl_path = vstring_alloc(25);

    if (msg_verbose)
	msg_info("begin tls_proxy_params_scan");

    /*
     * Note: memset() is not a portable way to initialize non-integer types.
     */
    memset(params, 0, sizeof(*params));
    ret = scan_fn(fp, flags | ATTR_FLAG_MORE,
		  RECV_ATTR_STR(VAR_TLS_HIGH_CLIST, tls_high_clist),
		  RECV_ATTR_STR(VAR_TLS_MEDIUM_CLIST, tls_medium_clist),
		  RECV_ATTR_STR(VAR_TLS_LOW_CLIST, tls_low_clist),
		  RECV_ATTR_STR(VAR_TLS_EXPORT_CLIST, tls_export_clist),
		  RECV_ATTR_STR(VAR_TLS_NULL_CLIST, tls_null_clist),
		  RECV_ATTR_STR(VAR_TLS_EECDH_AUTO, tls_eecdh_auto),
		  RECV_ATTR_STR(VAR_TLS_EECDH_STRONG, tls_eecdh_strong),
		  RECV_ATTR_STR(VAR_TLS_EECDH_ULTRA, tls_eecdh_ultra),
		  RECV_ATTR_STR(VAR_TLS_BUG_TWEAKS, tls_bug_tweaks),
		  RECV_ATTR_STR(VAR_TLS_SSL_OPTIONS, tls_ssl_options),
		  RECV_ATTR_STR(VAR_TLS_DANE_AGILITY, tls_dane_agility),
		  RECV_ATTR_STR(VAR_TLS_DANE_DIGESTS, tls_dane_digests),
		  RECV_ATTR_STR(VAR_TLS_MGR_SERVICE, tls_mgr_service),
		  RECV_ATTR_STR(VAR_TLS_TKT_CIPHER, tls_tkt_cipher),
		  RECV_ATTR_STR(VAR_OPENSSL_PATH, openssl_path),
		  RECV_ATTR_INT(VAR_TLS_DAEMON_RAND_BYTES,
				&params->tls_daemon_rand_bytes),
		  RECV_ATTR_INT(VAR_TLS_APPEND_DEF_CA,
				&params->tls_append_def_CA),
		  RECV_ATTR_INT(VAR_TLS_BC_PKEY_FPRINT,
				&params->tls_bc_pkey_fprint),
		  RECV_ATTR_INT(VAR_TLS_DANE_TAA_DGST,
				&params->tls_dane_taa_dgst),
		  RECV_ATTR_INT(VAR_TLS_PREEMPT_CLIST,
				&params->tls_preempt_clist),
		  RECV_ATTR_INT(VAR_TLS_MULTI_WILDCARD,
				&params->tls_multi_wildcard),
		  ATTR_TYPE_END);
    /* Always construct a well-formed structure. */
    params->tls_high_clist = vstring_export(tls_high_clist);
    params->tls_medium_clist = vstring_export(tls_medium_clist);
    params->tls_low_clist = vstring_export(tls_low_clist);
    params->tls_export_clist = vstring_export(tls_export_clist);
    params->tls_null_clist = vstring_export(tls_null_clist);
    params->tls_eecdh_auto = vstring_export(tls_eecdh_auto);
    params->tls_eecdh_strong = vstring_export(tls_eecdh_strong);
    params->tls_eecdh_ultra = vstring_export(tls_eecdh_ultra);
    params->tls_bug_tweaks = vstring_export(tls_bug_tweaks);
    params->tls_ssl_options = vstring_export(tls_ssl_options);
    params->tls_dane_agility = vstring_export(tls_dane_agility);
    params->tls_dane_digests = vstring_export(tls_dane_digests);
    params->tls_mgr_service = vstring_export(tls_mgr_service);
    params->tls_tkt_cipher = vstring_export(tls_tkt_cipher);
    params->openssl_path = vstring_export(openssl_path);

    ret = (ret == 21 ? 1 : -1);
    if (ret != 1) {
	tls_proxy_params_free(params);
	params = 0;
    }
    *(TLS_PARAMS **) ptr = params;
    if (msg_verbose)
	msg_info("tls_proxy_params_scan ret=%d", ret);
    return (ret);
}

#endif
