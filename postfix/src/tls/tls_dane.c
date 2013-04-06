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
/* DESCRIPTION
/*	tls_dane_avail() returns true if the features required to support DANE
/*	are present in OpenSSL's libcrypto and in libresolv.  Since OpenSSL's
/*	libcrypto is not initialized until we call tls_client_init(), calls
/*	to tls_dane_avail() must be deferred until this initialization is
/*	completed successufully.
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
#include <stdint.h>

#ifdef USE_TLS
#include <string.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <stringops.h>
#include <vstring.h>

#define STR(x)	vstring_str(x)

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
static TLS_TLSA **dane_locate(TLS_TLSA **, const char *);

IMPL_TLS_DANE_L(X509)
IMPL_TLS_DANE_L(EVP_PKEY)

/* tls_dane_verbose - enable/disable verbose logging */

void	tls_dane_verbose(int on)
{
    dane_verbose = on;
}

/* tls_dane_avail - check for availability of dane required digests */

int tls_dane_avail(void)
{
    static int avail = -1;
    const EVP_MD *sha256md;
    const EVP_MD *sha512md;

    if (avail >= 0)
	return avail;

    sha256md = EVP_get_digestbyname(sha256);
    sha512md = EVP_get_digestbyname(sha512);

    if (sha256md == 0 || sha512md == 0
	|| RES_USE_DNSSEC == 0 || RES_USE_EDNS0 == 0)
	return (avail = 0);

    sha256len = EVP_MD_size(sha256md);
    sha512len = EVP_MD_size(sha512md);

    return (avail = 1);
}

/* tls_dane_alloc - allocate a TLS_DANE structure */

TLS_DANE *tls_dane_alloc(int flags)
{
    TLS_DANE *dane = (TLS_DANE *)mymalloc(sizeof(*dane));

    dane->ta = 0;
    dane->ee = 0;
    dane->TLS_DANE_H(X509) = 0;
    dane->TLS_DANE_H(EVP_PKEY) = 0;
    dane->flags = flags;
    return (dane);
}

static void tlsa_free(TLS_TLSA *tlsa)
{

    myfree(tlsa->mdalg);
    if (tlsa->certs)
	argv_free(tlsa->certs);
    if (tlsa->pkeys)
	argv_free(tlsa->pkeys);
    myfree((char *)tlsa);
}

/* tls_dane_free - free a TLS_DANE structure */

void	tls_dane_free(TLS_DANE *dane)
{
    TLS_TLSA *tlsa;
    TLS_TLSA *next;

    /* De-allocate TA and EE lists */
    for (tlsa = dane->ta; tlsa; tlsa = next) {
	next = tlsa->next;
	tlsa_free(tlsa);
    }
    for (tlsa = dane->ee; tlsa; tlsa = next) {
	next = tlsa->next;
	tlsa_free(tlsa);
    }
    TLS_DANE_LFREE(X509)(dane);
    TLS_DANE_LFREE(EVP_PKEY)(dane);
    myfree((char *)dane);
}

/* dane_free - ctable style */

static void dane_free(void *dane, void *unused_context)
{
    tls_dane_free((TLS_DANE *)dane);
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
     * digest list that is sorted by algorithm name.  Below we maintain
     * the sort order (by algorithm name canonicalized to lowercase).
     */
    for (; *tlsap; tlsap = &(*tlsap)->next) {
	int     cmp = strcasecmp(mdalg, (*tlsap)->mdalg);

	if (cmp == 0)
	    return (tlsap);
	if (cmp < 0)
	    break;
    }

    new = (TLS_TLSA *)mymalloc(sizeof(*new));
    new->mdalg = lowercase(mystrdup(mdalg));
    new->certs = 0;
    new->pkeys = 0;
    new->next = *tlsap;
    *tlsap = new;

    return (tlsap);
}

/* tls_dane_split - split and append digests */

void tls_dane_split(TLS_DANE *dane, int certusage, int selector,
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
	certusage = TLS_DANE_TA;	/* Collapse 0/2 -> 2 */
	break;
    case DNS_TLSA_USAGE_SERVICE_CERTIFICATE_CONSTRAINT:
    case DNS_TLSA_USAGE_DOMAIN_ISSUED_CERTIFICATE:
	certusage = TLS_DANE_EE;	/* Collapse 1/3 -> 3 */
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

/* tls_dane_load_trustfile - load trust anchor certs or keys from file */

int	tls_dane_load_trustfile(TLS_DANE *dane, const char *tafile)
{
    FILE   *fp;
    char   *name = 0;
    char   *header = 0;
    long    error = 0;
    long    len;
    int     ret = 0;
    unsigned char *data = 0;

    /* nop */
    if (tafile == 0 || *tafile == 0)
	return (1);

    if ((fp = fopen(tafile, "r")) == NULL) {
	msg_warn("error opening trust anchor file: %s: %m", tafile);
	return (0);
    }

    /* Don't report old news */
    ERR_clear_error();

    while (PEM_read(fp, &name, &header, &data, &len)) {
	const unsigned char *p = data;
	int     selector = 0;

	if (strcmp(name, PEM_STRING_X509) == 0
	    || strcmp(name, PEM_STRING_X509_OLD) == 0) {
	    X509   *cert = d2i_X509(0, &p, len);

	    if (!(ret = (cert && (p - data) == len))) {
		msg_warn("error reading: %s: malformed trust-anchor %s",
			 "certificate", tafile);
		tls_print_errors();
		break;
	    }
	    TLS_DANE_LINSERT(X509)(dane, cert);
	    X509_free(cert);
	    selector = DNS_TLSA_SELECTOR_FULL_CERTIFICATE;
	} else if (strcmp(name, PEM_STRING_PUBLIC) == 0) {
	    EVP_PKEY *pkey = d2i_PUBKEY(0, &p, len);

	    if (!(ret = (pkey && (p - data) == len))) {
		msg_warn("error reading: %s: malformed trust-anchor %s",
			 "public key", tafile);
		tls_print_errors();
		break;
	    }
	    TLS_DANE_LINSERT(EVP_PKEY)(dane, pkey);
	    EVP_PKEY_free(pkey);
	    selector = DNS_TLSA_SELECTOR_SUBJECTPUBLICKEYINFO;
	}

	if (ret) {
	    int     usage = DNS_TLSA_USAGE_TRUST_ANCHOR_ASSERTION;
	    char   *digest = tls_fprint((char *)data, len, sha256);

	    dane_add(dane, usage, selector, sha256, digest);
	    myfree(digest);
	}
	if (name)
	    OPENSSL_free(name);
	if (header)
	    OPENSSL_free(header);
	if (data)
	    OPENSSL_free(data);
	name = header = (char *) (data = 0);
    }

    if (fclose(fp) == EOF) {
	msg_warn("error reading trust anchor file: %s: %m", tafile);
	return (0);
    }

    switch(ERR_GET_REASON(ERR_peek_last_error())) {
    case PEM_R_NO_START_LINE:
	ERR_clear_error();
	break;
    default:	/* Something to report */
	tls_print_errors();
    case 0:	/* All errors reported */
	ret = 0;
	break;
    }
    return (ret);
}

#endif
