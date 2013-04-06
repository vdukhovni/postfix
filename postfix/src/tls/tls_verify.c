/*++
/* NAME
/*	tls_verify 3
/* SUMMARY
/*	peer name and peer certificate verification
/* SYNOPSIS
/*	#define TLS_INTERNAL
/*	#include <tls.h>
/*
/*	int	tls_cert_match(TLSContext, usage, cert, depth)
/*	TLS_SESS_STATE *TLScontext;
/*	int	usage;
/*	X509	*cert;
/*	int	depth;
/*
/*	int	tls_verify_certificate_callback(ok, ctx)
/*	int	ok;
/*	X509_STORE_CTX *ctx;
/*
/*	int     tls_log_verify_error(TLScontext)
/*	TLS_SESS_STATE *TLScontext;
/*
/*	char *tls_peer_CN(peercert, TLScontext)
/*	X509   *peercert;
/*	TLS_SESS_STATE *TLScontext;
/*
/*	char *tls_issuer_CN(peercert, TLScontext)
/*	X509   *peercert;
/*	TLS_SESS_STATE *TLScontext;
/*
/*	const char *tls_dns_name(gn, TLScontext)
/*	const GENERAL_NAME *gn;
/*	TLS_SESS_STATE *TLScontext;
/* DESCRIPTION
/*	tls_cert_match() matches the full and/or public key digest of
/*	"cert" against each candidate digest in TLScontext->dane. If usage
/*	is TLS_DANE_EE, the match is against end-entity digests, otherwise
/*	it is against trust-anchor digests.  Returns true if a match is found,
/*	false otherwise.
/*
/*	tls_verify_certificate_callback() is called several times (directly
/*	or indirectly) from crypto/x509/x509_vfy.c. It collects errors
/*	and trust information at each element of the trust chain.
/*	The last call at depth 0 sets the verification status based
/*	on the cumulative winner (lowest depth) of errors vs. trust.
/*	We always return 1 (continue the handshake) and handle trust
/*	and peer-name verification problems at the application level.
/*
/*	tls_log_verify_error() (called only when we care about the
/*	peer certificate, that is not when opportunistic) logs the
/*	reason why the certificate failed to be verified.
/*
/*	tls_peer_CN() returns the text CommonName for the peer
/*	certificate subject, or an empty string if no CommonName was
/*	found. The result is allocated with mymalloc() and must be
/*	freed by the caller; it contains UTF-8 without non-printable
/*	ASCII characters.
/*
/*	tls_issuer_CN() returns the text CommonName for the peer
/*	certificate issuer, or an empty string if no CommonName was
/*	found. The result is allocated with mymalloc() and must be
/*	freed by the caller; it contains UTF-8 without non-printable
/*	ASCII characters.
/*
/*	tls_dns_name() returns the string value of a GENERAL_NAME
/*	from a DNS subjectAltName extension. If non-printable characters
/*	are found, a null string is returned instead. Further sanity
/*	checks may be added if the need arises.
/*
/*	Arguments:
/* .IP ok
/*	Result of prior verification: non-zero means success.  In
/*	order to reduce the noise level, some tests or error reports
/*	are disabled when verification failed because of some
/*	earlier problem.
/* .IP ctx
/*	SSL application context. This links to the Postfix TLScontext
/*	with enforcement and logging options.
/* .IP gn
/*	An OpenSSL GENERAL_NAME structure holding a DNS subjectAltName
/*	to be decoded and checked for validity.
/* .IP usage
/*	Trust anchor (TLS_DANE_TA) or end-entity (TLS_DANE_EE) digests?
/* .IP cert
/*	Certificate from peer trust chain (CA or leaf server).
/* .IP depth
/*	The certificate depth for logging.
/* .IP peercert
/*	Server or client X.509 certificate.
/* .IP TLScontext
/*	Server or client context for warning messages.
/* DIAGNOSTICS
/*	tls_peer_CN(), tls_issuer_CN() and tls_dns_name() log a warning
/*	when 1) the requested information is not available in the specified
/*	certificate, 2) the result exceeds a fixed limit, 3) the result
/*	contains NUL characters or the result contains non-printable or
/*	non-ASCII characters.
/* LICENSE
/* .ad
/* .fi
/*	This software is free. You can do with it whatever you want.
/*	The original author kindly requests that you acknowledge
/*	the use of his software.
/* AUTHOR(S)
/*	Originally written by:
/*	Lutz Jaenicke
/*	BTU Cottbus
/*	Allgemeine Elektrotechnik
/*	Universitaetsplatz 3-4
/*	D-03044 Cottbus, Germany
/*
/*	Updated by:
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*
/*	Victor Duchovni
/*	Morgan Stanley
/*--*/

/* System library. */

#include <sys_defs.h>
#include <ctype.h>

#ifdef USE_TLS
#include <string.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <stringops.h>

/* TLS library. */

#define TLS_INTERNAL
#include <tls.h>

/* update_error_state - safely stash away error state */

static void update_error_state(TLS_SESS_STATE *TLScontext, int depth,
			               X509 *errorcert, int errorcode)
{
    /* No news is good news */
    if ((TLScontext->trustdepth >= 0 && TLScontext->trustdepth < depth) ||
	(TLScontext->errordepth >= 0 && TLScontext->errordepth <= depth))
	return;

    /*
     * The certificate pointer is stable during the verification callback,
     * but may be freed after the callback returns.  Since we delay error
     * reporting till later, we bump the refcount so we can rely on it still
     * being there until later.
     */
    if (TLScontext->errorcert != 0)
	X509_free(TLScontext->errorcert);
    if (errorcert != 0)
	CRYPTO_add(&errorcert->references, 1, CRYPTO_LOCK_X509);
    TLScontext->errorcert = errorcert;
    TLScontext->errorcode = errorcode;

    /*
     * Maintain an invariant, at most one of errordepth and trustdepth
     * is non-negative at any given time.
     */
    TLScontext->errordepth = depth;
    TLScontext->trustdepth = -1;
}

/* update_trust_state - safely stash away trust state */

static void update_trust_state(TLS_SESS_STATE *TLScontext, int depth)
{
    /* No news is bad news */
    if ((TLScontext->trustdepth >= 0 && TLScontext->trustdepth <= depth)
        || (TLScontext->errordepth >= 0 && TLScontext->errordepth <= depth))
	return;

    /*
     * Maintain an invariant, at most one of errordepth and trustdepth
     * is non-negative at any given time.
     */
    TLScontext->trustdepth = depth;
    TLScontext->errordepth = -1;
}

/* tls_cert_match - match cert against given list of TA or EE digests */

int tls_cert_match(TLS_SESS_STATE *TLScontext, int usage, X509 *cert, int depth)
{
    const TLS_DANE *dane = TLScontext->dane;
    TLS_TLSA *tlsa = (usage == TLS_DANE_EE) ? dane->ee : dane->ta;
    const char *namaddr = TLScontext->namaddr;
    const char *ustr = (usage == TLS_DANE_EE) ? "end entity" : "trust anchor";
    int	    mixed = (dane->flags & TLS_DANE_FLAG_MIXED);
    int     matched;

    for (matched = 0; tlsa && !matched; tlsa = tlsa->next) {
	char  **dgst;
	ARGV   *certs;

	if (tlsa->pkeys) {
	    char   *pkey_dgst = tls_pkey_fprint(cert, tlsa->mdalg);
	    for (dgst = tlsa->pkeys->argv; !matched && *dgst; ++dgst)
		if (strcasecmp(pkey_dgst, *dgst) == 0)
		    matched = 1;
	    if (TLScontext->log_mask & (TLS_LOG_VERBOSE|TLS_LOG_CERTMATCH))
		msg_info("%s: depth=%d matched=%d %s public-key %s digest=%s",
			 namaddr, depth, matched, ustr, tlsa->mdalg, pkey_dgst);
	    myfree(pkey_dgst);
	}

	certs = mixed ? tlsa->pkeys : tlsa->certs;
	if (certs != 0 && !matched) {
	    char   *cert_dgst = tls_fingerprint(cert, tlsa->mdalg);
	    for (dgst = certs->argv; !matched && *dgst; ++dgst)
		if (strcasecmp(cert_dgst, *dgst) == 0)
		    matched = 1;
	    if (TLScontext->log_mask & (TLS_LOG_VERBOSE|TLS_LOG_CERTMATCH))
		msg_info("%s: depth=%d matched=%d %s certificate %s digest %s",
			 namaddr, depth, matched, ustr, tlsa->mdalg, cert_dgst);
	    myfree(cert_dgst);
	}
    }

    return (matched);
}

/* ta_match - match cert against out-of-band TA keys or digests */

static int ta_match(TLS_SESS_STATE *TLScontext, X509_STORE_CTX *ctx,
		    X509 *cert, int depth, int expired)
{
    const TLS_DANE *dane = TLScontext->dane;
    int     matched = tls_cert_match(TLScontext, TLS_DANE_TA, cert, depth);

    /*
     * If we are the TA, the first trusted certificate is one level below!
     * As a degenerate case a self-signed TA at depth 0 is also treated as
     * a TA validated trust chain, (even if the certificate is expired).
     *
     * Note: OpenSSL will flag an error when the chain contains just one
     * certificate that is not self-issued.
     */
    if (matched) {
	if (--depth < 0)
	    depth = 0;
	update_trust_state(TLScontext, depth);
	return (1);
    }

    /*
     * If expired, no need to check for a trust-anchor signature.  The TA
     * itself is matched by its digest, so we're at best looking at some
     * other expired certificate issued by the TA, which we don't accept.
     */
    if (expired)
    	return (0);

    /*
     * Compute the index of the topmost chain certificate; it may need to be
     * verified via one of our out-of-band trust-anchors.  Since we're here,
     * the chain contains at least one certificate.
     *
     * Optimization: if the top is self-issued, we don't need to try to check
     * whether it is signed by any ancestor TAs.  If it is trusted, it will be
     * matched by its fingerprint.
     */
    if (TLScontext->trustdepth < 0 && TLScontext->chaindepth < 0) {
	STACK_OF(X509) *chain = X509_STORE_CTX_get_chain(ctx);
	int     i = sk_X509_num(chain) - 1;
	X509   *top = sk_X509_value(chain, i);

	if (X509_check_issued(top, top) == X509_V_OK)
	    TLScontext->chaindepth = i + 1;
	else
	    TLScontext->chaindepth = i;
    }

    /*
     * Last resort, check whether signed by out-of-band TA public key.
     *
     * Only the top certificate of the server chain needs this logic, since
     * any certs below are signed by their parent, which we checked against
     * the TA list more cheaply.  Do this at most once (by incrementing the
     * depth when we're done).
     */
    if (depth == TLScontext->chaindepth) {
	TLS_DANE_L(EVP_PKEY) *k;
	TLS_DANE_L(X509) *x;

	/*
	 * First check whether issued and signed by a TA cert, this is cheaper
	 * than the bare-public key checks below, since we can determine
	 * whether the candidate TA certificate issued the certificate
	 * to be checked first (name comparisons), before we bother with
	 * signature checks (public key operations).
	 */
	for (x = TLS_DANE_HEAD(dane, X509); !matched && x; x = x->next) {
	    if (X509_check_issued(x->item, cert) == X509_V_OK) {
		EVP_PKEY *pk = X509_get_pubkey(x->item);
		matched = pk && X509_verify(cert, pk) > 0;
		EVP_PKEY_free(pk);
	    }
	}

	/*
	 * With bare TA public keys, we can't check whether the trust chain
	 * is issued by the key, but we can determine whether it is signed
	 * by the key, so we go with that.  Ideally, the corresponding
	 * certificate was presented in the chain, and we matched it by
	 * its public key digest one level up.  This code is here to handle
	 * adverse conditions imposed by sloppy administrators of receiving
	 * systems with poorly constructed chains.
	 */
	for (k = TLS_DANE_HEAD(dane, EVP_PKEY); !matched && k; k = k->next)
	    matched = X509_verify(cert, k->item) > 0;

	if (matched)
	    update_trust_state(TLScontext, depth);
	++TLScontext->chaindepth;
    }
    return (matched);
}

/* tls_verify_certificate_callback - verify peer certificate info */

int     tls_verify_certificate_callback(int ok, X509_STORE_CTX *ctx)
{
    char    buf[CCERT_BUFSIZ];
    X509   *cert;
    int     err;
    int     depth;
    int     max_depth;
    SSL    *con;
    TLS_SESS_STATE *TLScontext;

    /* May be NULL as of OpenSSL 1.0, thanks for the API change! */
    cert = X509_STORE_CTX_get_current_cert(ctx);
    err = X509_STORE_CTX_get_error(ctx);
    con = X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
    TLScontext = SSL_get_ex_data(con, TLScontext_index);

    /*
     * Certificate chain depth limit violations are mis-reported by the
     * OpenSSL library, from SSL_CTX_set_verify(3):
     * 
     * The certificate verification depth set with SSL[_CTX]_verify_depth()
     * stops the verification at a certain depth. The error message produced
     * will be that of an incomplete certificate chain and not
     * X509_V_ERR_CERT_CHAIN_TOO_LONG as may be expected.
     * 
     * We set a limit that is one higher than the user requested limit. If this
     * higher limit is reached, we raise an error even a trusted root CA is
     * present at this depth. This disambiguates trust chain truncation from
     * an incomplete trust chain.
     */
    depth = X509_STORE_CTX_get_error_depth(ctx);
    max_depth = SSL_get_verify_depth(con) - 1;

    /*
     * We never terminate the SSL handshake in the verification callback,
     * rather we allow the TLS handshake to continue, but mark the session as
     * unverified. The application is responsible for closing any sessions
     * with unverified credentials.
     * 
     * When we have an explicit list of trusted CA fingerprints, record the
     * smallest depth at which we find a trusted certificate. If this below
     * the smallest error depth we win and the chain is trusted. Otherwise,
     * the chain is untrusted. We make this decision *each* time we are
     * called with depth == 0 (yes we may be called more than once).
     */
    if (max_depth >= 0 && depth > max_depth) {
	update_error_state(TLScontext, depth, cert,
			   X509_V_ERR_CERT_CHAIN_TOO_LONG);
	return (1);
    }

    /*
     * Per RFC 5280 and its upstream ITU documents, a trust anchor is just
     * a public key, no more no less, and thus certificates bearing the
     * trust-anchor public key are just public keys in X.509v3 garb.  Any
     * meaning attached to their expiration, ... is simply local policy.
     *
     * We don't punish server administrators for including an expired optional
     * TA certificate in their chain.  Had they left it out, and provided us
     * instead with only the TA public-key via a "2 1 0" TLSA record, there'd
     * be no TA certificate from which to learn the expiration dates.
     *
     * Therefore, in the interests of consistent behavior, we only enforce
     * expiration dates BELOW the TA signature.  When we find an expired
     * certificate, we only check whether it is a TA, and not whether it is
     * signed by a TA.
     *
     * Other than allowing TA certificate expiration, the only errors we
     * allow are failure to chain to a trusted root.  Our TA set includes
     * out-of-band data not available to the X509_STORE_CTX.
     *
     * More than one of the allowed errors may be reported at a given depth,
     * trap all instances, but run the matching code at most once.  If the
     * current cert is ok, we have a trusted ancestor, and we're not verbose,
     * don't bother with matching.
     */
    if (cert != 0
	&& (ok == 0
	    || TLScontext->trustdepth < 0
	    || (TLScontext->log_mask & (TLS_LOG_VERBOSE | TLS_LOG_CERTMATCH)))
	&& TLS_DANE_HASTA(TLScontext->dane)
	&& (TLScontext->trustdepth == -1 || depth <= TLScontext->trustdepth)
	&& (TLScontext->errordepth == -1 || depth < TLScontext->errordepth)) {
	int     expired = 0; /* or not yet valid */

	switch (ok ? X509_V_OK : err) {
	case X509_V_ERR_CERT_NOT_YET_VALID:
	case X509_V_ERR_CERT_HAS_EXPIRED:
	    expired = 1;
	    /* FALLTHROUGH */
	case X509_V_OK:
	case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
	case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
	case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
	case X509_V_ERR_CERT_UNTRUSTED:
	    if ((!expired && depth == TLScontext->trustdepth)
		|| ta_match(TLScontext, ctx, cert, depth, expired))
		ok = 1;
	    break;
	}
    }
    if (ok == 0)
	update_error_state(TLScontext, depth, cert, err);

    /*
     * Perhaps the chain is verified, or perhaps we'll get called again,
     * either way the best we know is that if trust depth is below error
     * depth we win and otherwise we lose. Set the error state accordingly.
     *
     * If we are given explicit TA match list, we must match one of them
     * at a non-negative depth below any errors, otherwise we just need
     * no errors.
     */
    if (depth == 0) {
	ok = 0;
        if (TLScontext->trustdepth < 0 && TLS_DANE_HASTA(TLScontext->dane)) {
	    /* Required Policy or DANE certs not present */
	    if (TLScontext->errordepth < 0) {
		/*
		 * For lack of a better choice log the trust problem against
		 * the leaf cert when PKI says yes, but local policy or DANE
		 * says no. Logging a root cert as untrusted would far more
		 * likely confuse users!
		 */
		update_error_state(TLScontext, depth, cert,
				   X509_V_ERR_CERT_UNTRUSTED);
	    }
	} else if (TLScontext->errordepth < 0) {
	    /* No PKI trust errors, or only above a good policy or DANE CA. */
	    ok = 1;
	}
	X509_STORE_CTX_set_error(ctx, ok ? X509_V_OK : TLScontext->errorcode);
    }

    if (TLScontext->log_mask & TLS_LOG_VERBOSE) {
	if (cert)
	    X509_NAME_oneline(X509_get_subject_name(cert), buf, sizeof(buf));
	else
	    strcpy(buf, "<unknown>");
	msg_info("%s: depth=%d verify=%d subject=%s",
		 TLScontext->namaddr, depth, ok, printable(buf, '?'));
    }
    return (1);
}

/* tls_log_verify_error - Report final verification error status */

void    tls_log_verify_error(TLS_SESS_STATE *TLScontext)
{
    char    buf[CCERT_BUFSIZ];
    int     err = TLScontext->errorcode;
    X509   *cert = TLScontext->errorcert;
    int     depth = TLScontext->errordepth;

#define PURPOSE ((depth>0) ? "CA": TLScontext->am_server ? "client": "server")

    if (err == X509_V_OK)
	return;

    /*
     * Specific causes for verification failure.
     */
    switch (err) {
    case X509_V_ERR_CERT_UNTRUSTED:

	/*
	 * We expect the error cert to be the leaf, but it is likely
	 * sufficient to omit it from the log, even less user confusion.
	 */
	msg_info("certificate verification failed for %s: "
		 "not trusted by local or TLSA policy", TLScontext->namaddr);
	break;
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
	msg_info("certificate verification failed for %s: "
		 "self-signed certificate", TLScontext->namaddr);
	break;
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:

	/*
	 * There is no difference between issuing cert not provided and
	 * provided, but not found in CAfile/CApath. Either way, we don't
	 * trust it.
	 */
	if (cert)
	    X509_NAME_oneline(X509_get_issuer_name(cert), buf, sizeof(buf));
	else
	    strcpy(buf, "<unknown>");
	msg_info("certificate verification failed for %s: untrusted issuer %s",
		 TLScontext->namaddr, printable(buf, '?'));
	break;
    case X509_V_ERR_CERT_NOT_YET_VALID:
    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
	msg_info("%s certificate verification failed for %s: certificate not"
		 " yet valid", PURPOSE, TLScontext->namaddr);
	break;
    case X509_V_ERR_CERT_HAS_EXPIRED:
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
	msg_info("%s certificate verification failed for %s: certificate has"
		 " expired", PURPOSE, TLScontext->namaddr);
	break;
    case X509_V_ERR_INVALID_PURPOSE:
	msg_info("certificate verification failed for %s: not designated for "
		 "use as a %s certificate", TLScontext->namaddr, PURPOSE);
	break;
    case X509_V_ERR_CERT_CHAIN_TOO_LONG:
	msg_info("certificate verification failed for %s: "
		 "certificate chain longer than limit(%d)",
		 TLScontext->namaddr, depth - 1);
	break;
    default:
	msg_info("%s certificate verification failed for %s: num=%d:%s",
		 PURPOSE, TLScontext->namaddr, err,
		 X509_verify_cert_error_string(err));
	break;
    }
}

#ifndef DONT_GRIPE
#define DONT_GRIPE 0
#define DO_GRIPE 1
#endif

/* tls_text_name - extract certificate property value by name */

static char *tls_text_name(X509_NAME *name, int nid, const char *label,
			        const TLS_SESS_STATE *TLScontext, int gripe)
{
    const char *myname = "tls_text_name";
    int     pos;
    X509_NAME_ENTRY *entry;
    ASN1_STRING *entry_str;
    int     asn1_type;
    int     utf8_length;
    unsigned char *utf8_value;
    int     ch;
    unsigned char *cp;

    if (name == 0 || (pos = X509_NAME_get_index_by_NID(name, nid, -1)) < 0) {
	if (gripe != DONT_GRIPE) {
	    msg_warn("%s: %s: peer certificate has no %s",
		     myname, TLScontext->namaddr, label);
	    tls_print_errors();
	}
	return (0);
    }
#if 0

    /*
     * If the match is required unambiguous, insist that that no other values
     * be present.
     */
    if (X509_NAME_get_index_by_NID(name, nid, pos) >= 0) {
	msg_warn("%s: %s: multiple %ss in peer certificate",
		 myname, TLScontext->namaddr, label);
	return (0);
    }
#endif

    if ((entry = X509_NAME_get_entry(name, pos)) == 0) {
	/* This should not happen */
	msg_warn("%s: %s: error reading peer certificate %s entry",
		 myname, TLScontext->namaddr, label);
	tls_print_errors();
	return (0);
    }
    if ((entry_str = X509_NAME_ENTRY_get_data(entry)) == 0) {
	/* This should not happen */
	msg_warn("%s: %s: error reading peer certificate %s data",
		 myname, TLScontext->namaddr, label);
	tls_print_errors();
	return (0);
    }

    /*
     * XXX Convert everything into UTF-8. This is a super-set of ASCII, so we
     * don't have to bother with separate code paths for ASCII-like content.
     * If the payload is ASCII then we won't waste lots of CPU cycles
     * converting it into UTF-8. It's up to OpenSSL to do something
     * reasonable when converting ASCII formats that contain non-ASCII
     * content.
     * 
     * XXX Don't bother optimizing the string length error check. It is not
     * worth the complexity.
     */
    asn1_type = ASN1_STRING_type(entry_str);
    if ((utf8_length = ASN1_STRING_to_UTF8(&utf8_value, entry_str)) < 0) {
	msg_warn("%s: %s: error decoding peer %s of ASN.1 type=%d",
		 myname, TLScontext->namaddr, label, asn1_type);
	tls_print_errors();
	return (0);
    }

    /*
     * No returns without cleaning up. A good optimizer will replace multiple
     * blocks of identical code by jumps to just one such block.
     */
#define TLS_TEXT_NAME_RETURN(x) do { \
	char *__tls_text_name_temp = (x); \
	OPENSSL_free(utf8_value); \
	return (__tls_text_name_temp); \
    } while (0)

    /*
     * Remove trailing null characters. They would give false alarms with the
     * length check and with the embedded null check.
     */
#define TRIM0(s, l) do { while ((l) > 0 && (s)[(l)-1] == 0) --(l); } while (0)

    TRIM0(utf8_value, utf8_length);

    /*
     * Enforce the length limit, because the caller will copy the result into
     * a fixed-length buffer.
     */
    if (utf8_length >= CCERT_BUFSIZ) {
	msg_warn("%s: %s: peer %s too long: %d",
		 myname, TLScontext->namaddr, label, utf8_length);
	TLS_TEXT_NAME_RETURN(0);
    }

    /*
     * Reject embedded nulls in ASCII or UTF-8 names. OpenSSL is responsible
     * for producing properly-formatted UTF-8.
     */
    if (utf8_length != strlen((char *) utf8_value)) {
	msg_warn("%s: %s: NULL character in peer %s",
		 myname, TLScontext->namaddr, label);
	TLS_TEXT_NAME_RETURN(0);
    }

    /*
     * Reject non-printable ASCII characters in UTF-8 content.
     * 
     * Note: the code below does not find control characters in illegal UTF-8
     * sequences. It's OpenSSL's job to produce valid UTF-8, and reportedly,
     * it does validation.
     */
    for (cp = utf8_value; (ch = *cp) != 0; cp++) {
	if (ISASCII(ch) && !ISPRINT(ch)) {
	    msg_warn("%s: %s: non-printable content in peer %s",
		     myname, TLScontext->namaddr, label);
	    TLS_TEXT_NAME_RETURN(0);
	}
    }
    TLS_TEXT_NAME_RETURN(mystrdup((char *) utf8_value));
}

/* tls_dns_name - Extract valid DNS name from subjectAltName value */

const char *tls_dns_name(const GENERAL_NAME * gn,
			         const TLS_SESS_STATE *TLScontext)
{
    const char *myname = "tls_dns_name";
    char   *cp;
    const char *dnsname;
    int     len;

    /*
     * Peername checks are security sensitive, carefully scrutinize the
     * input!
     */
    if (gn->type != GEN_DNS)
	msg_panic("%s: Non DNS input argument", myname);

    /*
     * We expect the OpenSSL library to construct GEN_DNS extesion objects as
     * ASN1_IA5STRING values. Check we got the right union member.
     */
    if (ASN1_STRING_type(gn->d.ia5) != V_ASN1_IA5STRING) {
	msg_warn("%s: %s: invalid ASN1 value type in subjectAltName",
		 myname, TLScontext->namaddr);
	return (0);
    }

    /*
     * Safe to treat as an ASCII string possibly holding a DNS name
     */
    dnsname = (char *) ASN1_STRING_data(gn->d.ia5);
    len = ASN1_STRING_length(gn->d.ia5);
    TRIM0(dnsname, len);

    /*
     * Per Dr. Steven Henson of the OpenSSL development team, ASN1_IA5STRING
     * values can have internal ASCII NUL values in this context because
     * their length is taken from the decoded ASN1 buffer, a trailing NUL is
     * always appended to make sure that the string is terminated, but the
     * ASN.1 length may differ from strlen().
     */
    if (len != strlen(dnsname)) {
	msg_warn("%s: %s: internal NUL in subjectAltName",
		 myname, TLScontext->namaddr);
	return 0;
    }

    /*
     * XXX: Should we be more strict and call valid_hostname()? So long as
     * the name is safe to handle, if it is not a valid hostname, it will not
     * compare equal to the expected peername, so being more strict than
     * "printable" is likely excessive...
     */
    if (*dnsname && !allprint(dnsname)) {
	cp = mystrdup(dnsname);
	msg_warn("%s: %s: non-printable characters in subjectAltName: %.100s",
		 myname, TLScontext->namaddr, printable(cp, '?'));
	myfree(cp);
	return 0;
    }
    return (dnsname);
}

/* tls_peer_CN - extract peer common name from certificate */

char   *tls_peer_CN(X509 *peercert, const TLS_SESS_STATE *TLScontext)
{
    char   *cn;

    cn = tls_text_name(X509_get_subject_name(peercert), NID_commonName,
		       "subject CN", TLScontext, DONT_GRIPE);
    return (cn ? cn : mystrdup(""));
}

/* tls_issuer_CN - extract issuer common name from certificate */

char   *tls_issuer_CN(X509 *peer, const TLS_SESS_STATE *TLScontext)
{
    X509_NAME *name;
    char   *cn;

    name = X509_get_issuer_name(peer);

    /*
     * If no issuer CN field, use Organization instead. CA certs without a CN
     * are common, so we only complain if the organization is also missing.
     */
    if ((cn = tls_text_name(name, NID_commonName,
			    "issuer CN", TLScontext, DONT_GRIPE)) == 0)
	cn = tls_text_name(name, NID_organizationName,
			   "issuer Organization", TLScontext, DONT_GRIPE);
    return (cn ? cn : mystrdup(""));
}

#endif
