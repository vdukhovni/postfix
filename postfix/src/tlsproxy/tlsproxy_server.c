/*++
/* NAME
/*	tlsproxy_server 3
/* SUMMARY
/*	Postfix TLS proxy server role support
/* SYNOPSIS
/*	#include <tlsproxy_server.h>
/*
/*	void	pre_jail_init_server(void)
/*Begin TODO
/*	TLS_APPL_STATE *tlsp_server_init(
/*	TLS_SERVER_PARAMS *tls_params,
/*	TLS_SERVER_INIT_PROPS *init_props)
/*End TODO
/*	int	tlsp_server_start_pre_handshake(TLSP_STATE *state)
/* DESCRIPTION
/*	This module implements TLS proxy server role support. The legacy
/*	implementation uses the same tlsproxy(8) configuration for all
/*	tls_server_init() and tls_server_start() calls.
/*
/*	pre_jail_init_server() creates an SSL context based on tlsproxy(8)
/*	server configuration.
/*Begin TODO
/*	A future version will save a copy of serialized TLS_SERVER_PARAMS
/*	and TLS_SERVER_INIT_PROPS based on tlsproxy(8) server
/*	configuration. These will be used as a reference when receiving
/*	a request for the server role.
/*
/*	tlsp_server_init() processes a request for the TLS proxy server
/*	role. If the request has not been seen before it checks the
/*	request for relevant differences that would conflict with
/*	tlsproxy(8) server configuration. The result is null when TLS
/*	is not available.
/*End TODO
/*	tlsp_server_start_pre_handshake() requests the tls_server_start()
/*	handshake. It returns TLSP_STAT_OK when the request succeeds.
/*	Otherwise, it returns TLSP_STAT_ERR and state becomes a dangling
/*	pointer.
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

 /*
  * TLS per-process status.
  * 
  * TODO(wietse) delete externally visible state after tlsp_server_init() is
  * implemented.
  */
TLS_APPL_STATE *tlsp_server_ctx;
static int ask_client_cert;
const char *server_role_disabled;

/* tlsp_server_start_pre_handshake - turn on TLS or force disconnect */

int     tlsp_server_start_pre_handshake(TLSP_STATE *state)
{
    TLS_SERVER_START_PROPS props;
    static char *cipher_grade;
    static VSTRING *cipher_exclusions;

    /*
     * The code in this routine is pasted literally from smtpd(8). I am not
     * going to sanitize this because doing so surely will break things in
     * unexpected ways.
     */

    /*
     * Perform the before-handshake portion of per-session initialization.
     * Pass a null VSTREAM to indicate that this program will do the
     * ciphertext I/O, not libtls.
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
	cipher_grade =
	    var_tlsp_enforce_tls ? var_tlsp_tls_mand_ciph : var_tlsp_tls_ciph;
	cipher_exclusions = vstring_alloc(10);
	ADD_EXCLUDE(cipher_exclusions, var_tlsp_tls_excl_ciph);
	if (var_tlsp_enforce_tls)
	    ADD_EXCLUDE(cipher_exclusions, var_tlsp_tls_mand_excl);
	if (ask_client_cert)
	    ADD_EXCLUDE(cipher_exclusions, "aNULL");
    }
    state->tls_context =
	TLS_SERVER_START(&props,
			 ctx = tlsp_server_ctx,
			 stream = (VSTREAM *) 0,/* unused */
			 fd = state->ciphertext_fd,
			 timeout = 0,		/* unused */
			 requirecert = (var_tlsp_tls_req_ccert
					&& var_tlsp_enforce_tls),
			 enable_rpk = var_tlsp_tls_enable_rpk,
			 serverid = state->server_id,
			 namaddr = state->remote_endpt,
			 cipher_grade = cipher_grade,
			 cipher_exclusions = STR(cipher_exclusions),
			 mdalg = var_tlsp_tls_fpt_dgst);

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

/* pre_jail_init_server - pre-jail initialization */

void    pre_jail_init_server(void)
{
    TLS_SERVER_INIT_PROPS props;
    const char *cert_file;
    int     have_server_cert;
    int     no_server_cert_ok;
    int     require_server_cert;

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
	server_role_disabled = "TLS server role is disabled by configuration";
	return;
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

	tls_pre_jail_init(TLS_ROLE_SERVER);

	/*
	 * Large parameter lists are error-prone, so we emulate a language
	 * feature that C does not have natively: named parameter lists.
	 */
	tlsp_server_ctx =
	    TLS_SERVER_INIT(&props,
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
    } else {
	msg_warn("No server certs available. TLS can't be enabled");
    }

    /*
     * To maintain sanity, allow partial SSL_write() operations, and allow
     * SSL_write() buffer pointers to change after a WANT_READ or WANT_WRITE
     * result. This is based on OpenSSL developers talking on a mailing list,
     * but is not supported by documentation. If this code stops working then
     * no-one can be held responsible.
     */
    if (tlsp_server_ctx)
	SSL_CTX_set_mode(tlsp_server_ctx->ssl_ctx,
			 SSL_MODE_ENABLE_PARTIAL_WRITE
			 | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
}

#endif
