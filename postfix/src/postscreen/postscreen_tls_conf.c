/*++
/* NAME
/*	postscreen_tls_conf 3
/* SUMMARY
/*	postscreen TLS proxy support, configuration adapter
/* SYNOPSIS
/*	#include <postscreen.h>
/*
/*	bool	psc_tls_ready;
/*	TLS_SERVER_PARAMS psc_tls_params;
/*	TLS_SERVER_INIT_PROPS psc_init_props;
/*
/*	bool	psc_tls_pre_jail(void)
/*
/*	bool	psc_tls_pre_start(
/*	const char *remote_endpt,
/*	TLS_SERVER_START_PROPS *start_props)
/* DESCRIPTION
/*	This module converts Postfix configuration settings into
/*	per-process TLS_SERVER_PARAMS and TLS_SERVER_INIT_PROPS, and
/*	into per-request TLS_SERVER_START_PROPS.
/*
/*	psc_tls_ready represents the TLS support state: true when
/*	TLS support is compiled in and enabled by configuration.
/*
/*	psc_tls_pre_jail() must be called once, before the process handles
/*	requests. If TLS is enabled by configuration, this function
/*	pre-computes TLS_SERVER_PARAMS and TLS_SERVER_INIT_PROPS, and
/*	returns true. This function logs a configuration warning when
/*	TLS is requested by configuration, but Postfix is built without
/*	TLS support.
/*
/*	psc_tls_pre_start() always returns the value of psc_tls_ready.
/*	If TLS is enabled by configuration, this function updates the
/*	structure referenced by the start_props argument with information
/*	based on configuration and on the remote endpoint string.
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

/* System library. */

#include <sys_defs.h>

/* Utility library. */

#include <msg.h>

/* Global library. */

#include <mail_params.h>

/* TLS library. */

#include <tls_proxy.h>

/* Application-specific. */

#include <postscreen.h>

 /*
  * For now, the conversion from configuration parameters to tls_server_xxx()
  * arguments is built here into the postscreen(8) source code. In the future
  * it should be abstracted into a library module that can be reused use by
  * other programs such as smtpd(8), tlsproxy(8), and smtp-sink(1).
  */

 /*
  * Pre-computed state based on configuration parameters. TODO(wietse): some
  * legacy booleans use "|=". Fix that when this code is factored out.
  */
TLS_SERVER_PARAMS psc_tls_params;
TLS_SERVER_INIT_PROPS psc_init_props;
bool    psc_tls_ready;

 /*
  * Private state.
  */
static bool psc_tls_pre_jail_done;
static int ask_client_cert;

/* psc_tls_pre_jail - pre-compute per-process TLS properties */

bool    psc_tls_pre_jail(void)
{

    /*
     * Sanity check.
     */
    if (psc_tls_pre_jail_done)
	msg_panic("%s: multiple calls", __func__);

    /*
     * XXX Temporary fix to pretend that we consistently implement TLS
     * security levels. We implement only a subset for now.
     * 
     * Note: tls_level_lookup() logs no warning.
     */
    if (var_psc_tls_level) {
	switch (tls_level_lookup(var_psc_tls_level)) {
	default:
	    msg_fatal("Invalid TLS level \"%s\"", var_psc_tls_level);
	    /* NOTREACHED */
	    break;
	case TLS_LEV_SECURE:
	case TLS_LEV_VERIFY:
	case TLS_LEV_FPRINT:
	    msg_warn("%s: unsupported TLS level \"%s\", using \"encrypt\"",
		     VAR_SMTPD_TLS_LEVEL, var_psc_tls_level);
	    /* FALLTHROUGH */
	case TLS_LEV_ENCRYPT:
	    var_psc_enforce_tls = var_psc_use_tls = 1;
	    break;
	case TLS_LEV_MAY:
	    var_psc_enforce_tls = 0;
	    var_psc_use_tls = 1;
	    break;
	case TLS_LEV_NONE:
	    var_psc_enforce_tls = var_psc_use_tls = 0;
	    break;
	}
    }
    var_psc_use_tls = var_psc_use_tls || var_psc_enforce_tls;

    if (var_psc_use_tls) {
#ifdef USE_TLS
	const char *cert_file;
	int     have_server_cert;
	int     no_server_cert_ok;
	int     require_server_cert;


	/*
	 * Can't use anonymous ciphers if we want client certificates. Must
	 * use anonymous ciphers if we have no certificates.
	 * 
	 * XXX: Ugh! Too many booleans!
	 */
	ask_client_cert = require_server_cert =
	    (var_psc_tls_ask_ccert
	     || (var_psc_enforce_tls && var_psc_tls_req_ccert));
	if (strcasecmp(var_psc_tls_cert_file, "none") == 0) {
	    no_server_cert_ok = 1;
	    cert_file = "";
	} else {
	    no_server_cert_ok = 0;
	    cert_file = var_psc_tls_cert_file;
	}
	have_server_cert = *cert_file != 0;
	have_server_cert |= *var_psc_tls_eccert_file != 0;
	have_server_cert |= *var_psc_tls_dcert_file != 0;

	if (*var_psc_tls_chain_files != 0) {
	    if (!have_server_cert)
		have_server_cert = 1;
	    else
		msg_warn("Both %s and one or more of the legacy "
			 " %s, %s or %s are non-empty; the legacy "
			 " parameters will be ignored",
			 VAR_PSC_TLS_CHAIN_FILES,
			 VAR_PSC_TLS_CERT_FILE,
			 VAR_PSC_TLS_ECCERT_FILE,
			 VAR_PSC_TLS_DCERT_FILE);
	}
	/* Some TLS configuration errors are not show stoppers. */
	if (!have_server_cert && require_server_cert)
	    msg_warn("Need a server cert to request client certs");
	if (!var_psc_enforce_tls && var_psc_tls_req_ccert)
	    msg_warn("Can't require client certs unless TLS is required");
	/* After a show-stopper error, reply with 454 to STARTTLS. */
	if (have_server_cert
	    || (no_server_cert_ok && !require_server_cert)) {

	    tls_pre_jail_init(TLS_ROLE_SERVER);
	    tls_proxy_server_param_from_config(&psc_tls_params);
	    TLS_PROXY_SERVER_INIT_PROPS(&psc_init_props,
					log_param = VAR_PSC_TLS_LOGLEVEL,
					log_level = var_psc_tls_loglevel,
					verifydepth = var_psc_tls_ccert_vd,
					cache_type = TLS_MGR_SCACHE_SMTPD,
					set_sessid = var_psc_tls_set_sessid,
				      chain_files = var_psc_tls_chain_files,
					cert_file = cert_file,
					key_file = var_psc_tls_key_file,
					dcert_file = var_psc_tls_dcert_file,
					dkey_file = var_psc_tls_dkey_file,
				      eccert_file = var_psc_tls_eccert_file,
					eckey_file = var_psc_tls_eckey_file,
					CAfile = var_psc_tls_CAfile,
					CApath = var_psc_tls_CApath,
					dh1024_param_file
					= var_psc_tls_dh1024_param_file,
					dh512_param_file
					= var_psc_tls_dh512_param_file,
					eecdh_grade = var_psc_tls_eecdh,
					protocols = var_psc_enforce_tls ?
					var_psc_tls_mand_proto :
					var_psc_tls_proto,
					ask_ccert = ask_client_cert,
					mdalg = var_psc_tls_fpt_dgst);
	    psc_tls_ready = true;
	} else {
	    msg_warn("No server certs available. TLS won't be enabled");
	}
#else
	msg_warn("TLS has been selected, but TLS support is not compiled in");
#endif
    }
    psc_tls_pre_jail_done = true;
    return (psc_tls_ready);
}

#ifdef USE_TLS

/* psc_tls_pre_start - assign per-request TLS properties */

bool    psc_tls_pre_start(const char *remote_endpt,
			          TLS_SERVER_START_PROPS *start_props)
{
    static char *cipher_grade;
    static VSTRING *cipher_exclusions;
    int     requirecert;

    if (!psc_tls_ready)
	return (false);

    /*
     * In non-wrapper mode, it is possible to require client certificate
     * verification without requiring TLS. Since certificates can be verified
     * only while TLS is turned on, this means that Postfix will happily
     * perform SMTP transactions when the client does not use the STARTTLS
     * command. For this reason, Postfix does not require client certificate
     * verification unless TLS is required.
     * 
     * The cipher grade and exclusions don't change between sessions. Compute
     * just once and cache.
     */
#define ADD_EXCLUDE(vstr, str) \
    do { \
        if (*(str)) \
            vstring_sprintf_append((vstr), "%s%s", \
                                   VSTRING_LEN(vstr) ? " " : "", (str)); \
    } while (0)

    if (cipher_grade == 0) {
	cipher_grade = var_psc_enforce_tls ?
	    var_psc_tls_mand_ciph : var_psc_tls_ciph;
	cipher_exclusions = vstring_alloc(10);
	ADD_EXCLUDE(cipher_exclusions, var_psc_tls_excl_ciph);
	if (var_psc_enforce_tls)
	    ADD_EXCLUDE(cipher_exclusions, var_psc_tls_mand_excl);
	if (ask_client_cert)
	    ADD_EXCLUDE(cipher_exclusions, "aNULL");
    }
    requirecert = (var_psc_tls_req_ccert && var_psc_enforce_tls);
    TLS_PROXY_SERVER_START_PROPS(start_props,
				 timeout = var_psc_starttls_tmout,
				 enable_rpk = var_psc_tls_enable_rpk,
				 requirecert = requirecert,
				 serverid = var_servname,
				 namaddr = remote_endpt,
				 cipher_grade = cipher_grade,
				 cipher_exclusions = STR(cipher_exclusions),
				 mdalg = var_psc_tls_fpt_dgst);
    return (true);
}

#endif
