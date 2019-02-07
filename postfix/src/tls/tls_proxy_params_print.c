/*++
/* NAME
/*	tls_proxy_params_print 3
/* SUMMARY
/*	write TLS_PARAMS structures to stream
/* SYNOPSIS
/*	#include <tls_proxy.h>
/*
/*	int     tls_proxy_params_print(print_fn, stream, flags, ptr)
/*	ATTR_PRINT_MASTER_FN print_fn;
/*	VSTREAM *stream;
/*	int     flags;
/*	void    *ptr;
/* DESCRIPTION
/*	tls_proxy_params_print() writes a TLS_PARAMS structure to
/*	the named stream using the specified attribute print routine.
/*	tls_proxy_params_print() is meant to be passed as a call-back to
/*	attr_print(), thusly:
/*
/*	SEND_ATTR_FUNC(tls_proxy_params_print, (void *) params), ...
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

#include <attr.h>
#include <msg.h>

/* Global library. */

#include <mail_params.h>

/* TLS library. */

#include <tls.h>
#include <tls_proxy.h>

/* tls_proxy_params_print - send TLS_PARAMS over stream */

int     tls_proxy_params_print(ATTR_PRINT_MASTER_FN print_fn, VSTREAM *fp,
			         int flags, void *ptr)
{
    TLS_PARAMS *params = (TLS_PARAMS *) ptr;
    int     ret;

    if (msg_verbose)
	msg_info("begin tls_proxy_params_print");

    ret = print_fn(fp, flags | ATTR_FLAG_MORE,
		   SEND_ATTR_STR(VAR_TLS_HIGH_CLIST, params->tls_high_clist),
		   SEND_ATTR_STR(VAR_TLS_MEDIUM_CLIST,
				 params->tls_medium_clist),
		   SEND_ATTR_STR(VAR_TLS_LOW_CLIST, params->tls_low_clist),
		   SEND_ATTR_STR(VAR_TLS_EXPORT_CLIST,
				 params->tls_export_clist),
		   SEND_ATTR_STR(VAR_TLS_NULL_CLIST, params->tls_null_clist),
		   SEND_ATTR_STR(VAR_TLS_EECDH_AUTO, params->tls_eecdh_auto),
		   SEND_ATTR_STR(VAR_TLS_EECDH_STRONG,
				 params->tls_eecdh_strong),
		   SEND_ATTR_STR(VAR_TLS_EECDH_ULTRA,
				 params->tls_eecdh_ultra),
		   SEND_ATTR_STR(VAR_TLS_BUG_TWEAKS, params->tls_bug_tweaks),
		   SEND_ATTR_STR(VAR_TLS_SSL_OPTIONS,
				 params->tls_ssl_options),
		   SEND_ATTR_STR(VAR_TLS_DANE_AGILITY,
				 params->tls_dane_agility),
		   SEND_ATTR_STR(VAR_TLS_DANE_DIGESTS,
				 params->tls_dane_digests),
		   SEND_ATTR_STR(VAR_TLS_MGR_SERVICE,
				 params->tls_mgr_service),
		   SEND_ATTR_STR(VAR_TLS_TKT_CIPHER, params->tls_tkt_cipher),
		   SEND_ATTR_STR(VAR_OPENSSL_PATH, params->openssl_path),
		   SEND_ATTR_INT(VAR_TLS_DAEMON_RAND_BYTES,
				 params->tls_daemon_rand_bytes),
		   SEND_ATTR_INT(VAR_TLS_APPEND_DEF_CA,
				 params->tls_append_def_CA),
		   SEND_ATTR_INT(VAR_TLS_BC_PKEY_FPRINT,
				 params->tls_bc_pkey_fprint),
		   SEND_ATTR_INT(VAR_TLS_DANE_TAA_DGST,
				 params->tls_dane_taa_dgst),
		   SEND_ATTR_INT(VAR_TLS_PREEMPT_CLIST,
				 params->tls_preempt_clist),
		   SEND_ATTR_INT(VAR_TLS_MULTI_WILDCARD,
				 params->tls_multi_wildcard),
		   ATTR_TYPE_END);
    /* Do not flush the stream. */
    if (msg_verbose)
	msg_info("tls_proxy_params_print ret=%d", ret);
    return (ret);
}

#endif
