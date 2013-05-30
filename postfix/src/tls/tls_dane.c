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
/*	TLS_DANE *tls_dane_final(dane)
/*	TLS_DANE *dane;
/*
/*	TLS_DANE *tls_dane_resolve(host, proto, port)
/*	const char *host;
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
/*	Once all the entries have been added, the caller must call
/*	tls_dane_final() to complete its construction.
/*
/*	tls_dane_load_trustfile() imports trust-anchor certificates and
/*	public keys from a file (rather than DNS TLSA records).
/*
/*	tls_dane_final() completes the construction of a TLS_DANE structure,
/*	obtained via tls_dane_alloc() and populated via tls_dane_split() or
/*	tls_dane_load_trustfile().  After tls_dane_final() is called, the
/*	structure must not be modified.  The return value is the input
/*	argument.
/*
/*	tls_dane_resolve() maps a (host, protocol, port) triple to a
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
/* .IP host
/*	DNSSEC validated (cname resolved) FQDN of target service.
/* .IP proto
/*	Almost certainly "tcp".
/* .IP port
/*	The TCP port in network byte order.
/* .IP flags
/*	Only one flag is part of the public interface at this time:
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

#define STR(x)	vstring_str(x)

/* Global library */

#include <mail_params.h>

/* DNS library. */

#include <dns.h>

/* TLS library. */

#define TLS_INTERNAL
#include <tls.h>

/* Application-specific. */

static const char *sha256 = "sha256";
static const char *sha512 = "sha512";
static int sha256len;
static int sha512len;
static int dane_verbose;
static int digest_mask;
static TLS_TLSA **dane_locate(TLS_TLSA **, const char *);

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

/* tls_dane_verbose - enable/disable verbose logging */

void    tls_dane_verbose(int on)
{
    dane_verbose = on;
}

/* tls_dane_avail - check for availability of dane required digests */

int     tls_dane_avail(void)
{
#ifdef TLSEXT_MAXLEN_host_name		/* DANE mandates client SNI. */
    static int avail = -1;
    const EVP_MD *sha256md;
    const EVP_MD *sha512md;
    static NAME_MASK ta_dgsts[] = {
	TLS_DANE_CC, TLS_DANE_ENABLE_CC,
	TLS_DANE_TAA, TLS_DANE_ENABLE_TAA,
	0,
    };

    if (avail >= 0)
	return (avail);

    sha256md = EVP_get_digestbyname(sha256);
    sha512md = EVP_get_digestbyname(sha512);

    if (sha256md == 0 || sha512md == 0
	|| RES_USE_DNSSEC == 0 || RES_USE_EDNS0 == 0)
	return (avail = 0);

    digest_mask =
	name_mask_opt(VAR_TLS_DANE_TA_DGST, ta_dgsts, var_tls_dane_ta_dgst,
		      NAME_MASK_ANY_CASE | NAME_MASK_FATAL);

    sha256len = EVP_MD_size(sha256md);
    sha512len = EVP_MD_size(sha512md);

    return (avail = 1);
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

    myfree((char *) dane);
}

/* dane_free - ctable style */

static void dane_free(void *dane, void *unused_context)
{
    tls_dane_free((TLS_DANE *) dane);
}

/* tlsa_sort - sort digests for a single certusage */

static void tlsa_sort(TLS_TLSA *tlsa)
{
    for (; tlsa; tlsa = tlsa->next) {
	if (tlsa->pkeys)
	    argv_sort(tlsa->pkeys);
	if (tlsa->certs)
	    argv_sort(tlsa->certs);
    }
}

/* tls_dane_final - finish by sorting into canonical order */

TLS_DANE *tls_dane_final(TLS_DANE *dane)
{

    /*
     * We only sort the trust anchors, see tls_serverid_digest().
     */
    if (dane->ta)
	tlsa_sort(dane->ta);
    dane->flags |= TLS_DANE_FLAG_FINAL;
    return (dane);
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

    if (dane->flags & TLS_DANE_FLAG_FINAL)
	msg_panic("updating frozen TLS_DANE object");

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

    if (dane->flags & TLS_DANE_FLAG_FINAL)
	msg_panic("updating frozen TLS_DANE object");

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

/* parse_tlsa_rrs - parse a validated TLSA RRset */

static void parse_tlsa_rrs(TLS_DANE *dane, DNS_RR *rr)
{
    uint8_t usage;
    uint8_t selector;
    uint8_t mtype;
    int     mlen;
    const unsigned char *p;
    X509   *x = 0;			/* OpenSSL tries to re-use *x if x!=0 */
    EVP_PKEY *k = 0;			/* OpenSSL tries to re-use *k if k!=0 */

    if (rr == 0)
	msg_panic("null TLSA rr");

    for ( /* nop */ ; rr; rr = rr->next) {
	const char *mdalg = 0;
	int     mdlen;
	char   *digest;
	int     same = (strcasecmp(rr->rname, rr->qname) == 0);
	uint8_t *ip = (uint8_t *) rr->data;

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
	    dane_add(dane, usage, selector, sha256,
		     digest = tls_data_fprint((char *) ip, mlen, sha256));
	    break;
	}
	if (msg_verbose || dane_verbose)
	    msg_info("using DANE RR: %s%s%s IN TLSA %u %u %u %s",
		     rcname(rr), rarrow(rr), rr->rname,
		     usage, selector, mtype, digest);
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

	if (rrs->dnssec_valid)
	    parse_tlsa_rrs(dane, rrs);

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

    return ((void *) tls_dane_final(dane));
}

/* tls_dane_resolve - cached map: (host, proto, port) -> TLS_DANE */

TLS_DANE *tls_dane_resolve(const char *host, const char *proto,
			           unsigned port)
{
    static VSTRING *qname;
    TLS_DANE *dane;

    if (!tls_dane_avail())
	return (0);

    if (!dane_cache)
	dane_cache = ctable_create(CACHE_SIZE, dane_lookup, dane_free, 0);

    if (qname == 0)
	qname = vstring_alloc(64);
    vstring_sprintf(qname, "_%u._%s.%s", ntohs(port), proto, host);
    dane = (TLS_DANE *) ctable_locate(dane_cache, STR(qname));
    if (timecmp(event_time(), dane->expires) > 0)
	dane = (TLS_DANE *) ctable_refresh(dane_cache, STR(qname));

    if (dane->flags & TLS_DANE_FLAG_ERROR)
	return (0);

    ++dane->refs;
    return (dane);
}

/* tls_dane_load_trustfile - load trust anchor certs or keys from file */

int     tls_dane_load_trustfile(TLS_DANE *dane, const char *tafile)
{
    BIO    *bp;
    char   *name = 0;
    char   *header = 0;
    unsigned char *data = 0;
    long    len;
    int     tacount;
    char   *errtype = 0;		/* if error: cert or pkey? */

    /* nop */
    if (tafile == 0 || *tafile == 0)
	return (1);

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
		digest = tls_data_fprint((char *) data, len, sha256);
		dane_add(dane, usage, selector, sha256, digest);
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
		digest = tls_data_fprint((char *) data, len, sha256);
		dane_add(dane, usage, selector, sha256, digest);
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
    return (0);
}

#endif
