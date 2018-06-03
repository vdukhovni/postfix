/*++
/* NAME
/*	tls_proxy_client_scan 3
/* SUMMARY
/*	read TLS_CLIENT_XXX structures from stream
/* SYNOPSIS
/*	#include <tls_proxy.h>
/*
/*	int	tls_proxy_client_init_scan(scan_fn, stream, flags, ptr)
/*	ATTR_SCAN_MASTER_FN scan_fn;
/*	VSTREAM *stream;
/*	int     flags;
/*	void    *ptr;
/*
/*	char	*tls_proxy_client_init_to_string(buf, init_props)
/*	VSTRING *buf;
/*	TLS_CLIENT_INIT_PROPS *init_props;
/*
/*	void	tls_proxy_client_init_free(init_props)
/*	TLS_CLIENT_INIT_PROPS *init_props;
/*
/*	int	tls_proxy_client_start_scan(scan_fn, stream, flags, ptr)
/*	ATTR_SCAN_MASTER_FN scan_fn;
/*	VSTREAM *stream;
/*	int     flags;
/*	void    *ptr;
/*
/*	void	tls_proxy_client_start_free(start_props)
/*	TLS_CLIENT_START_PROPS *start_props;
/* DESCRIPTION
/*	tls_proxy_client_init_scan() reads a full TLS_CLIENT_INIT_PROPS
/*	structure from the named stream using the specified attribute
/*	scan routine. tls_proxy_client_init_scan() is meant to be passed
/*	as a call-back function to attr_scan(), as shown below.
/*
/*	tls_proxy_client_init_to_string() produces a lookup key
/*	that is unique for the properties received by
/*	tls_proxy_client_init_scan().
/*
/*	tls_proxy_client_init_free() destroys a TLS_CLIENT_INIT_PROPS
/*	structure that was created by tls_proxy_client_init_scan().
/*
/*	TLS_CLIENT_INIT_PROPS *init_props = 0;
/*	...
/*	... RECV_ATTR_FUNC(tls_proxy_client_init_scan, (void *) &init_props)
/*	...
/*	if (init_props != 0)
/*	    tls_proxy_client_init_free(init_props);
/*
/*	tls_proxy_client_start_scan() reads a TLS_CLIENT_START_PROPS
/*	structure, without the stream of file descriptor members,
/*	from the named stream using the specified attribute scan
/*	routine. tls_proxy_client_start_scan() is meant to be passed
/*	as a call-back function to attr_scan(), as shown below.
/*
/*	tls_proxy_client_start_free() destroys a TLS_CLIENT_START_PROPS
/*	structure that was created by tls_proxy_client_start_scan().
/*
/*	TLS_CLIENT_START_PROPS *start_props = 0;
/*	...
/*	... RECV_ATTR_FUNC(tls_proxy_client_start_scan, (void *) &start_props)
/*	...
/*	if (start_props != 0)
/*	    tls_proxy_client_start_free(start_props);
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

#include <argv_attr.h>
#include <attr.h>
#include <msg.h>
#include <vstring.h>

/* TLS library. */

#include <tls.h>
#include <tls_proxy.h>

#define STR(x) vstring_str(x)
#define LEN(x) VSTRING_LEN(x)

/* tls_proxy_client_init_free - destroy TLS_CLIENT_INIT_PROPS structure */

void    tls_proxy_client_init_free(TLS_CLIENT_INIT_PROPS *props)
{
    myfree((void *) props->log_param);
    myfree((void *) props->log_level);
    myfree((void *) props->cache_type);
    myfree((void *) props->cert_file);
    myfree((void *) props->key_file);
    myfree((void *) props->dcert_file);
    myfree((void *) props->dkey_file);
    myfree((void *) props->eccert_file);
    myfree((void *) props->eckey_file);
    myfree((void *) props->CAfile);
    myfree((void *) props->CApath);
    myfree((void *) props->mdalg);
    myfree((void *) props);
}

/* tls_proxy_client_init_scan - receive TLS_CLIENT_INIT_PROPS from stream */

int     tls_proxy_client_init_scan(ATTR_SCAN_MASTER_FN scan_fn, VSTREAM *fp,
				           int flags, void *ptr)
{
    TLS_CLIENT_INIT_PROPS *props
    = (TLS_CLIENT_INIT_PROPS *) mymalloc(sizeof(*props));
    int     ret;
    VSTRING *log_param = vstring_alloc(25);
    VSTRING *log_level = vstring_alloc(25);
    VSTRING *cache_type = vstring_alloc(25);
    VSTRING *cert_file = vstring_alloc(25);
    VSTRING *key_file = vstring_alloc(25);
    VSTRING *dcert_file = vstring_alloc(25);
    VSTRING *dkey_file = vstring_alloc(25);
    VSTRING *eccert_file = vstring_alloc(25);
    VSTRING *eckey_file = vstring_alloc(25);
    VSTRING *CAfile = vstring_alloc(25);
    VSTRING *CApath = vstring_alloc(25);
    VSTRING *mdalg = vstring_alloc(25);

    if (msg_verbose)
	msg_info("begin tls_proxy_client_init_scan");

    /*
     * Note: memset() is not a portable way to initialize non-integer types.
     */
    memset(props, 0, sizeof(*props));
    ret = scan_fn(fp, flags | ATTR_FLAG_MORE,
		  RECV_ATTR_STR(TLS_ATTR_LOG_PARAM, log_param),
		  RECV_ATTR_STR(TLS_ATTR_LOG_LEVEL, log_level),
		  RECV_ATTR_INT(TLS_ATTR_VERIFYDEPTH, &props->verifydepth),
		  RECV_ATTR_STR(TLS_ATTR_CACHE_TYPE, cache_type),
		  RECV_ATTR_STR(TLS_ATTR_CERT_FILE, cert_file),
		  RECV_ATTR_STR(TLS_ATTR_KEY_FILE, key_file),
		  RECV_ATTR_STR(TLS_ATTR_DCERT_FILE, dcert_file),
		  RECV_ATTR_STR(TLS_ATTR_DKEY_FILE, dkey_file),
		  RECV_ATTR_STR(TLS_ATTR_ECCERT_FILE, eccert_file),
		  RECV_ATTR_STR(TLS_ATTR_ECKEY_FILE, eckey_file),
		  RECV_ATTR_STR(TLS_ATTR_CAFILE, CAfile),
		  RECV_ATTR_STR(TLS_ATTR_CAPATH, CApath),
		  RECV_ATTR_STR(TLS_ATTR_MDALG, mdalg),
		  ATTR_TYPE_END);
    /* Always construct a well-formed structure. */
    props->log_param = vstring_export(log_param);
    props->log_level = vstring_export(log_level);
    props->cache_type = vstring_export(cache_type);
    props->cert_file = vstring_export(cert_file);
    props->key_file = vstring_export(key_file);
    props->dcert_file = vstring_export(dcert_file);
    props->dkey_file = vstring_export(dkey_file);
    props->eccert_file = vstring_export(eccert_file);
    props->eckey_file = vstring_export(eckey_file);
    props->CAfile = vstring_export(CAfile);
    props->CApath = vstring_export(CApath);
    props->mdalg = vstring_export(mdalg);
    ret = (ret == 13 ? 1 : -1);
    if (ret != 1) {
	tls_proxy_client_init_free(props);
	props = 0;
    }
    *(TLS_CLIENT_INIT_PROPS **) ptr = props;
    if (msg_verbose)
	msg_info("tls_proxy_client_init_scan ret=%d", ret);
    return (ret);
}

/* tls_proxy_client_init_to_string - serialize to string */

char   *tls_proxy_client_init_to_string(VSTRING *buf,
					        TLS_CLIENT_INIT_PROPS *props)
{
    vstring_sprintf(buf, "%s\n%s\n%d\n%s\n%s\n%s\n%s\n%s\n"
		    "%s\n%s\n%s\n%s\n%s\n", props->log_param,
		    props->log_level, props->verifydepth,
		    props->cache_type, props->cert_file, props->key_file,
		    props->dcert_file, props->dkey_file,
		    props->eccert_file, props->eckey_file,
		    props->CAfile, props->CApath, props->mdalg);
    return (vstring_str(buf));
}

/* tls_proxy_client_certs_free - destroy TLS_PKEYS from stream */

static void tls_proxy_client_certs_free(TLS_CERTS *tp)
{
    if (tp->next)
	tls_proxy_client_certs_free(tp->next);
    X509_free(tp->cert);
    myfree((void *) tp);
}

/* tls_proxy_client_pkeys_free - destroy TLS_PKEYS from stream */

static void tls_proxy_client_pkeys_free(TLS_PKEYS *tp)
{
    if (tp->next)
	tls_proxy_client_pkeys_free(tp->next);
    EVP_PKEY_free(tp->pkey);
    myfree((void *) tp);
}

/* tls_proxy_client_tlsa_free - destroy TLS_TLSA from stream */

static void tls_proxy_client_tlsa_free(TLS_TLSA *tp)
{
    if (tp->next)
	tls_proxy_client_tlsa_free(tp->next);
    myfree(tp->mdalg);
    if (tp->certs)
	argv_free(tp->certs);
    if (tp->pkeys)
	argv_free(tp->pkeys);
    myfree((void *) tp);
}

/* tls_proxy_client_dane_free - destroy TLS_DANE from stream */

static void tls_proxy_client_dane_free(TLS_DANE *dane)
{
    if (dane->ta)
	tls_proxy_client_tlsa_free(dane->ta);
    if (dane->ee)
	tls_proxy_client_tlsa_free(dane->ee);
    if (dane->certs)
	tls_proxy_client_certs_free(dane->certs);
    if (dane->pkeys)
	tls_proxy_client_pkeys_free(dane->pkeys);
    myfree(dane->base_domain);
    if (dane->refs-- == 1)
	myfree((void *) dane);
}

/* tls_proxy_client_start_free - destroy TLS_CLIENT_START_PROPS structure */

void    tls_proxy_client_start_free(TLS_CLIENT_START_PROPS *props)
{
    myfree((void *) props->nexthop);
    myfree((void *) props->host);
    myfree((void *) props->namaddr);
    myfree((void *) props->serverid);
    myfree((void *) props->helo);
    myfree((void *) props->protocols);
    myfree((void *) props->cipher_grade);
    myfree((void *) props->cipher_exclusions);
    if (props->matchargv)
	argv_free((ARGV *) props->matchargv);
    myfree((void *) props->mdalg);
    if (props->dane)
	tls_proxy_client_dane_free((TLS_DANE *) props->dane);
    myfree((void *) props);
}

/* tls_proxy_client_certs_scan - receive TLS_CERTS from stream */

static int tls_proxy_client_certs_scan(ATTR_SCAN_MASTER_FN scan_fn,
				          VSTREAM *fp, int flags, void *ptr)
{
    int     ret;
    int     count;
    VSTRING *buf = 0;
    TLS_CERTS **tpp;
    TLS_CERTS *head = 0;
    TLS_CERTS *tp;
    int     n;

    ret = scan_fn(fp, flags | ATTR_FLAG_MORE,
		  RECV_ATTR_INT(TLS_ATTR_COUNT, &count),
		  ATTR_TYPE_END);
    if (msg_verbose)
	msg_info("tls_proxy_client_certs_scan count=%d", count);

    for (tpp = &head, n = 0; ret == 1 && n < count; n++, tpp = &tp->next) {
	*tpp = tp = (TLS_CERTS *) mymalloc(sizeof(*tp));
	D2I_const unsigned char *bp;

	if (buf == 0)
	    buf = vstring_alloc(100);

	/*
	 * Note: memset() is not a portable way to initialize non-integer
	 * types.
	 */
	memset(tp, 0, sizeof(*tp));
	ret = scan_fn(fp, flags | ATTR_FLAG_MORE,
		      RECV_ATTR_DATA(TLS_ATTR_CERT, buf),
		      ATTR_TYPE_END);
	/* Always construct a well-formed structure. */
	if (ret == 1) {
	    bp = (D2I_const unsigned char *) STR(buf);
	    if (d2i_X509(&tp->cert, &bp, LEN(buf)) == 0
		|| LEN(buf) != ((char *) bp) - STR(buf)) {
		msg_warn("malformed certificate in TLS_CERTS");
		ret = -1;
	    }
	} else {
	    tp->cert = 0;
	}
	tp->next = 0;
    }
    if (buf)
	vstring_free(buf);
    if (ret != 1) {
	tls_proxy_client_certs_free(head);
	head = 0;
    }
    *(TLS_CERTS **) ptr = head;
    if (msg_verbose)
	msg_info("tls_proxy_client_certs_scan ret=%d", ret);
    return (ret);
}

/* tls_proxy_client_pkeys_scan - receive TLS_PKEYS from stream */

static int tls_proxy_client_pkeys_scan(ATTR_SCAN_MASTER_FN scan_fn,
				          VSTREAM *fp, int flags, void *ptr)
{
    int     ret;
    int     count;
    VSTRING *buf = vstring_alloc(100);
    TLS_PKEYS **tpp;
    TLS_PKEYS *head = 0;
    TLS_PKEYS *tp;
    int     n;

    ret = scan_fn(fp, flags | ATTR_FLAG_MORE,
		  RECV_ATTR_INT(TLS_ATTR_COUNT, &count),
		  ATTR_TYPE_END);
    if (msg_verbose)
	msg_info("tls_proxy_client_pkeys_scan count=%d", count);

    for (tpp = &head, n = 0; ret == 1 && n < count; n++, tpp = &tp->next) {
	*tpp = tp = (TLS_PKEYS *) mymalloc(sizeof(*tp));
	D2I_const unsigned char *bp;

	if (buf == 0)
	    buf = vstring_alloc(100);

	/*
	 * Note: memset() is not a portable way to initialize non-integer
	 * types.
	 */
	memset(tp, 0, sizeof(*tp));
	ret = scan_fn(fp, flags | ATTR_FLAG_MORE,
		      RECV_ATTR_DATA(TLS_ATTR_PKEY, buf),
		      ATTR_TYPE_END);
	/* Always construct a well-formed structure. */
	if (ret == 1) {
	    bp = (D2I_const unsigned char *) STR(buf);
	    if (d2i_PUBKEY(&tp->pkey, &bp, LEN(buf)) == 0
		|| LEN(buf) != (char *) bp - STR(buf)) {
		msg_warn("malformed public key in TLS_PKEYS");
		ret = -1;
	    }
	} else {
	    tp->pkey = 0;
	}
	tp->next = 0;
    }
    if (buf)
	vstring_free(buf);
    if (ret != 1) {
	tls_proxy_client_pkeys_free(head);
	head = 0;
    }
    *(TLS_PKEYS **) ptr = head;
    if (msg_verbose)
	msg_info("tls_proxy_client_pkeys_scan ret=%d", ret);
    return (ret);
}

/* tls_proxy_client_tlsa_scan - receive TLS_TLSA from stream */

static int tls_proxy_client_tlsa_scan(ATTR_SCAN_MASTER_FN scan_fn,
				          VSTREAM *fp, int flags, void *ptr)
{
    int     ret;
    int     count;
    TLS_TLSA **tpp;
    TLS_TLSA *head = 0;
    TLS_TLSA *tp;
    int     n;

    ret = scan_fn(fp, flags | ATTR_FLAG_MORE,
		  RECV_ATTR_INT(TLS_ATTR_COUNT, &count),
		  ATTR_TYPE_END);
    if (msg_verbose)
	msg_info("tls_proxy_client_tlsa_scan count=%d", count);

    for (tpp = &head, n = 0; ret == 1 && n < count; n++, tpp = &tp->next) {
	*tpp = tp = (TLS_TLSA *) mymalloc(sizeof(*tp));
	VSTRING *mdalg = vstring_alloc(25);

	/*
	 * Note: memset() is not a portable way to initialize non-integer
	 * types.
	 */
	memset(tp, 0, sizeof(*tp));
	/* Always construct a well-formed structure. */
	tp->certs = 0;				/* scan_fn may return early */
	tp->pkeys = 0;
	ret = scan_fn(fp, flags | ATTR_FLAG_MORE,
		      RECV_ATTR_STR(TLS_ATTR_MDALG, mdalg),
		      RECV_ATTR_FUNC(argv_attr_scan, &tp->certs),
		      RECV_ATTR_FUNC(argv_attr_scan, &tp->pkeys),
		      ATTR_TYPE_END);
	tp->mdalg = vstring_export(mdalg);
	tp->next = 0;
	ret = (ret == 3 ? 1 : -1);
    }
    if (ret != 1) {
	tls_proxy_client_tlsa_free(head);
	head = 0;
    }
    *(TLS_TLSA **) ptr = head;
    if (msg_verbose)
	msg_info("tls_proxy_client_tlsa_scan ret=%d", ret);
    return (ret);
}

/* tls_proxy_client_dane_scan - receive TLS_DANE from stream */

static int tls_proxy_client_dane_scan(ATTR_SCAN_MASTER_FN scan_fn,
				          VSTREAM *fp, int flags, void *ptr)
{
    TLS_DANE *dane = 0;
    int     ret;
    int     have_dane = 0;

    ret = scan_fn(fp, flags | ATTR_FLAG_MORE,
		  RECV_ATTR_INT(TLS_ATTR_DANE, &have_dane),
		  ATTR_TYPE_END);
    if (msg_verbose)
	msg_info("tls_proxy_client_dane_scan have_dane=%d", have_dane);

    if (ret == 1 && have_dane) {
	VSTRING *base_domain = vstring_alloc(25);
	long    expires;

	dane = (TLS_DANE *) mymalloc(sizeof(*dane));

	/*
	 * Note: memset() is not a portable way to initialize non-integer
	 * types.
	 */
	memset(dane, 0, sizeof(*dane));
	/* Always construct a well-formed structure. */
	dane->ta = 0;				/* scan_fn may return early */
	dane->ee = 0;
	dane->certs = 0;
	dane->pkeys = 0;
	ret = scan_fn(fp, flags | ATTR_FLAG_MORE,
		      RECV_ATTR_FUNC(tls_proxy_client_tlsa_scan,
				     &dane->ta),
		      RECV_ATTR_FUNC(tls_proxy_client_tlsa_scan,
				     &dane->ee),
		      RECV_ATTR_FUNC(tls_proxy_client_certs_scan,
				     &dane->certs),
		      RECV_ATTR_FUNC(tls_proxy_client_pkeys_scan,
				     &dane->pkeys),
		      RECV_ATTR_STR(TLS_ATTR_DOMAIN, base_domain),
		      RECV_ATTR_INT(TLS_ATTR_FLAGS, &dane->flags),
		      RECV_ATTR_LONG(TLS_ATTR_EXP, &expires),
		      ATTR_TYPE_END);
	/* Always construct a well-formed structure. */
	dane->base_domain = vstring_export(base_domain);
	dane->expires = expires;
	dane->refs = 1;
	ret = (ret == 7 ? 1 : -1);
	/* XXX if scan_fn() completed normally, sanity check dane->flags. */
	if (ret != 1) {
	    tls_proxy_client_dane_free(dane);
	    dane = 0;
	}
    }
    *(TLS_DANE **) ptr = dane;
    if (msg_verbose)
	msg_info("tls_proxy_client_dane_scan ret=%d", ret);
    return (ret);
}

/* tls_proxy_client_start_scan - receive TLS_CLIENT_START_PROPS from stream */

int     tls_proxy_client_start_scan(ATTR_SCAN_MASTER_FN scan_fn, VSTREAM *fp,
				            int flags, void *ptr)
{
    TLS_CLIENT_START_PROPS *props
    = (TLS_CLIENT_START_PROPS *) mymalloc(sizeof(*props));
    int     ret;
    VSTRING *nexthop = vstring_alloc(25);
    VSTRING *host = vstring_alloc(25);
    VSTRING *namaddr = vstring_alloc(25);
    VSTRING *serverid = vstring_alloc(25);
    VSTRING *helo = vstring_alloc(25);
    VSTRING *protocols = vstring_alloc(25);
    VSTRING *cipher_grade = vstring_alloc(25);
    VSTRING *cipher_exclusions = vstring_alloc(25);
    VSTRING *mdalg = vstring_alloc(25);

    if (msg_verbose)
	msg_info("begin tls_proxy_client_start_scan");

    /*
     * Note: memset() is not a portable way to initialize non-integer types.
     */
    memset(props, 0, sizeof(*props));
    props->ctx = 0;
    props->stream = 0;
    props->fd = -1;
    props->dane = 0;				/* scan_fn may return early */
    ret = scan_fn(fp, flags | ATTR_FLAG_MORE,
		  RECV_ATTR_INT(TLS_ATTR_TIMEOUT, &props->timeout),
		  RECV_ATTR_INT(TLS_ATTR_TLS_LEVEL, &props->tls_level),
		  RECV_ATTR_STR(TLS_ATTR_NEXTHOP, nexthop),
		  RECV_ATTR_STR(TLS_ATTR_HOST, host),
		  RECV_ATTR_STR(TLS_ATTR_NAMADDR, namaddr),
		  RECV_ATTR_STR(TLS_ATTR_SERVERID, serverid),
		  RECV_ATTR_STR(TLS_ATTR_HELO, helo),
		  RECV_ATTR_STR(TLS_ATTR_PROTOCOLS, protocols),
		  RECV_ATTR_STR(TLS_ATTR_CIPHER_GRADE, cipher_grade),
		  RECV_ATTR_STR(TLS_ATTR_CIPHER_EXCLUSIONS,
				cipher_exclusions),
		  RECV_ATTR_FUNC(argv_attr_scan, &props->matchargv),
		  RECV_ATTR_STR(TLS_ATTR_MDALG, mdalg),
		  RECV_ATTR_FUNC(tls_proxy_client_dane_scan,
				 &props->dane),
		  ATTR_TYPE_END);
    /* Always construct a well-formed structure. */
    props->nexthop = vstring_export(nexthop);
    props->host = vstring_export(host);
    props->namaddr = vstring_export(namaddr);
    props->serverid = vstring_export(serverid);
    props->helo = vstring_export(helo);
    props->protocols = vstring_export(protocols);
    props->cipher_grade = vstring_export(cipher_grade);
    props->cipher_exclusions = vstring_export(cipher_exclusions);
    props->mdalg = vstring_export(mdalg);
    ret = (ret == 13 ? 1 : -1);
    if (ret != 1) {
	tls_proxy_client_start_free(props);
	props = 0;
    }
    *(TLS_CLIENT_START_PROPS **) ptr = props;
    if (msg_verbose)
	msg_info("tls_proxy_client_start_scan ret=%d", ret);
    return (ret);
}

#endif
