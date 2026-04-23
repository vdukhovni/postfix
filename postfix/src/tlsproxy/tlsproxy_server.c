/*++
/* NAME
/*	tlsproxy_server 3
/* SUMMARY
/*	Postfix TLS proxy server role support
/* SYNOPSIS
/*	#include <tlsproxy_server.h>
/*
/*	bool	pre_jail_init_server(void)
/*
/*	TLS_APPL_STATE *tlsp_server_init(
/*	TLS_SERVER_PARAMS *tls_params,
/*	TLS_SERVER_INIT_PROPS *init_props)
/*
/*	int	tlsp_server_start_pre_handshake(TLSP_STATE *state)
/* DESCRIPTION
/*	This module implements TLS proxy server role support. The legacy
/*	implementation uses the same tlsproxy(8) configuration for all
/*	tls_server_init() and tls_server_start() calls.
/*
/*	pre_jail_init_server() creates an SSL context based on local
/*	tlsproxy(8) server configuration, and creates TLS_SERVER_PARAMS
/*	and TLS_SERVER_INIT_PROPS objects that will be used as a reference
/*	when receiving a remote request for the server role. The result
/*	is true if successful.
/*
/*	tlsp_server_init() processes a request for the TLS proxy server
/*	role. If the request has not been seen before, it checks the
/*	request for relevant differences that would conflict with
/*	tlsproxy(8) server configuration. The result is null when TLS
/*	is not available.
/*
/*	tlsp_server_start_pre_handshake() requests the tls_server_start()
/*	handshake. It returns TLSP_STAT_OK when the request succeeds.
/*	Otherwise, it destroys the state, and returns TLSP_STAT_ERR.
/* DIAGNOSTICS
/*	Problems are logged to \fBsyslogd\fR(8) or \fBpostlogd\fR(8).
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* HISTORY
/* .ad
/* .fi
/*	This service was introduced with Postfix version 2.8.
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

 /*
  * System library.
  */
#include <sys_defs.h>
#include <errno.h>

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

 /*
  * Utility library.
  */
#include <msg.h>
#include <mymalloc.h>

 /*
  * Global library.
  */
#include <been_here.h>
#include <mail_params.h>

 /*
  * TLS library.
  */
#ifdef USE_TLS
#define TLS_INTERNAL			/* XXX */
#include <tls.h>
#include <tls_proxy.h>

 /*
  * Application-specific.
  */
#include <tlsproxy.h>
#include <tlsproxy_server.h>
#include <tlsproxy_diff.h>

 /*
  * TLS per-process status.
  */
static int ask_client_cert;		/* move to pre-jail code? */
static int tlsp_pre_jail_server_done;
static char *tlsp_pre_jail_server_param_key;	/* pre-jail global params */
static char *tlsp_pre_jail_server_init_key;	/* pre-jail init props */

 /*
  * TLS per-server status.
  */
static HTABLE *tlsp_server_app_cache;
static BH_TABLE *tlsp_server_params_nag_filter;

/* tlsp_server_start_pre_handshake - turn on TLS or force disconnect */

int     tlsp_server_start_pre_handshake(TLSP_STATE *state)
{
    state->server_start_props->ctx = state->appl_state;
    state->server_start_props->fd = state->ciphertext_fd;
    state->tls_context = tls_server_start(state->server_start_props);
    if (state->tls_context == 0) {
	tlsp_state_free(state);
	return (TLSP_STAT_ERR);
    }

    /*
     * XXX Do we care about TLS session rate limits? Good postscreen(8)
     * clients will occasionally require the tlsproxy to renew their
     * allowlist status, but bad clients hammering the server can suck up
     * lots of CPU cycles. Per-client concurrency limits in postscreen(8)
     * will divert only naive security "researchers".
     */
    return (TLSP_STAT_OK);
}

/* tlsp_server_init - initialize a TLS server engine */

TLS_APPL_STATE *tlsp_server_init(TLS_SERVER_PARAMS *tls_params,
				         TLS_SERVER_INIT_PROPS *init_props)
{
    TLS_APPL_STATE *appl_state;
    VSTRING *param_buf;
    char   *param_key;
    VSTRING *init_buf;
    char   *init_key;
    int     log_hints = 0;
    const char *saved_log_param;

    /*
     * Use one TLS_APPL_STATE object for all requests that specify the same
     * TLS_SERVER_INIT_PROPS. Each TLS_APPL_STATE owns an SSL_CTX, which is
     * expensive to create. Bug: TLS_SERVER_PARAMS are not used when creating
     * a TLS_APPL_STATE instance.
     * 
     * First, compute the TLS_APPL_STATE cache lookup key. Save a copy of the
     * pre-jail request TLS_SERVER_PARAMS and TLSPROXY_SERVER_INIT_PROPS
     * settings, so that we can detect post-jail requests that do not match.
     * 
     * For TLS_APPL_STATE cache lookup, ignore harmless differences in
     * xxx_tls_loglevel parameter names. They don't affect program behavior.
     */
    param_buf = vstring_alloc(100);
    param_key = tls_proxy_server_param_serialize(attr_print_plain, param_buf,
						 tls_params);

    init_buf = vstring_alloc(100);
    saved_log_param = init_props->log_param;
    init_props->log_param = "dummy";
    init_key = tls_proxy_server_init_serialize(attr_print_plain, init_buf,
					       init_props);
    init_props->log_param = saved_log_param;

#define TLSP_SERVER_INIT_RETURN(retval) do { \
        vstring_free(init_buf); \
        vstring_free(param_buf); \
        return (retval); \
    } while (0)

    if (tlsp_pre_jail_server_done == 0) {
	if (tlsp_pre_jail_server_param_key == 0
	    || tlsp_pre_jail_server_init_key == 0) {
	    tlsp_pre_jail_server_param_key = mystrdup(param_key);
	    tlsp_pre_jail_server_init_key = mystrdup(init_key);
	} else if (strcmp(tlsp_pre_jail_server_param_key, param_key) != 0
		   || strcmp(tlsp_pre_jail_server_init_key, init_key) != 0) {
	    msg_panic("tlsp_server_init: too many pre-jail calls");
	}
    }

    /*
     * Log a warning if a post-jail request uses unexpected TLS_SERVER_PARAMS
     * settings. Bug: TLS_SERVER_PARAMS settings are not used when creating a
     * TLS_APPL_STATE instance; this makes a mismatch of TLS_SERVER_PARAMS
     * settings problematic.
     */
    else if (tlsp_pre_jail_server_param_key == 0
	     || tlsp_pre_jail_server_init_key == 0) {
	msg_warn("TLS server role is disabled by configuration");
	TLSP_SERVER_INIT_RETURN(0);
    } else if (!been_here_fixed(tlsp_server_params_nag_filter, param_key)
	       && strcmp(tlsp_pre_jail_server_param_key, param_key) != 0) {
	msg_warn("request from tlsproxy client with unexpected settings");
	tlsp_log_config_diff(tlsp_pre_jail_server_param_key, param_key);
	log_hints = 1;
    }

    /*
     * Look up the cached TLS_APPL_STATE for this tls_server_init request.
     */
    if ((appl_state = (TLS_APPL_STATE *)
	 htable_find(tlsp_server_app_cache, init_key)) == 0) {

	/*
	 * Before creating a TLS_APPL_STATE instance, log a warning if a
	 * post-jail request differs from the saved pre-jail request AND the
	 * post-jail request specifies file/directory pathname arguments.
	 * Unexpected requests containing pathnames are problematic after
	 * chroot (pathname resolution) and after dropping privileges (key
	 * files must be root read-only). Unexpected requests are not a
	 * problem as long as they contain no pathnames (for example a
	 * tls_loglevel change).
	 * 
	 * We could eliminate some of this complication by adding code that
	 * opens a cert/key lookup table at pre-jail time, and by reading
	 * cert/key info on-the-fly from that table. But then all requests
	 * would still have to specify the same table.
	 */
#define NOT_EMPTY(x) ((x) && *(x))

	if (tlsp_pre_jail_server_done
	    && strcmp(tlsp_pre_jail_server_init_key, init_key) != 0
	    && (NOT_EMPTY(init_props->chain_files)
		|| NOT_EMPTY(init_props->cert_file)
		|| NOT_EMPTY(init_props->key_file)
		|| NOT_EMPTY(init_props->dcert_file)
		|| NOT_EMPTY(init_props->dkey_file)
		|| NOT_EMPTY(init_props->eccert_file)
		|| NOT_EMPTY(init_props->eckey_file)
		|| NOT_EMPTY(init_props->CAfile)
		|| NOT_EMPTY(init_props->CApath))) {
	    msg_warn("request from tlsproxy server with unexpected settings");
	    tlsp_log_config_diff(tlsp_pre_jail_server_init_key, init_key);
	    log_hints = 1;
	}
    }
    if (log_hints)
	msg_warn("to avoid this warning, 1) identify the tlsproxy "
		 "server that is making this request, 2) configure "
		 "a custom tlsproxy service with settings that "
		 "match that tlsproxy server, and 3) configure "
		 "that tlsproxy server with a tlsproxy_service_name "
		 "setting that resolves to that custom tlsproxy "
		 "service");

    /*
     * TLS_APPL_STATE creation may fail when a post-jail request specifies
     * unexpected cert/key information, but that is OK because we already
     * logged a warning with configuration suggestions.
     */
    if (appl_state == 0 && (appl_state = tls_server_init(init_props)) != 0) {
	(void) htable_enter(tlsp_server_app_cache, init_key,
			    (void *) appl_state);

	/*
	 * To maintain sanity, allow partial SSL_write() operations, and
	 * allow SSL_write() buffer pointers to change after a WANT_READ or
	 * WANT_WRITE result. This is based on OpenSSL developers talking on
	 * a mailing list, but is not supported by documentation. If this
	 * code stops working then no-one can be held responsible.
	 */
	SSL_CTX_set_mode(appl_state->ssl_ctx,
			 SSL_MODE_ENABLE_PARTIAL_WRITE
			 | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    }
    TLSP_SERVER_INIT_RETURN(appl_state);
}

/* pre_jail_init_server - pre-jail initialization */

bool    pre_jail_init_server(void)
{
    const char *cert_file;
    int     have_server_cert;
    int     no_server_cert_ok;
    int     require_server_cert;

    /*
     * TODO(wietse): simplify module initialization state and module error
     * state (too many booleans).
     */
    bool    ret = false;

    /*
     * Sanity check.
     */
    if (tlsp_pre_jail_server_done)
	msg_panic("%s: multiple calls", __func__);

    /*
     * The cache with TLS_APPL_STATE instances for different TLS_CLIENT_INIT
     * configurations.
     */
    tlsp_server_app_cache = htable_create(10);

    /*
     * The code in this routine is pasted literally from smtpd(8). I am not
     * going to sanitize this because doing so surely will break things in
     * unexpected ways.
     */
    if (*var_tlsp_tls_level) {
	switch (tls_level_lookup(var_tlsp_tls_level)) {
	default:
	    msg_fatal("Invalid TLS level \"%s\"", var_tlsp_tls_level);
	    /* NOTREACHED */
	    break;
	case TLS_LEV_SECURE:
	case TLS_LEV_VERIFY:
	case TLS_LEV_FPRINT:
	    msg_warn("%s: unsupported TLS level \"%s\", using \"encrypt\"",
		     VAR_TLSP_TLS_LEVEL, var_tlsp_tls_level);
	    /* FALLTHROUGH */
	case TLS_LEV_ENCRYPT:
	    var_tlsp_enforce_tls = var_tlsp_use_tls = 1;
	    break;
	case TLS_LEV_MAY:
	    var_tlsp_enforce_tls = 0;
	    var_tlsp_use_tls = 1;
	    break;
	case TLS_LEV_NONE:
	    var_tlsp_enforce_tls = var_tlsp_use_tls = 0;
	    break;
	}
    }
    var_tlsp_use_tls = var_tlsp_use_tls || var_tlsp_enforce_tls;
    if (!var_tlsp_use_tls) {
	tlsp_pre_jail_server_done = 1;
	return (false);
    }

    /*
     * Load TLS keys before dropping privileges.
     * 
     * Can't use anonymous ciphers if we want client certificates. Must use
     * anonymous ciphers if we have no certificates.
     */
    ask_client_cert = require_server_cert =
	(var_tlsp_tls_ask_ccert
	 || (var_tlsp_enforce_tls && var_tlsp_tls_req_ccert));
    if (strcasecmp(var_tlsp_tls_cert_file, "none") == 0) {
	no_server_cert_ok = 1;
	cert_file = "";
    } else {
	no_server_cert_ok = 0;
	cert_file = var_tlsp_tls_cert_file;
    }
    have_server_cert =
	(*cert_file || *var_tlsp_tls_dcert_file || *var_tlsp_tls_eccert_file);

    if (*var_tlsp_tls_chain_files != 0) {
	if (!have_server_cert)
	    have_server_cert = 1;
	else
	    msg_warn("Both %s and one or more of the legacy "
		     " %s, %s or %s are non-empty; the legacy "
		     " parameters will be ignored",
		     VAR_TLSP_TLS_CHAIN_FILES,
		     VAR_TLSP_TLS_CERT_FILE,
		     VAR_TLSP_TLS_ECCERT_FILE,
		     VAR_TLSP_TLS_DCERT_FILE);
    }
    /* Some TLS configuration errors are not show stoppers. */
    if (!have_server_cert && require_server_cert)
	msg_warn("Need a server cert to request client certs");
    if (!var_tlsp_enforce_tls && var_tlsp_tls_req_ccert)
	msg_warn("Can't require client certs unless TLS is required");
    /* After a show-stopper error, log a warning. */
    if (have_server_cert || (no_server_cert_ok && !require_server_cert)) {
	TLS_SERVER_PARAMS tls_params;
	TLS_SERVER_INIT_PROPS init_props;

	tls_pre_jail_init(TLS_ROLE_SERVER);

	/*
	 * Large parameter lists are error-prone, so we emulate a language
	 * feature that C does not have natively: named parameter lists.
	 */
	(void) tls_proxy_server_param_from_config(&tls_params);
	(void) TLS_SERVER_INIT_ARGS(&init_props,
				    log_param = VAR_TLSP_TLS_LOGLEVEL,
				    log_level = var_tlsp_tls_loglevel,
				    verifydepth = var_tlsp_tls_ccert_vd,
				    cache_type = TLS_MGR_SCACHE_SMTPD,
				    set_sessid = var_tlsp_tls_set_sessid,
				    chain_files = var_tlsp_tls_chain_files,
				    cert_file = cert_file,
				    key_file = var_tlsp_tls_key_file,
				    dcert_file = var_tlsp_tls_dcert_file,
				    dkey_file = var_tlsp_tls_dkey_file,
				    eccert_file = var_tlsp_tls_eccert_file,
				    eckey_file = var_tlsp_tls_eckey_file,
				    CAfile = var_tlsp_tls_CAfile,
				    CApath = var_tlsp_tls_CApath,
				    dh1024_param_file
				    = var_tlsp_tls_dh1024_param_file,
				    dh512_param_file
				    = var_tlsp_tls_dh512_param_file,
				    eecdh_grade = var_tlsp_tls_eecdh,
				    protocols = var_tlsp_enforce_tls ?
				    var_tlsp_tls_mand_proto :
				    var_tlsp_tls_proto,
				    ask_ccert = ask_client_cert,
				    mdalg = var_tlsp_tls_fpt_dgst);
	if (tlsp_server_init(&tls_params, &init_props) == 0)
	    msg_warn("TLS server initialization failed");
	else
	    ret = true;
    } else {
	msg_warn("No server certs available. TLS can't be enabled");
    }

    /*
     * Bug: TLS_SERVER_PARAMS attributes are not used when creating a
     * TLS_APPL_STATE instance; we can only warn about attribute mismatches.
     */
    tlsp_server_params_nag_filter = been_here_init(BH_BOUND_NONE, BH_FLAG_NONE);

    /*
     * Any of the static global variables would suffice, but this is more
     * explicit.
     */
    tlsp_pre_jail_server_done = 1;

    return (ret);
}

#endif
