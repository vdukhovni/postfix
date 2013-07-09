/*++
/* NAME
/*	tls_dane 3
/* SUMMARY
/*	Support for RFC 6698 (DANE) certificate matching
/* SYNOPSIS
/*	#include <tls.h>
/*
/*	int	tls_dane_avail()
/*
/*	void	tls_dane_flush()
/*
/*	void	tls_dane_verbose(on)
/*	int	on;
/*
/*	TLS_DANE *tls_dane_alloc(flags)
/*	int     flags;
/*
/*	void	tls_dane_free(dane)
/*	TLS_DANE *dane;
/*
/*	void	tls_dane_split(dane, certusage, selector, mdalg, digest, delim)
/*	TLS_DANE *dane;
/*	int	certusage;
/*	int	selector;
/*	const char *mdalg;
/*	const char *digest;
/*	const char *delim;
/*
/*	int	tls_dane_load_trustfile(dane, tafile)
/*	TLS_DANE *dane;
/*	const char *tafile;
/*
/*	int	tls_dane_match(TLSContext, usage, cert, depth)
/*	TLS_SESS_STATE *TLScontext;
/*	int	usage;
/*	X509	*cert;
/*	int	depth;
/*
/*	void	tls_dane_set_callback(ssl_ctx, TLScontext)
/*	SSL_CTX *ssl_ctx;
/*	TLS_SESS_STATE *TLScontext;
/*
/*	TLS_DANE *tls_dane_resolve(qname, rname, proto, port)
/*	const char *qname;
/*	const char *rname;
/*	const char *proto;
/*	unsigned port;
/*
/*	int	tls_dane_unusable(dane)
/*	const TLS_DANE *dane;
/*
/*	int	tls_dane_notfound(dane)
/*	const TLS_DANE *dane;
/* DESCRIPTION
/*	tls_dane_avail() returns true if the features required to support DANE
/*	are present in OpenSSL's libcrypto and in libresolv.  Since OpenSSL's
/*	libcrypto is not initialized until we call tls_client_init(), calls
/*	to tls_dane_avail() must be deferred until this initialization is
/*	completed successufully.
/*
/*	tls_dane_flush() flushes all entries from the cache, and deletes
/*	the cache.
/*
/*	tls_dane_verbose() turns on verbose logging of TLSA record lookups.
/*
/*	tls_dane_alloc() returns a pointer to a newly allocated TLS_DANE
/*	structure with null ta and ee digest sublists.  If "flags" includes
/*	TLS_DANE_FLAG_MIXED, the cert and pkey digests are stored together on
/*	the pkeys list with the certs list empty, otherwise they are stored
/*	separately.
/*
/*	tls_dane_free() frees the structure allocated by tls_dane_alloc().
/*
/*	tls_dane_split() splits "digest" using the characters in "delim" as
/*	delimiters and stores the results with the requested "certusage"
/*	and "selector".  This is an incremental interface, that builds a
/*	TLS_DANE structure outside the cache by manually adding entries.
/*
/*	tls_dane_load_trustfile() imports trust-anchor certificates and
/*	public keys from a file (rather than DNS TLSA records).
/*
/*	tls_dane_match() matches the full and/or public key digest of
/*	"cert" against each candidate digest in TLScontext->dane. If usage
/*	is TLS_DANE_EE, the match is against end-entity digests, otherwise
/*	it is against trust-anchor digests.  Returns true if a match is found,
/*	false otherwise.
/*
/*	tls_dane_set_callback() wraps the SSL certificate verification logic
/*	in a function that modifies the input trust chain and trusted
/*	certificate store to map DANE TA validation onto the existing PKI
/*	verification model.  When TLScontext is NULL the callback is
/*	cleared, otherwise it is set.  This callback should only be set
/*	when out-of-band trust-anchors (via DNSSEC DANE TLSA records or
/*	per-destination local configuration) are provided.  Such trust
/*	anchors always override the legacy public CA PKI.  Otherwise, the
/*	callback MUST be cleared.
/*
/*	tls_dane_resolve() maps a (qname, rname, protocol, port) tuple to a
/*	a corresponding TLS_DANE policy structure found in the DNS.  The port
/*	argument is in network byte order.  A null pointer is returned when
/*	the DNS query for the TLSA record tempfailed.  In all other cases the
/*	return value is a pointer to the corresponding TLS_DANE structure.
/*	The caller must free the structure via tls_dane_free().
/*
/*	tls_dane_unusable() checks whether a cached TLS_DANE record is
/*	the result of a validated RRset, with no usable elements.  In
/*	this case, TLS is mandatory, but certificate verification is
/*	not DANE-based.
/*
/*	tls_dane_notfound() checks whether a cached TLS_DANE record is
/*	the result of a validated DNS lookup returning NODATA. In
/*	this case, TLS is not required by RFC, though users may elect
/*	a mandatory TLS fallback policy.
/*
/*	Arguments:
/* .IP dane
/*	Pointer to a TLS_DANE structure that lists the valid trust-anchor
/*	and end-entity full-certificate and/or public-key digests.
/* .IP qname
/*	FQDN of target service (original input form).
/* .IP rname
/*	DNSSEC validated (cname resolved) FQDN of target service.
/* .IP proto
/*	Almost certainly "tcp".
/* .IP port
/*	The TCP port in network byte order.
/* .IP flags
/*	Only one flag is part of the public interface at this time:
/* .IP TLScontext
/*	Client context with TA/EE matching data and related state.
/* .IP usage
/*	Trust anchor (TLS_DANE_TA) or end-entity (TLS_DANE_EE) digests?
/* .IP cert
/*	Certificate from peer trust chain (CA or leaf server).
/* .IP depth
/*	The certificate depth for logging.
/* .IP ssl_ctx
/*	The global SSL_CTX structure used to initialize child SSL
/*	conenctions.
/* .RS
/* .IP TLS_DANE_FLAG_MIXED
/*	Don't distinguish between certificate and public-key digests.
/*	A single digest list for both certificates and keys with be
/*	stored for each algorithm in the "pkeys" field, the "certs"
/*	field will be null.
/* .RE
/* .IP certusage
/*	Trust anchor (TLS_DANE_TA) or end-entity (TLS_DANE_EE) digests?
/* .IP selector
/*	Full certificate (TLS_DANE_CERT) or pubkey (TLS_DANE_PKEY) digests?
/* .IP mdalg
/*	Name of a message digest algorithm suitable for computing secure
/*	(1st pre-image resistant) message digests of certificates. For now,
/*	md5, sha1, or member of SHA-2 family if supported by OpenSSL.
/* .IP digest
/*	The digest (or list of digests concatenated with characters from
/*	"delim") to be added to the TLS_DANE record.
/* .IP delim
/*	The set of delimiter characters used above.
/* LICENSE
/* .ad
/* .fi
/*	This software is free. You can do with it whatever you want.
/*	The original author kindly requests that you acknowledge
/*	the use of his software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*
/*	Viktor Dukhovni
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
#include <vstring.h>
#include <events.h>			/* event_time() */
#include <timecmp.h>
#include <ctable.h>
#include <hex_code.h>

#define STR(x)	vstring_str(x)

/* Global library */

#include <mail_params.h>

/* DNS library. */

#include <dns.h>

/* TLS library. */

#define TLS_INTERNAL
#include <tls.h>

/* Application-specific. */

#undef TRUST_ANCHOR_SUPPORT
#undef DANE_TLSA_SUPPORT
#undef WRAP_SIGNED

#if OPENSSL_VERSION_NUMBER >= 0x1000000fL && \
	(defined(X509_V_FLAG_PARTIAL_CHAIN) || !defined(OPENSSL_NO_ECDH))
#define TRUST_ANCHOR_SUPPORT

#ifndef X509_V_FLAG_PARTIAL_CHAIN
#define WRAP_SIGNED
#endif

#if defined(TLSEXT_MAXLEN_host_name) && RES_USE_DNSSEC && RES_USE_EDNS0
#define DANE_TLSA_SUPPORT
#endif

#endif					/* OPENSSL_VERSION_NUMBER ... */

#ifdef WRAP_SIGNED
static int wrap_signed = 1;

#else
static int wrap_signed = 0;

#endif
static const EVP_MD *signmd;

static EVP_PKEY *danekey;
static ASN1_OBJECT *serverAuth;

static const char *sha256 = "sha256";
static const EVP_MD *sha256md;
static int sha256len;

static const char *sha512 = "sha512";
static const EVP_MD *sha512md;
static int sha512len;

static int digest_mask;

#define TLS_DANE_ENABLE_CC	(1<<0)	/* ca-constraint digests OK */
#define TLS_DANE_ENABLE_TAA	(1<<1)	/* trust-anchor-assertion digests OK */

/*
 * This is not intended to be a long-term cache of pre-parsed TLSA data,
 * rather we primarily want to avoid fetching and parsing the TLSA records
 * for a single multi-homed MX host more than once per delivery. Therefore,
 * we keep the table reasonably small.
 */
#define CACHE_SIZE 20
static CTABLE *dane_cache;

static int dane_initialized;
static int dane_verbose;

/* tls_dane_verbose - enable/disable verbose logging */

void    tls_dane_verbose(int on)
{
    dane_verbose = on;
}

/* gencakey - generate interal DANE root CA key */

static EVP_PKEY *gencakey(void)
{
    EVP_PKEY *key = 0;

#ifdef WRAP_SIGNED
    int     len;
    unsigned char *p;
    EC_KEY *eckey;
    EC_GROUP *group;

    ERR_clear_error();

    if ((eckey = EC_KEY_new()) != 0
	&& (group = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1)) != 0
	&& (EC_GROUP_set_asn1_flag(group, OPENSSL_EC_NAMED_CURVE),
	    EC_KEY_set_group(eckey, group))
	&& EC_KEY_generate_key(eckey)
	&& (key = EVP_PKEY_new()) != 0
	&& !EVP_PKEY_set1_EC_KEY(key, eckey)) {
	EVP_PKEY_free(key);
	key = 0;
    }
    if (group)
	EC_GROUP_free(group);
    if (eckey)
	EC_KEY_free(eckey);
#endif
    return (key);
}

/* dane_init - initialize DANE parameters */

static void dane_init(void)
{
    static NAME_MASK ta_dgsts[] = {
	TLS_DANE_CC, TLS_DANE_ENABLE_CC,
	TLS_DANE_TAA, TLS_DANE_ENABLE_TAA,
	0,
    };

    digest_mask =
	name_mask_opt(VAR_TLS_DANE_TA_DGST, ta_dgsts, var_tls_dane_ta_dgst,
		      NAME_MASK_ANY_CASE | NAME_MASK_FATAL);

    if ((sha256md = EVP_get_digestbyname(sha256)) != 0)
	sha256len = EVP_MD_size(sha256md);
    if ((sha512md = EVP_get_digestbyname(sha512)) != 0)
	sha512len = EVP_MD_size(sha512md);
    signmd = sha256md ? sha256md : EVP_sha1();

    /* Don't report old news */
    ERR_clear_error();

#ifdef TRUST_ANCHOR_SUPPORT
    if ((wrap_signed && (danekey = gencakey()) == 0)
	|| (serverAuth = OBJ_nid2obj(NID_server_auth)) == 0) {
	msg_warn("cannot generate TA certificates, no DANE support");
	tls_print_errors();
    }
#endif
    dane_initialized = 1;
}

/* tls_dane_avail - check for availability of dane required digests */

int     tls_dane_avail(void)
{
    if (!dane_initialized)
	dane_init();

#ifdef DANE_TLSA_SUPPORT
    return (sha256md && sha512md && serverAuth);
#else
    return (0);
#endif
}

/* tls_dane_flush - flush the cache */

void    tls_dane_flush(void)
{
    if (dane_cache)
	ctable_free(dane_cache);
    dane_cache = 0;
}

/* tls_dane_alloc - allocate a TLS_DANE structure */

TLS_DANE *tls_dane_alloc(int flags)
{
    TLS_DANE *dane = (TLS_DANE *) mymalloc(sizeof(*dane));

    dane->ta = 0;
    dane->ee = 0;
    dane->certs = 0;
    dane->pkeys = 0;
    dane->base_domain = 0;
    dane->flags = flags;
    dane->expires = 0;
    dane->refs = 1;
    return (dane);
}

static void ta_cert_insert(TLS_DANE *d, X509 *x)
{
    TLS_CERTS *new = (TLS_CERTS *) mymalloc(sizeof(*new));

    CRYPTO_add(&x->references, 1, CRYPTO_LOCK_X509);
    new->cert = x;
    new->next = d->certs;
    d->certs = new;
}

static void free_ta_certs(TLS_DANE *d)
{
    TLS_CERTS *head;
    TLS_CERTS *next;

    for (head = d->certs; head; head = next) {
	next = head->next;
	X509_free(head->cert);
	myfree((char *) head);
    }
}

static void ta_pkey_insert(TLS_DANE *d, EVP_PKEY *k)
{
    TLS_PKEYS *new = (TLS_PKEYS *) mymalloc(sizeof(*new));

    CRYPTO_add(&k->references, 1, CRYPTO_LOCK_EVP_PKEY);
    new->pkey = k;
    new->next = d->pkeys;
    d->pkeys = new;
}

static void free_ta_pkeys(TLS_DANE *d)
{
    TLS_PKEYS *head;
    TLS_PKEYS *next;

    for (head = d->pkeys; head; head = next) {
	next = head->next;
	EVP_PKEY_free(head->pkey);
	myfree((char *) head);
    }
}

static void tlsa_free(TLS_TLSA *tlsa)
{

    myfree(tlsa->mdalg);
    if (tlsa->certs)
	argv_free(tlsa->certs);
    if (tlsa->pkeys)
	argv_free(tlsa->pkeys);
    myfree((char *) tlsa);
}

/* tls_dane_free - free a TLS_DANE structure */

void    tls_dane_free(TLS_DANE *dane)
{
    TLS_TLSA *tlsa;
    TLS_TLSA *next;

    if (--dane->refs > 0)
	return;

    /* De-allocate TA and EE lists */
    for (tlsa = dane->ta; tlsa; tlsa = next) {
	next = tlsa->next;
	tlsa_free(tlsa);
    }
    for (tlsa = dane->ee; tlsa; tlsa = next) {
	next = tlsa->next;
	tlsa_free(tlsa);
    }

    /* De-allocate full trust-anchor certs and pkeys */
    free_ta_certs(dane);
    free_ta_pkeys(dane);
    if (dane->base_domain)
	myfree(dane->base_domain);

    myfree((char *) dane);
}

/* dane_free - ctable style */

static void dane_free(void *dane, void *unused_context)
{
    tls_dane_free((TLS_DANE *) dane);
}

/* dane_locate - list head address of TLSA sublist for given algorithm */

static TLS_TLSA **dane_locate(TLS_TLSA **tlsap, const char *mdalg)
{
    TLS_TLSA *new;

    /*
     * Correct computation of the session cache serverid requires a TLSA
     * digest list that is sorted by algorithm name.  Below we maintain the
     * sort order (by algorithm name canonicalized to lowercase).
     */
    for (; *tlsap; tlsap = &(*tlsap)->next) {
	int     cmp = strcasecmp(mdalg, (*tlsap)->mdalg);

	if (cmp == 0)
	    return (tlsap);
	if (cmp < 0)
	    break;
    }

    new = (TLS_TLSA *) mymalloc(sizeof(*new));
    new->mdalg = lowercase(mystrdup(mdalg));
    new->certs = 0;
    new->pkeys = 0;
    new->next = *tlsap;
    *tlsap = new;

    return (tlsap);
}

/* tls_dane_split - split and append digests */

void    tls_dane_split(TLS_DANE *dane, int certusage, int selector,
	           const char *mdalg, const char *digest, const char *delim)
{
    TLS_TLSA **tlsap;
    TLS_TLSA *tlsa;
    ARGV  **argvp;

    tlsap = (certusage == TLS_DANE_EE) ? &dane->ee : &dane->ta;
    tlsa = *(tlsap = dane_locate(tlsap, mdalg));
    argvp = ((dane->flags & TLS_DANE_FLAG_MIXED) || selector == TLS_DANE_PKEY) ?
	&tlsa->pkeys : &tlsa->certs;

    /* Delimited append, may append nothing */
    if (*argvp == 0)
	*argvp = argv_split(digest, delim);
    else
	argv_split_append(*argvp, digest, delim);

    if ((*argvp)->argc == 0) {
	argv_free(*argvp);
	*argvp = 0;

	/* Remove empty elements from the list */
	if (tlsa->pkeys == 0 && tlsa->certs == 0) {
	    *tlsap = tlsa->next;
	    tlsa_free(tlsa);
	}
    }
}

/* dane_add - add a digest entry */

static void dane_add(TLS_DANE *dane, int certusage, int selector,
		             const char *mdalg, char *digest)
{
    TLS_TLSA **tlsap;
    TLS_TLSA *tlsa;
    ARGV  **argvp;

    switch (certusage) {
    case DNS_TLSA_USAGE_CA_CONSTRAINT:
    case DNS_TLSA_USAGE_TRUST_ANCHOR_ASSERTION:
	certusage = TLS_DANE_TA;		/* Collapse 0/2 -> 2 */
	break;
    case DNS_TLSA_USAGE_SERVICE_CERTIFICATE_CONSTRAINT:
    case DNS_TLSA_USAGE_DOMAIN_ISSUED_CERTIFICATE:
	certusage = TLS_DANE_EE;		/* Collapse 1/3 -> 3 */
	break;
    }

    switch (selector) {
    case DNS_TLSA_SELECTOR_FULL_CERTIFICATE:
	selector = TLS_DANE_CERT;
	break;
    case DNS_TLSA_SELECTOR_SUBJECTPUBLICKEYINFO:
	selector = TLS_DANE_PKEY;
	break;
    }

    tlsap = (certusage == TLS_DANE_EE) ? &dane->ee : &dane->ta;
    tlsa = *(tlsap = dane_locate(tlsap, mdalg));
    argvp = ((dane->flags & TLS_DANE_FLAG_MIXED) || selector == TLS_DANE_PKEY) ?
	&tlsa->pkeys : &tlsa->certs;

    if (*argvp == 0)
	*argvp = argv_alloc(1);
    argv_add(*argvp, digest, ARGV_END);
}

#ifdef DANE_TLSA_SUPPORT

/* tlsa_rr_cmp - qsort TLSA rrs in case shuffled by name server */

static int tlsa_rr_cmp(DNS_RR *a, DNS_RR *b)
{
    if (a->data_len == b->data_len)
	return (memcmp(a->data, b->data, a->data_len));
    return ((a->data_len > b->data_len) ? 1 : -1);
}

/* parse_tlsa_rrs - parse a validated TLSA RRset */

static void parse_tlsa_rrs(TLS_DANE *dane, DNS_RR *rr)
{
    uint8_t usage;
    uint8_t selector;
    uint8_t mtype;
    int     mlen;
    const unsigned char *p;

    if (rr == 0)
	msg_panic("null TLSA rr");

    for ( /* nop */ ; rr; rr = rr->next) {
	const char *mdalg = 0;
	int     mdlen;
	char   *digest;
	int     same = (strcasecmp(rr->rname, rr->qname) == 0);
	uint8_t *ip = (uint8_t *) rr->data;
	X509   *x = 0;			/* OpenSSL tries to re-use *x if x!=0 */
	EVP_PKEY *k = 0;		/* OpenSSL tries to re-use *k if k!=0 */

#define rcname(rr) (same ? "" : rr->qname)
#define rarrow(rr) (same ? "" : " -> ")

	if (rr->type != T_TLSA)
	    msg_panic("unexpected non-TLSA RR type %u for %s%s%s", rr->type,
		      rcname(rr), rarrow(rr), rr->rname);

	/* Skip malformed */
	if ((mlen = rr->data_len - 3) < 0) {
	    msg_warn("truncated length %u RR: %s%s%s IN TLSA ...",
		(unsigned) rr->data_len, rcname(rr), rarrow(rr), rr->rname);
	    continue;
	}
	switch (usage = *ip++) {
	default:
	    msg_warn("unsupported certificate usage %u in RR: "
		     "%s%s%s IN TLSA %u ...", usage,
		     rcname(rr), rarrow(rr), rr->rname, usage);
	    continue;
	case DNS_TLSA_USAGE_CA_CONSTRAINT:
	case DNS_TLSA_USAGE_TRUST_ANCHOR_ASSERTION:
	case DNS_TLSA_USAGE_SERVICE_CERTIFICATE_CONSTRAINT:
	case DNS_TLSA_USAGE_DOMAIN_ISSUED_CERTIFICATE:
	    break;
	}

	switch (selector = *ip++) {
	default:
	    msg_warn("unsupported selector %u in RR: "
		     "%s%s%s IN TLSA %u %u ...", selector,
		     rcname(rr), rarrow(rr), rr->rname, usage, selector);
	    continue;
	case DNS_TLSA_SELECTOR_FULL_CERTIFICATE:
	case DNS_TLSA_SELECTOR_SUBJECTPUBLICKEYINFO:
	    break;
	}

	switch (mtype = *ip++) {
	default:
	    msg_warn("unsupported matching type %u in RR: "
		     "%s%s%s IN TLSA %u %u %u ...", mtype, rcname(rr),
		     rarrow(rr), rr->rname, usage, selector, mtype);
	    continue;
	case DNS_TLSA_MATCHING_TYPE_SHA256:
	    if (!mdalg) {
		mdalg = sha256;
		mdlen = sha256len;
	    }
	    /* FALLTHROUGH */
	case DNS_TLSA_MATCHING_TYPE_SHA512:
	    if (!mdalg) {
		mdalg = sha512;
		mdlen = sha512len;
	    }
	    if (mlen != mdlen) {
		msg_warn("malformed %s digest, length %u, in RR: "
			 "%s%s%s IN TLSA %u %u %u ...", mdalg, mlen,
			 rcname(rr), rarrow(rr), rr->rname,
			 usage, selector, mtype);
		continue;
	    }
	    switch (usage) {
	    case DNS_TLSA_USAGE_CA_CONSTRAINT:
		if (!(digest_mask & TLS_DANE_ENABLE_CC)) {
		    msg_warn("%s trust-anchor %s digests disabled, in RR: "
			     "%s%s%s IN TLSA %u %u %u ...", TLS_DANE_CC,
			     mdalg, rcname(rr), rarrow(rr), rr->rname,
			     usage, selector, mtype);
		    continue;
		}
		break;
	    case DNS_TLSA_USAGE_TRUST_ANCHOR_ASSERTION:
		if (!(digest_mask & TLS_DANE_ENABLE_TAA)) {
		    msg_warn("%s trust-anchor %s digests disabled, in RR: "
			     "%s%s%s IN TLSA %u %u %u ...", TLS_DANE_TAA,
			     mdalg, rcname(rr), rarrow(rr), rr->rname,
			     usage, selector, mtype);
		    continue;
		}
		break;
	    }
	    dane_add(dane, usage, selector, mdalg,
		   digest = tls_digest_encode((unsigned char *) ip, mdlen));
	    break;

	case DNS_TLSA_MATCHING_TYPE_NO_HASH_USED:
	    p = (unsigned char *) ip;

	    /* Validate the cert or public key via d2i_mumble() */
	    switch (selector) {
	    case DNS_TLSA_SELECTOR_FULL_CERTIFICATE:
		if (!d2i_X509(&x, &p, mlen)) {
		    msg_warn("malformed %s in RR: "
			     "%s%s%s IN TLSA %u %u %u ...", "certificate",
			     rcname(rr), rarrow(rr), rr->rname,
			     usage, selector, mtype);
		    continue;
		}
		/* Also unusable if public key is malformed */
		if ((k = X509_get_pubkey(x)) == 0) {
		    msg_warn("%s public key malformed in RR: "
			     "%s%s%s IN TLSA %u %u %u ...", "certificate",
			     rcname(rr), rarrow(rr), rr->rname,
			     usage, selector, mtype);
		    X509_free(x);
		    continue;
		}
		EVP_PKEY_free(k);

		/*
		 * When a full trust-anchor certificate is published via DNS,
		 * we may need to use it to validate the server trust chain.
		 * Store it away for later use.  We collapse certificate
		 * usage 0/2 because MTAs don't stock a complete list of the
		 * usual browser-trusted CAs.  Thus, here (and in the public
		 * key case below) we treat the usages identically.
		 */
		switch (usage) {
		case DNS_TLSA_USAGE_CA_CONSTRAINT:
		case DNS_TLSA_USAGE_TRUST_ANCHOR_ASSERTION:
		    ta_cert_insert(dane, x);
		    break;
		}
		X509_free(x);
		break;
	    case DNS_TLSA_SELECTOR_SUBJECTPUBLICKEYINFO:
		if (!d2i_PUBKEY(&k, &p, mlen)) {
		    msg_warn("malformed %s in RR: "
			     "%s%s%s IN TLSA %u %u %u ...", "public key",
			     rcname(rr), rarrow(rr), rr->rname,
			     usage, selector, mtype);
		    continue;
		}
		/* See full cert case above */
		switch (usage) {
		case DNS_TLSA_USAGE_CA_CONSTRAINT:
		case DNS_TLSA_USAGE_TRUST_ANCHOR_ASSERTION:
		    ta_pkey_insert(dane, k);
		    break;
		}
		EVP_PKEY_free(k);
		break;
	    }

	    /*
	     * The cert or key was valid, just digest the raw object, and
	     * encode the digest value.  We choose SHA256.
	     */
	    dane_add(dane, usage, selector, mdalg = sha256,
		     digest = tls_data_fprint((char *) ip, mlen, sha256));
	    break;
	}
	if (msg_verbose || dane_verbose) {
	    switch (mtype) {
	    default:
		msg_info("using DANE RR: %s%s%s IN TLSA %u %u %u %s",
			 rcname(rr), rarrow(rr), rr->rname,
			 usage, selector, mtype, digest);
		break;
	    case DNS_TLSA_MATCHING_TYPE_NO_HASH_USED:
		msg_info("using DANE RR: %s%s%s IN TLSA %u %u %u <%s>; "
			 "%s digest %s",
			 rcname(rr), rarrow(rr), rr->rname,
			 usage, selector, mtype,
			 (selector == DNS_TLSA_SELECTOR_FULL_CERTIFICATE) ?
			 "certificate" : "public key", mdalg, digest);
		break;
	    }
	}
	myfree(digest);
    }

    if (dane->ta == 0 && dane->ee == 0)
	dane->flags |= TLS_DANE_FLAG_EMPTY;
}

/* dane_lookup - TLSA record lookup, ctable style */

static void *dane_lookup(const char *tlsa_fqdn, void *unused_ctx)
{
    static VSTRING *why = 0;
    int     ret;
    DNS_RR *rrs = 0;
    TLS_DANE *dane;

    if (why == 0)
	why = vstring_alloc(10);

    dane = tls_dane_alloc(0);
    ret = dns_lookup(tlsa_fqdn, T_TLSA, RES_USE_DNSSEC, &rrs, 0, why);

    switch (ret) {
    case DNS_OK:
	if (TLS_DANE_CACHE_TTL_MIN && rrs->ttl < TLS_DANE_CACHE_TTL_MIN)
	    rrs->ttl = TLS_DANE_CACHE_TTL_MIN;
	if (TLS_DANE_CACHE_TTL_MAX && rrs->ttl > TLS_DANE_CACHE_TTL_MAX)
	    rrs->ttl = TLS_DANE_CACHE_TTL_MAX;

	/* One more second to account for discrete time */
	dane->expires = 1 + event_time() + rrs->ttl;

	if (rrs->dnssec_valid) {
	    /* Sort for deterministic digest in session cache lookup key */
	    rrs = dns_rr_sort(rrs, tlsa_rr_cmp);
	    parse_tlsa_rrs(dane, rrs);
	} else
	    dane->flags |= TLS_DANE_FLAG_NORRS;

	dns_rr_free(rrs);
	break;

    case DNS_NOTFOUND:
	dane->flags |= TLS_DANE_FLAG_NORRS;
	dane->expires = 1 + event_time() + TLS_DANE_CACHE_TTL_MIN;
	break;

    default:
	msg_warn("DANE TLSA lookup problem: %s", STR(why));
	dane->flags |= TLS_DANE_FLAG_ERROR;
	break;
    }

    return (void *) dane;
}

/* resolve_host - resolve TLSA RRs for hostname (rname or qname) */

static TLS_DANE *resolve_host(const char *host, const char *proto,
			              unsigned port)
{
    static VSTRING *query_domain;
    TLS_DANE *dane;

    if (query_domain == 0)
	query_domain = vstring_alloc(64);

    vstring_sprintf(query_domain, "_%u._%s.%s", ntohs(port), proto, host);
    dane = (TLS_DANE *) ctable_locate(dane_cache, STR(query_domain));
    if (timecmp(event_time(), dane->expires) > 0)
	dane = (TLS_DANE *) ctable_refresh(dane_cache, STR(query_domain));
    if (dane->base_domain == 0)
	dane->base_domain = mystrdup(host);
    return (dane);
}

#endif

/* tls_dane_resolve - cached map: (name, proto, port) -> TLS_DANE */

TLS_DANE *tls_dane_resolve(const char *qname, const char *rname,
			           const char *proto, unsigned port)
{
    TLS_DANE *dane = 0;

#ifdef DANE_TLSA_SUPPORT
    if (!tls_dane_avail())
	return (dane);

    if (!dane_cache)
	dane_cache = ctable_create(CACHE_SIZE, dane_lookup, dane_free, 0);

    /*
     * Try the rname first, if nothing there, try the qname.  Note, lookup
     * errors are distinct from success with nothing found.  If the rname
     * lookup fails we don't try the qname.  The rname may be null when only
     * the qname is in a secure zone.
     */
    if (rname)
	dane = resolve_host(rname, proto, port);
    if (!rname || (tls_dane_notfound(dane) && strcmp(qname, rname) != 0))
	dane = resolve_host(qname, proto, port);
    if (dane->flags & TLS_DANE_FLAG_ERROR)
	return (0);

    ++dane->refs;
#endif
    return (dane);
}

/* tls_dane_load_trustfile - load trust anchor certs or keys from file */

int     tls_dane_load_trustfile(TLS_DANE *dane, const char *tafile)
{
#ifdef TRUST_ANCHOR_SUPPORT
    BIO    *bp;
    char   *name = 0;
    char   *header = 0;
    unsigned char *data = 0;
    long    len;
    int     tacount;
    char   *errtype = 0;		/* if error: cert or pkey? */
    const char *mdalg;

    /* nop */
    if (tafile == 0 || *tafile == 0)
	return (1);

    if (!dane_initialized)
	dane_init();

    if (serverAuth == 0) {
	msg_warn("trust-anchor files not supported");
	return (0);
    }
    mdalg = sha256md ? sha256 : "sha1";

    /*
     * On each call, PEM_read() wraps a stdio file in a BIO_NOCLOSE bio,
     * calls PEM_read_bio() and then frees the bio.  It is just as easy to
     * open a BIO as a stdio file, so we use BIOs and call PEM_read_bio()
     * directly.
     */
    if ((bp = BIO_new_file(tafile, "r")) == NULL) {
	msg_warn("error opening trust anchor file: %s: %m", tafile);
	return (0);
    }
    /* Don't report old news */
    ERR_clear_error();

    for (tacount = 0;
	 errtype == 0 && PEM_read_bio(bp, &name, &header, &data, &len);
	 ++tacount) {
	const unsigned char *p = data;
	int     usage = DNS_TLSA_USAGE_TRUST_ANCHOR_ASSERTION;
	int     selector;
	char   *digest;

	if (strcmp(name, PEM_STRING_X509) == 0
	    || strcmp(name, PEM_STRING_X509_OLD) == 0) {
	    X509   *cert = d2i_X509(0, &p, len);

	    if (cert && (p - data) == len) {
		selector = DNS_TLSA_SELECTOR_FULL_CERTIFICATE;
		digest = tls_data_fprint((char *) data, len, mdalg);
		dane_add(dane, usage, selector, mdalg, digest);
		myfree(digest);
		ta_cert_insert(dane, cert);
	    } else
		errtype = "certificate";
	    if (cert)
		X509_free(cert);
	} else if (strcmp(name, PEM_STRING_PUBLIC) == 0) {
	    EVP_PKEY *pkey = d2i_PUBKEY(0, &p, len);

	    if (pkey && (p - data) == len) {
		selector = DNS_TLSA_SELECTOR_SUBJECTPUBLICKEYINFO;
		digest = tls_data_fprint((char *) data, len, mdalg);
		dane_add(dane, usage, selector, mdalg, digest);
		myfree(digest);
		ta_pkey_insert(dane, pkey);
	    } else
		errtype = "public key";
	    if (pkey)
		EVP_PKEY_free(pkey);
	}

	/*
	 * If any of these were null, PEM_read() would have failed.
	 */
	OPENSSL_free(name);
	OPENSSL_free(header);
	OPENSSL_free(data);
    }
    BIO_free(bp);

    if (errtype) {
	tls_print_errors();
	msg_warn("error reading: %s: malformed trust-anchor %s",
		 tafile, errtype);
	return (0);
    }
    if (ERR_GET_REASON(ERR_peek_last_error()) == PEM_R_NO_START_LINE) {
	/* Reached end of PEM file */
	ERR_clear_error();
	return (tacount > 0);
    }
    /* Some other PEM read error */
    tls_print_errors();
#else
    msg_warn("Trust anchor files not supported");
#endif
    return (0);
}

/* tls_dane_match - match cert against given list of TA or EE digests */

int     tls_dane_match(TLS_SESS_STATE *TLScontext, int usage,
		               X509 *cert, int depth)
{
    const TLS_DANE *dane = TLScontext->dane;
    TLS_TLSA *tlsa = (usage == TLS_DANE_EE) ? dane->ee : dane->ta;
    const char *namaddr = TLScontext->namaddr;
    const char *ustr = (usage == TLS_DANE_EE) ? "end entity" : "trust anchor";
    int     mixed = (dane->flags & TLS_DANE_FLAG_MIXED);
    int     matched;

    for (matched = 0; tlsa && !matched; tlsa = tlsa->next) {
	char  **dgst;
	ARGV   *certs;

	if (tlsa->pkeys) {
	    char   *pkey_dgst = tls_pkey_fprint(cert, tlsa->mdalg);

	    for (dgst = tlsa->pkeys->argv; !matched && *dgst; ++dgst)
		if (strcasecmp(pkey_dgst, *dgst) == 0)
		    matched = 1;
	    if (TLScontext->log_mask & (TLS_LOG_VERBOSE | TLS_LOG_CERTMATCH)
		&& matched)
		msg_info("%s: depth=%d matched %s public-key %s digest=%s",
			 namaddr, depth, ustr, tlsa->mdalg, pkey_dgst);
	    myfree(pkey_dgst);
	}

	/*
	 * Backwards compatible "fingerprint" security level interface:
	 * 
	 * Certificate digests and public key digests are interchangeable, each
	 * leaf certificate is matched via either the public key digest or
	 * full certificate digest when "mixed" is true.  The combined set of
	 * digests is stored on the pkeys digest list and the certs list is
	 * empty.  An attacker would need a 2nd-preimage (not just a
	 * collision) that is feasible across types (given cert digest ==
	 * some key digest) while difficult within a type (e.g. given cert
	 * some other cert digest).  No such attacks are know at this time,
	 * and it is expected that if any are found they would work within as
	 * well as across the cert/key data types.
	 */
	certs = mixed ? tlsa->pkeys : tlsa->certs;
	if (certs != 0 && !matched) {
	    char   *cert_dgst = tls_cert_fprint(cert, tlsa->mdalg);

	    for (dgst = certs->argv; !matched && *dgst; ++dgst)
		if (strcasecmp(cert_dgst, *dgst) == 0)
		    matched = 1;
	    if (TLScontext->log_mask & (TLS_LOG_VERBOSE | TLS_LOG_CERTMATCH)
		&& matched)
		msg_info("%s: depth=%d matched %s certificate %s digest %s",
			 namaddr, depth, ustr, tlsa->mdalg, cert_dgst);
	    myfree(cert_dgst);
	}
    }

    return (matched);
}

/* add_ext - add simple extension (no config section references) */

static int add_ext(X509 *issuer, X509 *subject, int ext_nid, char *ext_val)
{
    X509V3_CTX v3ctx;
    X509_EXTENSION *ext;
    x509_extension_stack_t *exts;

    X509V3_set_ctx(&v3ctx, issuer, subject, 0, 0, 0);
    if ((exts = subject->cert_info->extensions) == 0)
	exts = subject->cert_info->extensions = sk_X509_EXTENSION_new_null();

    if ((ext = X509V3_EXT_conf_nid(0, &v3ctx, ext_nid, ext_val)) != 0
	&& sk_X509_EXTENSION_push(exts, ext))
	return (1);
    if (ext)
	X509_EXTENSION_free(ext);
    return (0);
}

/* set_serial - set serial number to match akid or use subject's plus 1 */

static int set_serial(X509 *cert, AUTHORITY_KEYID *akid, X509 *subject)
{
    int     ret = 0;
    BIGNUM *bn;

    if (akid && akid->serial)
	return (X509_set_serialNumber(cert, akid->serial));

    /*
     * Add one to subject's serial to avoid collisions between TA serial and
     * serial of signing root.
     */
    if ((bn = ASN1_INTEGER_to_BN(X509_get_serialNumber(subject), 0)) != 0
	&& BN_add_word(bn, 1)
	&& BN_to_ASN1_INTEGER(bn, X509_get_serialNumber(cert)))
	ret = 1;

    if (bn)
	BN_free(bn);
    return (ret);
}

/* add_akid - add authority key identifier */

static int add_akid(X509 *cert, AUTHORITY_KEYID *akid)
{
    ASN1_STRING *id;
    unsigned char c = 0;
    int     ret = 0;

    /*
     * 0 will never be our subject keyid from a SHA-1 hash, but it could be
     * our subject keyid if forced from child's akid.  If so, set our
     * authority keyid to 1.  This way we are never self-signed, and thus
     * exempt from any potential (off by default for now in OpenSSL)
     * self-signature checks!
     */
    id = (ASN1_STRING *) ((akid && akid->keyid) ? akid->keyid : 0);
    if (id && M_ASN1_STRING_length(id) == 1 && *M_ASN1_STRING_data(id) == c)
	c = 1;

    if ((akid = AUTHORITY_KEYID_new()) != 0
	&& (akid->keyid = ASN1_OCTET_STRING_new()) != 0
	&& M_ASN1_OCTET_STRING_set(akid->keyid, (void *) &c, 1)
	&& X509_add1_ext_i2d(cert, NID_authority_key_identifier, akid, 0, 0))
	ret = 1;
    if (akid)
	AUTHORITY_KEYID_free(akid);
    return (ret);
}

/* add_skid - add subject key identifier to match child's akid */

static int add_skid(X509 *cert, AUTHORITY_KEYID *akid)
{
    int     ret;

    if (akid && akid->keyid) {
	VSTRING *hexid = vstring_alloc(2 * EVP_MAX_MD_SIZE);
	ASN1_STRING *id = (ASN1_STRING *) (akid->keyid);

	hex_encode(hexid, (char *) M_ASN1_STRING_data(id),
		   M_ASN1_STRING_length(id));
	ret = add_ext(0, cert, NID_subject_key_identifier, STR(hexid));
	vstring_free(hexid);
    } else {
	ret = add_ext(0, cert, NID_subject_key_identifier, "hash");
    }
    return (ret);
}

/* set_issuer - set issuer DN to match akid if specified */

static int set_issuer_name(X509 *cert, AUTHORITY_KEYID *akid)
{

    /*
     * If subject's akid specifies an authority key identifer issuer name, we
     * must use that.
     */
    if (akid && akid->issuer) {
	int     i;
	general_name_stack_t *gens = akid->issuer;

	for (i = 0; i < sk_GENERAL_NAME_num(gens); ++i) {
	    GENERAL_NAME *gn = sk_GENERAL_NAME_value(gens, i);

	    if (gn->type == GEN_DIRNAME)
		return (X509_set_issuer_name(cert, gn->d.dirn));
	}
    }
    return (X509_set_issuer_name(cert, X509_get_subject_name(cert)));
}

/* grow_chain - add certificate to chain */

static void grow_chain(x509_stack_t **skptr, X509 *cert, ASN1_OBJECT *trust)
{
    if (!*skptr && (*skptr = sk_X509_new_null()) == 0)
	msg_fatal("out of memory");
    if (cert) {
	if (trust && !X509_add1_trust_object(cert, trust))
	    msg_fatal("out of memory");
	CRYPTO_add(&cert->references, 1, CRYPTO_LOCK_X509);
	if (!sk_X509_push(*skptr, cert))
	    msg_fatal("out of memory");
    }
}

/* wrap_key - wrap TA "key" as issuer of "subject" */

static int wrap_key(TLS_SESS_STATE *TLScontext, EVP_PKEY *key, X509 *subject,
		            int depth)
{
    int     ret = 1;
    X509   *cert = 0;
    AUTHORITY_KEYID *akid;
    X509_NAME *name = X509_get_issuer_name(subject);

    /*
     * Record the depth of the intermediate wrapper certificate, logged in
     * the verify callback, unlike the parent root CA.
     */
    if (!key)
	TLScontext->tadepth = depth;
    else if (TLScontext->log_mask & (TLS_LOG_VERBOSE | TLS_LOG_CERTMATCH))
	msg_info("%s: depth=%d chain is trust-anchor signed",
		 TLScontext->namaddr, depth);

    /*
     * If key is NULL generate a self-signed root CA, with key "danekey",
     * otherwise an intermediate CA signed by above.
     */
    if ((cert = X509_new()) == 0)
	return (0);

    akid = X509_get_ext_d2i(subject, NID_authority_key_identifier, 0, 0);

    ERR_clear_error();

    /* CA cert valid for +/- 30 days */
    if (!X509_set_version(cert, 2)
	|| !set_serial(cert, akid, subject)
	|| !X509_set_subject_name(cert, name)
	|| !set_issuer_name(cert, akid)
	|| !X509_gmtime_adj(X509_get_notBefore(cert), -30 * 86400L)
	|| !X509_gmtime_adj(X509_get_notAfter(cert), 30 * 86400L)
	|| !X509_set_pubkey(cert, key ? key : danekey)
	|| !add_ext(0, cert, NID_basic_constraints, "CA:TRUE")
	|| (key && !add_akid(cert, akid))
	|| !add_skid(cert, akid)
	|| (wrap_signed
	    && (!X509_sign(cert, danekey, signmd)
		|| (key && !wrap_key(TLScontext, 0, cert, depth + 1))))) {
	msg_warn("error generating DANE wrapper certificate");
	tls_print_errors();
	ret = 0;
    }
    if (akid)
	AUTHORITY_KEYID_free(akid);
    if (ret) {
	if (key && wrap_signed)
	    grow_chain(&TLScontext->untrusted, cert, 0);
	else
	    grow_chain(&TLScontext->trusted, cert, serverAuth);
    }
    if (cert)
	X509_free(cert);
    return (ret);
}

/* ta_signed - is certificate signed by a TLSA cert or pkey */

static int ta_signed(TLS_SESS_STATE *TLScontext, X509 *cert, int depth)
{
    const TLS_DANE *dane = TLScontext->dane;
    EVP_PKEY *pk;
    TLS_PKEYS *k;
    TLS_CERTS *x;
    int     done = 0;

    /*
     * First check whether issued and signed by a TA cert, this is cheaper
     * than the bare-public key checks below, since we can determine whether
     * the candidate TA certificate issued the certificate to be checked
     * first (name comparisons), before we bother with signature checks
     * (public key operations).
     */
    for (x = dane->certs; !done && x; x = x->next) {
	if (X509_check_issued(x->cert, cert) == X509_V_OK) {
	    if ((pk = X509_get_pubkey(x->cert)) == 0)
		continue;
	    /* Check signature, since some other TA may work if not this. */
	    if (X509_verify(cert, pk) > 0)
		done = wrap_key(TLScontext, pk, cert, depth);
	    EVP_PKEY_free(pk);
	}
    }

    /*
     * With bare TA public keys, we can't check whether the trust chain is
     * issued by the key, but we can determine whether it is signed by the
     * key, so we go with that.
     * 
     * Ideally, the corresponding certificate was presented in the chain, and we
     * matched it by its public key digest one level up.  This code is here
     * to handle adverse conditions imposed by sloppy administrators of
     * receiving systems with poorly constructed chains.
     * 
     * We'd like to optimize out keys that should not match when the cert's
     * authority key id does not match the key id of this key computed via
     * the RFC keyid algorithm (SHA-1 digest of public key bit-string sans
     * ASN1 tag and length thus also excluding the unused bits field that is
     * logically part of the length).  However, some CAs have a non-standard
     * authority keyid, so we lose.  Too bad.
     */
    for (k = dane->pkeys; !done && k; k = k->next)
	if (X509_verify(cert, k->pkey) > 0)
	    done = wrap_key(TLScontext, k->pkey, cert, depth);

    return (done);
}

/* set_trust - configure for DANE validation */

static void set_trust(TLS_SESS_STATE *TLScontext, X509_STORE_CTX *ctx)
{
    int     n;
    int     i;
    int     depth = 0;
    EVP_PKEY *takey;
    X509   *ca;
    X509   *cert = ctx->cert;		/* XXX: Accessor? */
    x509_stack_t *in = ctx->untrusted;	/* XXX: Accessor? */

    /* shallow copy */
    if ((in = sk_X509_dup(in)) == 0)
	msg_fatal("out of memory");

    /*
     * At each iteration we consume the issuer of the current cert.  This
     * reduces the length of the "in" chain by one.  If no issuer is found,
     * we are done.  We also stop when a certificate matches a TA in the
     * peer's TLSA RRset.
     * 
     * Caller ensures that the initial certificate is not self-signed.
     */
    for (n = sk_X509_num(in); n > 0; --n, ++depth) {
	for (i = 0; i < n; ++i)
	    if (X509_check_issued(sk_X509_value(in, i), cert) == X509_V_OK)
		break;

	/*
	 * Final untrusted element with no issuer in the peer's chain, it may
	 * however be signed by a pkey or cert obtained via a TLSA RR.
	 */
	if (i == n)
	    break;

	/* Peer's chain contains an issuer ca. */
	ca = sk_X509_delete(in, i);

	/* Is it a trust anchor? */
	if (tls_dane_match(TLScontext, TLS_DANE_TA, ca, depth + 1)) {
	    if ((takey = X509_get_pubkey(ca)) != 0
		&& wrap_key(TLScontext, takey, cert, depth))
		EVP_PKEY_free(takey);
	    cert = 0;
	    break;
	}
	/* Add untrusted ca. */
	grow_chain(&TLScontext->untrusted, ca, 0);

	/* Final untrusted self-signed element? */
	if (X509_check_issued(ca, ca) == X509_V_OK) {
	    cert = 0;
	    break;
	}
	/* Restart with issuer as subject */
	cert = ca;
    }

    /*
     * When the loop exits, if "cert" is set, it is not self-signed and has
     * no issuer in the chain, we check for a possible signature via a DNS
     * obtained TA cert or public key.  Otherwise, we found no TAs and no
     * issuer, so set an empty list of TAs.
     */
    if (!cert || !ta_signed(TLScontext, cert, depth)) {
	/* Create empty trust list if null, else NOP */
	grow_chain(&TLScontext->trusted, 0, 0);
    }
    /* shallow free */
    if (in)
	sk_X509_free(in);
}

/* dane_cb - wrap chain verification for DANE */

static int dane_cb(X509_STORE_CTX *ctx, void *app_ctx)
{
    const char *myname = "dane_cb";
    TLS_SESS_STATE *TLScontext = (TLS_SESS_STATE *) app_ctx;
    X509   *cert = ctx->cert;		/* XXX: accessor? */

    /*
     * Degenerate case: depth 0 self-signed cert.
     * 
     * XXX: Should we suppress name checks, ... when the leaf certificate is a
     * TA.  After all they could sign any name they want.  However, this
     * requires a bit of additional code.  For now we allow depth 0 TAs, but
     * then the peer name has to match.
     */
    if (X509_check_issued(cert, cert) == X509_V_OK) {

	/*
	 * Empty untrusted chain, could be NULL, but then ABI check less
	 * reliable, we may zero some other field, ...
	 */
	grow_chain(&TLScontext->untrusted, 0, 0);
	if (tls_dane_match(TLScontext, TLS_DANE_TA, cert, 0))
	    grow_chain(&TLScontext->trusted, cert, serverAuth);
	else
	    grow_chain(&TLScontext->trusted, 0, 0);
    } else {
	set_trust(TLScontext, ctx);
    }

    /*
     * Check that setting the untrusted chain updates the expected structure
     * member at the expected offset.
     */
    X509_STORE_CTX_trusted_stack(ctx, TLScontext->trusted);
    X509_STORE_CTX_set_chain(ctx, TLScontext->untrusted);
    if (ctx->untrusted != TLScontext->untrusted)
	msg_panic("%s: OpenSSL ABI change", myname);

    return X509_verify_cert(ctx);
}

/* tls_dane_set_callback - set or clear verification wrapper callback */

void    tls_dane_set_callback(SSL_CTX *ctx, TLS_SESS_STATE *TLScontext)
{
    if (!serverAuth || !TLS_DANE_HASTA(TLScontext->dane))
	SSL_CTX_set_cert_verify_callback(ctx, 0, 0);
    else
	SSL_CTX_set_cert_verify_callback(ctx, dane_cb, (void *) TLScontext);
}

#endif					/* USE_TLS */

