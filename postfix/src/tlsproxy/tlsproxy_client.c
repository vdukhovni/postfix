/*++
/* NAME
/*	tlsproxy_client 3
/* SUMMARY
/*	Postfix TLS proxy client role support
/* SYNOPSIS
/*	#include <tlsproxy_client.h>
/*
/*	bool	tlsp_pre_jail_client_init(void)
/*
/*	TLS_APPL_STATE *tlsp_client_init(
/*	TLS_CLIENT_PARAMS *tls_params,
/*	TLS_CLIENT_INIT_PROPS *init_props)
/*
/*	int	tlsp_client_start_pre_handshake(TLSP_STATE *state)
/* DESCRIPTION
/*	This module implements TLS proxy client role support.
/*
/*	tlsp_pre_jail_client_init() creates an SSL context based on local
/*	tlsproxy(8) client configuration, and populates TLS_CLIENT_PARAMS
/*	and TLS_CLIENT_INIT_PROPS objects that will be used as a reference
/*	when receiving a remote request for the client role. The result
/*	is true if successful.
/*
/*	tlsp_client_init() processes a request for the TLS proxy client
/*	role. If the request has not been seen before it checks the
/*	request for relevant differences that would conflict with
/*	tlsproxy(8) client configuration. The result is null when
/*	TLS is not available.
/*
/*	tlsp_client_start_pre_handshake() requests the tls_client_start()
/*	handshake. It returns TLSP_STAT_OK when the request succeeds.
/*	Otherwise, it destroys the state and returns TLSP_STAT_ERR.
/* DIAGNOSTICS
/*	Problems and transactions are logged to \fBsyslogd\fR(8)
/*	or \fBpostlogd\fR(8).
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
#include <mail_proto.h>
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
#include <tlsproxy_client.h>
#include <tlsproxy_diff.h>

 /*
  * TLS per-process status.
  */
static bool tlsp_pre_jail_client_done;
static char *tlsp_pre_jail_client_param_key;	/* pre-jail global params */
static char *tlsp_pre_jail_client_init_key;	/* pre-jail init props */

 /*
  * TLS per-client status.
  */
static HTABLE *tlsp_client_app_cache;	/* per-client init props */
static BH_TABLE *tlsp_client_params_nag_filter;	/* per-client nag filter */

/* tlsp_client_start_pre_handshake - turn on TLS or force disconnect */

int     tlsp_client_start_pre_handshake(TLSP_STATE *state)
{
    state->client_start_props->ctx = state->appl_state;
    state->client_start_props->fd = state->ciphertext_fd;
    state->tls_context = tls_client_start(state->client_start_props);
    if (state->tls_context != 0)
	return (TLSP_STAT_OK);

    tlsp_state_free(state);
    return (TLSP_STAT_ERR);
}

/* tlsp_client_init - initialize a TLS client engine */

TLS_APPL_STATE *tlsp_client_init(TLS_CLIENT_PARAMS *tls_params,
				         TLS_CLIENT_INIT_PROPS *init_props)
{
    TLS_APPL_STATE *appl_state;
    VSTRING *param_buf;
    char   *param_key;
    VSTRING *init_buf;
    char   *init_key;
    int     log_hints = 0;

    /*
     * Use one TLS_APPL_STATE object for all requests that specify the same
     * TLS_CLIENT_INIT_PROPS. Each TLS_APPL_STATE owns an SSL_CTX, which is
     * expensive to create. Bug: TLS_CLIENT_PARAMS are not used when creating
     * a TLS_APPL_STATE instance.
     * 
     * First, compute the TLS_APPL_STATE cache lookup key. Save a copy of the
     * pre-jail request TLS_CLIENT_PARAMS and TLSPROXY_CLIENT_INIT_PROPS
     * settings, so that we can detect post-jail requests that do not match.
     */
    param_buf = vstring_alloc(100);
    param_key = tls_proxy_client_param_serialize(attr_print_plain, param_buf,
						 tls_params);
    init_buf = vstring_alloc(100);
    init_key = tls_proxy_client_init_serialize(attr_print_plain, init_buf,
					       init_props);
#define TLSP_CLIENT_INIT_RETURN(retval) do { \
	vstring_free(init_buf); \
	vstring_free(param_buf); \
	return (retval); \
    } while (0)

    if (tlsp_pre_jail_client_done == 0) {
	if (tlsp_pre_jail_client_param_key == 0
	    || tlsp_pre_jail_client_init_key == 0) {
	    tlsp_pre_jail_client_param_key = mystrdup(param_key);
	    tlsp_pre_jail_client_init_key = mystrdup(init_key);
	} else if (strcmp(tlsp_pre_jail_client_param_key, param_key) != 0
		   || strcmp(tlsp_pre_jail_client_init_key, init_key) != 0) {
	    msg_panic("tlsp_client_init: too many pre-jail calls");
	}
    }

    /*
     * Log a warning if a post-jail request uses unexpected TLS_CLIENT_PARAMS
     * settings. Bug: TLS_CLIENT_PARAMS settings are not used when creating a
     * TLS_APPL_STATE instance; this makes a mismatch of TLS_CLIENT_PARAMS
     * settings problematic.
     */
    else if (tlsp_pre_jail_client_param_key == 0
	     || tlsp_pre_jail_client_init_key == 0) {
	msg_warn("TLS client role is disabled by configuration");
	TLSP_CLIENT_INIT_RETURN(0);
    } else if (!been_here_fixed(tlsp_client_params_nag_filter, param_key)
	       && strcmp(tlsp_pre_jail_client_param_key, param_key) != 0) {
	msg_warn("request from tlsproxy client with unexpected settings");
	tlsp_log_config_diff(tlsp_pre_jail_client_param_key, param_key);
	log_hints = 1;
    }

    /*
     * Look up the cached TLS_APPL_STATE for this tls_client_init request.
     */
    if ((appl_state = (TLS_APPL_STATE *)
	 htable_find(tlsp_client_app_cache, init_key)) == 0) {

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

	if (tlsp_pre_jail_client_done
	    && strcmp(tlsp_pre_jail_client_init_key, init_key) != 0
	    && (NOT_EMPTY(init_props->chain_files)
		|| NOT_EMPTY(init_props->cert_file)
		|| NOT_EMPTY(init_props->key_file)
		|| NOT_EMPTY(init_props->dcert_file)
		|| NOT_EMPTY(init_props->dkey_file)
		|| NOT_EMPTY(init_props->eccert_file)
		|| NOT_EMPTY(init_props->eckey_file)
		|| NOT_EMPTY(init_props->CAfile)
		|| NOT_EMPTY(init_props->CApath))) {
	    msg_warn("request from tlsproxy client with unexpected settings");
	    tlsp_log_config_diff(tlsp_pre_jail_client_init_key, init_key);
	    log_hints = 1;
	}
    }
    if (log_hints)
	msg_warn("to avoid this warning, 1) identify the tlsproxy "
		 "client that is making this request, 2) configure "
		 "a custom tlsproxy service with settings that "
		 "match that tlsproxy client, and 3) configure "
		 "that tlsproxy client with a tlsproxy_service_name "
		 "setting that resolves to that custom tlsproxy "
		 "service");

    /*
     * TLS_APPL_STATE creation may fail when a post-jail request specifies
     * unexpected cert/key information, but that is OK because we already
     * logged a warning with configuration suggestions.
     */
    if (appl_state == 0
	&& (appl_state = tls_client_init(init_props)) != 0) {
	(void) htable_enter(tlsp_client_app_cache, init_key,
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
    TLSP_CLIENT_INIT_RETURN(appl_state);
}

/* tlsp_pre_jail_client_init - pre-jail initialization */

bool    tlsp_pre_jail_client_init(void)
{
    int     clnt_use_tls;

    /*
     * TODO(wietse):  simplify module initialization state and module error
     * state.
     */
    bool    ret = false;

    /*
     * Sanity check.
     */
    if (tlsp_pre_jail_client_done)
	msg_panic("%s: multiple calls", __func__);

    /*
     * The cache with TLS_APPL_STATE instances for different TLS_CLIENT_INIT
     * configurations.
     */
    tlsp_client_app_cache = htable_create(10);

    /* Postfix <= 3.10 backwards compatibility. */
    if (warn_compat_break_tlsp_clnt_level
	&& warn_compat_break_smtp_tls_level)
	msg_info("using backwards-compatible default setting "
		 VAR_TLSP_CLNT_LEVEL "=(empty)");

    /*
     * Most sites don't use TLS client certs/keys. In that case, enabling
     * tlsproxy-based connection caching is trivial.
     * 
     * But some sites do use TLS client certs/keys, and that is challenging when
     * tlsproxy runs in a post-jail environment: chroot breaks pathname
     * resolution, and an unprivileged process should not be able to open
     * files with secrets. The workaround: assume that most of those sites
     * will use a fixed TLS client identity. In that case, tlsproxy can load
     * the corresponding certs/keys at pre-jail time, so that secrets can
     * remain read-only for root. As long as the tlsproxy pre-jail TLS client
     * configuration with cert or key pathnames is the same as the one used
     * in the Postfix SMTP client, sites can selectively or globally enable
     * tlsproxy-based connection caching without additional TLS
     * configuration.
     * 
     * Loading one TLS client configuration at pre-jail time is not sufficient
     * for the minority of sites that want to use TLS connection caching with
     * multiple TLS client identities. To alert the operator, tlsproxy will
     * log a warning when a TLS_CLIENT_INIT message specifies a different
     * configuration than the tlsproxy pre-jail client configuration, and
     * that different configuration specifies file/directory pathname
     * arguments. The workaround is to have one tlsproxy process per TLS
     * client identity.
     * 
     * The general solution for single-identity or multi-identity clients is to
     * stop loading certs and keys from individual files. Instead, have a
     * cert/key map, indexed by client identity, read-only by root. After
     * opening the map as root at pre-jail time, tlsproxy can read certs/keys
     * on-the-fly as an unprivileged process at post-jail time. This is the
     * approach that was already proposed for server-side SNI support, and it
     * could be reused here. It would also end the proliferation of RSA
     * cert/key parameters, DSA cert/key parameters, EC cert/key parameters,
     * and so on.
     * 
     * Horror: In order to create the same pre-jail TLS client context as the
     * one used in the Postfix SMTP client, we have to duplicate intricate
     * SMTP client code, including a handful configuration parameters that
     * tlsproxy does not need. We must duplicate the logic, so that we only
     * load certs and keys when the SMTP client would load them.
     */
    if (*var_tlsp_clnt_level != 0)
	switch (tls_level_lookup(var_tlsp_clnt_level)) {
	case TLS_LEV_SECURE:
	case TLS_LEV_VERIFY:
	case TLS_LEV_DANE_ONLY:
	case TLS_LEV_FPRINT:
	case TLS_LEV_ENCRYPT:
	    var_tlsp_clnt_use_tls = var_tlsp_clnt_enforce_tls = 1;
	    break;
	case TLS_LEV_DANE:
	case TLS_LEV_MAY:
	    var_tlsp_clnt_use_tls = 1;
	    var_tlsp_clnt_enforce_tls = 0;
	    break;
	case TLS_LEV_NONE:
	    var_tlsp_clnt_use_tls = var_tlsp_clnt_enforce_tls = 0;
	    break;
	default:
	    /* tls_level_lookup() logs no warning. */
	    /* session_tls_init() assumes that var_tlsp_clnt_level is sane. */
	    msg_fatal("Invalid TLS level \"%s\"", var_tlsp_clnt_level);
	}
    clnt_use_tls = (var_tlsp_clnt_use_tls || var_tlsp_clnt_enforce_tls);

    /*
     * Initialize the TLS data before entering the chroot jail.
     */
    if (clnt_use_tls || var_tlsp_clnt_per_site[0] || var_tlsp_clnt_policy[0]) {
	TLS_CLIENT_PARAMS tls_params;
	TLS_CLIENT_INIT_PROPS init_props;

	tls_pre_jail_init(TLS_ROLE_CLIENT);

	/*
	 * We get stronger type safety and a cleaner interface by combining
	 * the various parameters into a single tls_client_props structure.
	 * 
	 * Large parameter lists are error-prone, so we emulate a language
	 * feature that C does not have natively: named parameter lists.
	 */
	(void) tls_proxy_client_param_from_config(&tls_params);
	(void) TLS_CLIENT_INIT_ARGS(&init_props,
				    log_param = var_tlsp_clnt_logparam,
				    log_level = var_tlsp_clnt_loglevel,
				    verifydepth = var_tlsp_clnt_scert_vd,
				    cache_type = TLS_MGR_SCACHE_SMTP,
				    chain_files = var_tlsp_clnt_chain_files,
				    cert_file = var_tlsp_clnt_cert_file,
				    key_file = var_tlsp_clnt_key_file,
				    dcert_file = var_tlsp_clnt_dcert_file,
				    dkey_file = var_tlsp_clnt_dkey_file,
				    eccert_file = var_tlsp_clnt_eccert_file,
				    eckey_file = var_tlsp_clnt_eckey_file,
				    CAfile = var_tlsp_clnt_CAfile,
				    CApath = var_tlsp_clnt_CApath,
				    mdalg = var_tlsp_clnt_fpt_dgst);
	if (tlsp_client_init(&tls_params, &init_props) == 0)
	    msg_warn("TLS client initialization failed");
	else
	    ret = true;
    }

    /*
     * Bug: TLS_CLIENT_PARAMS attributes are not used when creating a
     * TLS_APPL_STATE instance; we can only warn about attribute mismatches.
     */
    tlsp_client_params_nag_filter = been_here_init(BH_BOUND_NONE, BH_FLAG_NONE);

    /*
     * Any of the static global variables would suffice, but this is more
     * explicit.
     */
    tlsp_pre_jail_client_done = 1;

    return (ret);
}

#endif
