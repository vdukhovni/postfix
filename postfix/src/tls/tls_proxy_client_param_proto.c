/*++
/* NAME
/*	tls_proxy_client_param_proto 3
/* SUMMARY
/*	TLS_CLIENT_PARAMS structure support
/* SYNOPSIS
/*	#include <tls_proxy_client_param_proto.h>
/*
/*	TLS_CLIENT_PARAMS *tls_proxy_client_param_from_config(params)
/*	TLS_CLIENT_PARAMS *params;
/*
/*	char	*tls_proxy_client_param_serialize(print_fn, buf, params)
/*	ATTR_PRINT_COMMON_FN print_fn;
/*	VSTRING *buf;
/*	const TLS_CLIENT_PARAMS *params;
/*
/*	TLS_CLIENT_PARAMS *tls_proxy_client_param_from_string(
/*	ATTR_SCAN_COMMON_FN scan_fn,
/*	const VSTRING *buf)
/*
/*	int	tls_proxy_client_param_print(print_fn, stream, flags, ptr)
/*	ATTR_PRINT_COMMON_FN print_fn;
/*	VSTREAM	*stream;
/*	int	flags;
/*	const void *ptr;
/*
/*	int	tls_proxy_client_param_scan(scan_fn, stream, flags, ptr)
/*	ATTR_SCAN_COMMON_FN scan_fn;
/*	VSTREAM *stream;
/*	int	flags;
/*	void	*ptr;
/*
/*	void	tls_proxy_client_param_free(params)
/*	TLS_CLIENT_PARAMS *params;
/* DESCRIPTION
/*	tls_proxy_client_param_from_config() initializes a TLS_CLIENT_PARAMS
/*	structure from configuration parameters and returns its
/*	argument. Strings are not copied. The result must therefore
/*	not be passed to tls_proxy_client_param_free().
/*
/*	tls_proxy_client_param_serialize() serializes the specified
/*	object to a memory buffer, using the specified print function
/*	(typically, attr_print_plain). The result can be used
/*	determine whether there are any differences between instances
/*	of the same object type.
/*
/*	tls_proxy_client_param_from_string() deserializes the specified
/*	buffer into a TLS_CLIENT_PARAMS object, and returns null in case
/*	of error. The result if not null should be passed to
/*	tls_proxy_client_param_free().
/*
/*	tls_proxy_client_param_print() writes a TLS_CLIENT_PARAMS structure to
/*	the named stream using the specified attribute print routine.
/*	tls_proxy_client_param_print() is meant to be passed as a call-back to
/*	attr_print(), thusly:
/*
/*	SEND_ATTR_FUNC(tls_proxy_client_param_print, (const void *) param), ...
/*
/*	tls_proxy_client_param_scan() reads a TLS_CLIENT_PARAMS structure from
/*	the named stream using the specified attribute scan routine.
/*	tls_proxy_client_param_scan() is meant to be passed as a call-back
/*	function to attr_scan(), as shown below.
/*
/*	tls_proxy_client_param_free() destroys a TLS_CLIENT_PARAMS structure
/*	that was created by tls_proxy_client_param_scan().
/*
/*	TLS_CLIENT_PARAMS *param = 0;
/*	...
/*	... RECV_ATTR_FUNC(tls_proxy_client_param_scan, (void *) &param)
/*	...
/*	if (param != 0)
/*	    tls_proxy_client_param_free(param);
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	Google, Inc.
/*	111 8th Avenue
/*	New York, NY 10011, USA
/*
/*	Wietse Venema
/*	porcupine.org
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
#include <tls_proxy_attr.h>
#include <tls_proxy_client_param_proto.h>

/* tls_proxy_client_param_from_config - initialize TLS_CLIENT_PARAMS from configuration */

TLS_CLIENT_PARAMS *tls_proxy_client_param_from_config(TLS_CLIENT_PARAMS *params)
{
    TLS_PROXY_CLIENT_PARAMS(params,
			    tls_cnf_file = var_tls_cnf_file,
			    tls_cnf_name = var_tls_cnf_name,
			    tls_high_clist = var_tls_high_clist,
			    tls_medium_clist = var_tls_medium_clist,
			    tls_null_clist = var_tls_null_clist,
			    tls_eecdh_auto = var_tls_eecdh_auto,
			    tls_eecdh_strong = var_tls_eecdh_strong,
			    tls_eecdh_ultra = var_tls_eecdh_ultra,
			    tls_ffdhe_auto = var_tls_ffdhe_auto,
			    tls_bug_tweaks = var_tls_bug_tweaks,
			    tls_ssl_options = var_tls_ssl_options,
			    tls_dane_digests = var_tls_dane_digests,
			    tls_mgr_service = var_tls_mgr_service,
			    tls_tkt_cipher = var_tls_tkt_cipher,
			  tls_daemon_rand_bytes = var_tls_daemon_rand_bytes,
			    tls_append_def_CA = var_tls_append_def_CA,
			    tls_preempt_clist = var_tls_preempt_clist,
			    tls_multi_wildcard = var_tls_multi_wildcard,
			    tls_fast_shutdown = var_tls_fast_shutdown);
    return (params);
}

/* tls_proxy_client_param_serialize - serialize TLS_CLIENT_PARAMS to string */

char   *tls_proxy_client_param_serialize(ATTR_PRINT_COMMON_FN print_fn,
					         VSTRING *buf,
				            const TLS_CLIENT_PARAMS *params)
{
    const char myname[] = "tls_proxy_client_param_serialize";
    VSTREAM *mp;

    if ((mp = vstream_memopen(buf, O_WRONLY)) == 0
	|| print_fn(mp, ATTR_FLAG_NONE,
		    SEND_ATTR_FUNC(tls_proxy_client_param_print,
				   (const void *) params),
		    ATTR_TYPE_END) != 0
	|| vstream_fclose(mp) != 0)
	msg_fatal("%s: can't serialize properties: %m", myname);
    return (vstring_str(buf));
}

/* tls_proxy_client_param_from_string - deserialize TLS_CLIENT_PARAMS */

TLS_CLIENT_PARAMS *tls_proxy_client_param_from_string(
					        ATTR_SCAN_COMMON_FN scan_fn,
						              VSTRING *buf)
{
    const char myname[] = "tls_proxy_client_param_from_string";
    TLS_CLIENT_PARAMS *params = 0;
    VSTREAM *mp;

    if ((mp = vstream_memopen(buf, O_RDONLY)) == 0
	|| scan_fn(mp, ATTR_FLAG_NONE,
		   RECV_ATTR_FUNC(tls_proxy_client_param_scan,
				  (void *) &params),
		   ATTR_TYPE_END) != 1
	|| vstream_fclose(mp) != 0)
	msg_fatal("%s: can't deserialize properties: %m", myname);
    return (params);
}

/* tls_proxy_client_param_print - send TLS_CLIENT_PARAMS over stream */

int     tls_proxy_client_param_print(ATTR_PRINT_COMMON_FN print_fn, VSTREAM *fp,
				             int flags, const void *ptr)
{
    const TLS_CLIENT_PARAMS *params = (const TLS_CLIENT_PARAMS *) ptr;
    int     ret;

    if (msg_verbose)
	msg_info("begin tls_proxy_client_param_print");

    ret = print_fn(fp, flags | ATTR_FLAG_MORE,
		   SEND_ATTR_STR(TLS_ATTR_CNF_FILE, params->tls_cnf_file),
		   SEND_ATTR_STR(TLS_ATTR_CNF_NAME, params->tls_cnf_name),
		   SEND_ATTR_STR(VAR_TLS_HIGH_CLIST, params->tls_high_clist),
		   SEND_ATTR_STR(VAR_TLS_MEDIUM_CLIST,
				 params->tls_medium_clist),
		   SEND_ATTR_STR(VAR_TLS_NULL_CLIST, params->tls_null_clist),
		   SEND_ATTR_STR(VAR_TLS_EECDH_AUTO, params->tls_eecdh_auto),
		   SEND_ATTR_STR(VAR_TLS_EECDH_STRONG,
				 params->tls_eecdh_strong),
		   SEND_ATTR_STR(VAR_TLS_EECDH_ULTRA,
				 params->tls_eecdh_ultra),
		   SEND_ATTR_STR(VAR_TLS_FFDHE_AUTO, params->tls_ffdhe_auto),
		   SEND_ATTR_STR(VAR_TLS_BUG_TWEAKS, params->tls_bug_tweaks),
		   SEND_ATTR_STR(VAR_TLS_SSL_OPTIONS,
				 params->tls_ssl_options),
		   SEND_ATTR_STR(VAR_TLS_DANE_DIGESTS,
				 params->tls_dane_digests),
		   SEND_ATTR_STR(VAR_TLS_MGR_SERVICE,
				 params->tls_mgr_service),
		   SEND_ATTR_STR(VAR_TLS_TKT_CIPHER, params->tls_tkt_cipher),
		   SEND_ATTR_INT(VAR_TLS_DAEMON_RAND_BYTES,
				 params->tls_daemon_rand_bytes),
		   SEND_ATTR_INT(VAR_TLS_APPEND_DEF_CA,
				 params->tls_append_def_CA),
		   SEND_ATTR_INT(VAR_TLS_PREEMPT_CLIST,
				 params->tls_preempt_clist),
		   SEND_ATTR_INT(VAR_TLS_MULTI_WILDCARD,
				 params->tls_multi_wildcard),
		   SEND_ATTR_INT(VAR_TLS_FAST_SHUTDOWN,
				 params->tls_fast_shutdown),
		   ATTR_TYPE_END);
    /* Do not flush the stream. */
    if (msg_verbose)
	msg_info("tls_proxy_client_param_print ret=%d", ret);
    return (ret);
}

/* tls_proxy_client_param_free - destroy TLS_CLIENT_PARAMS structure */

void    tls_proxy_client_param_free(TLS_CLIENT_PARAMS *params)
{
    myfree(params->tls_cnf_file);
    myfree(params->tls_cnf_name);
    myfree(params->tls_high_clist);
    myfree(params->tls_medium_clist);
    myfree(params->tls_null_clist);
    myfree(params->tls_eecdh_auto);
    myfree(params->tls_eecdh_strong);
    myfree(params->tls_eecdh_ultra);
    myfree(params->tls_ffdhe_auto);
    myfree(params->tls_bug_tweaks);
    myfree(params->tls_ssl_options);
    myfree(params->tls_dane_digests);
    myfree(params->tls_mgr_service);
    myfree(params->tls_tkt_cipher);
    myfree((void *) params);
}

/* tls_proxy_client_param_scan - receive TLS_CLIENT_PARAMS from stream */

int     tls_proxy_client_param_scan(ATTR_SCAN_COMMON_FN scan_fn, VSTREAM *fp,
				            int flags, void *ptr)
{
    TLS_CLIENT_PARAMS *params
    = (TLS_CLIENT_PARAMS *) mymalloc(sizeof(*params));
    int     ret;
    VSTRING *cnf_file = vstring_alloc(25);
    VSTRING *cnf_name = vstring_alloc(25);
    VSTRING *tls_high_clist = vstring_alloc(25);
    VSTRING *tls_medium_clist = vstring_alloc(25);
    VSTRING *tls_null_clist = vstring_alloc(25);
    VSTRING *tls_eecdh_auto = vstring_alloc(25);
    VSTRING *tls_eecdh_strong = vstring_alloc(25);
    VSTRING *tls_eecdh_ultra = vstring_alloc(25);
    VSTRING *tls_ffdhe_auto = vstring_alloc(25);
    VSTRING *tls_bug_tweaks = vstring_alloc(25);
    VSTRING *tls_ssl_options = vstring_alloc(25);
    VSTRING *tls_dane_digests = vstring_alloc(25);
    VSTRING *tls_mgr_service = vstring_alloc(25);
    VSTRING *tls_tkt_cipher = vstring_alloc(25);

    if (msg_verbose)
	msg_info("begin tls_proxy_client_param_scan");

    /*
     * Note: memset() is not a portable way to initialize non-integer types.
     */
    memset(params, 0, sizeof(*params));
    ret = scan_fn(fp, flags | ATTR_FLAG_MORE,
		  RECV_ATTR_STR(TLS_ATTR_CNF_FILE, cnf_file),
		  RECV_ATTR_STR(TLS_ATTR_CNF_NAME, cnf_name),
		  RECV_ATTR_STR(VAR_TLS_HIGH_CLIST, tls_high_clist),
		  RECV_ATTR_STR(VAR_TLS_MEDIUM_CLIST, tls_medium_clist),
		  RECV_ATTR_STR(VAR_TLS_NULL_CLIST, tls_null_clist),
		  RECV_ATTR_STR(VAR_TLS_EECDH_AUTO, tls_eecdh_auto),
		  RECV_ATTR_STR(VAR_TLS_EECDH_STRONG, tls_eecdh_strong),
		  RECV_ATTR_STR(VAR_TLS_EECDH_ULTRA, tls_eecdh_ultra),
		  RECV_ATTR_STR(VAR_TLS_FFDHE_AUTO, tls_ffdhe_auto),
		  RECV_ATTR_STR(VAR_TLS_BUG_TWEAKS, tls_bug_tweaks),
		  RECV_ATTR_STR(VAR_TLS_SSL_OPTIONS, tls_ssl_options),
		  RECV_ATTR_STR(VAR_TLS_DANE_DIGESTS, tls_dane_digests),
		  RECV_ATTR_STR(VAR_TLS_MGR_SERVICE, tls_mgr_service),
		  RECV_ATTR_STR(VAR_TLS_TKT_CIPHER, tls_tkt_cipher),
		  RECV_ATTR_INT(VAR_TLS_DAEMON_RAND_BYTES,
				&params->tls_daemon_rand_bytes),
		  RECV_ATTR_INT(VAR_TLS_APPEND_DEF_CA,
				&params->tls_append_def_CA),
		  RECV_ATTR_INT(VAR_TLS_PREEMPT_CLIST,
				&params->tls_preempt_clist),
		  RECV_ATTR_INT(VAR_TLS_MULTI_WILDCARD,
				&params->tls_multi_wildcard),
		  RECV_ATTR_INT(VAR_TLS_FAST_SHUTDOWN,
				&params->tls_fast_shutdown),
		  ATTR_TYPE_END);
    /* Always construct a well-formed structure. */
    params->tls_cnf_file = vstring_export(cnf_file);
    params->tls_cnf_name = vstring_export(cnf_name);
    params->tls_high_clist = vstring_export(tls_high_clist);
    params->tls_medium_clist = vstring_export(tls_medium_clist);
    params->tls_null_clist = vstring_export(tls_null_clist);
    params->tls_eecdh_auto = vstring_export(tls_eecdh_auto);
    params->tls_eecdh_strong = vstring_export(tls_eecdh_strong);
    params->tls_eecdh_ultra = vstring_export(tls_eecdh_ultra);
    params->tls_ffdhe_auto = vstring_export(tls_ffdhe_auto);
    params->tls_bug_tweaks = vstring_export(tls_bug_tweaks);
    params->tls_ssl_options = vstring_export(tls_ssl_options);
    params->tls_dane_digests = vstring_export(tls_dane_digests);
    params->tls_mgr_service = vstring_export(tls_mgr_service);
    params->tls_tkt_cipher = vstring_export(tls_tkt_cipher);

    ret = (ret == 19 ? 1 : -1);
    if (ret != 1) {
	tls_proxy_client_param_free(params);
	params = 0;
    }
    *(TLS_CLIENT_PARAMS **) ptr = params;
    if (msg_verbose)
	msg_info("tls_proxy_client_param_scan ret=%d", ret);
    return (ret);
}

#endif
