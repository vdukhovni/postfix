/*++
/* NAME
/*	tls_proxy_client_start_proto 3
/* SUMMARY
/*	Support for TLS_CLIENT_START structures
/* SYNOPSIS
/*	#include <tls_proxy_client_start_proto.h>
/*
/*	int	tls_proxy_client_start_print(print_fn, stream, flags, ptr)
/*	ATTR_PRINT_COMMON_FN print_fn;
/*	VSTREAM	*stream;
/*	int	flags;
/*	const void *ptr;
/*
/*	int	tls_proxy_client_start_scan(scan_fn, stream, flags, ptr)
/*	ATTR_SCAN_COMMON_FN scan_fn;
/*	VSTREAM	*stream;
/*	int	flags;
/*	void	*ptr;
/*
/*	void	tls_proxy_client_start_free(start_props)
/*	TLS_CLIENT_START_PROPS *start_props;
/* DESCRIPTION
/*	tls_proxy_client_start_print() writes a TLS_CLIENT_START_PROPS
/*	structure, without stream or file descriptor members, to
/*	the named stream using the specified attribute print routine.
/*	tls_proxy_client_start_print() is meant to be passed as a
/*	call-back to attr_print(), thusly:
/*
/*	SEND_ATTR_FUNC(tls_proxy_client_start_print, (const void *) start_props), ...
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
/*
/*	Wietse Venema
/*	porcupine.org
/*--*/

#ifdef USE_TLS

/* System library. */

#include <sys_defs.h>

/* Utility library */

#include <argv_attr.h>
#include <attr.h>
#include <msg.h>

/* Global library. */

#include <mail_params.h>

/* TLS library. */

#define TLS_INTERNAL
#include <tls.h>
#include <tls_proxy_attr.h>
#include <tls_proxy_client_start_proto.h>

#ifdef USE_TLSRPT
#define TLSRPT_WRAPPER_INTERNAL
#include <tlsrpt_wrapper.h>
#endif

#define STR(x) vstring_str(x)
#define LEN(x) VSTRING_LEN(x)

/* tls_proxy_client_tlsa_print - send TLS_TLSA over stream */

static int tls_proxy_client_tlsa_print(ATTR_PRINT_COMMON_FN print_fn,
			            VSTREAM *fp, int flags, const void *ptr)
{
    const TLS_TLSA *head = (const TLS_TLSA *) ptr;
    const TLS_TLSA *tp;
    int     count;
    int     ret;

    for (tp = head, count = 0; tp != 0; tp = tp->next)
	++count;
    if (msg_verbose)
	msg_info("tls_proxy_client_tlsa_print count=%d", count);

    ret = print_fn(fp, flags | ATTR_FLAG_MORE,
		   SEND_ATTR_INT(TLS_ATTR_COUNT, count),
		   ATTR_TYPE_END);

    for (tp = head; ret == 0 && tp != 0; tp = tp->next)
	ret = print_fn(fp, flags | ATTR_FLAG_MORE,
		       SEND_ATTR_INT(TLS_ATTR_USAGE, tp->usage),
		       SEND_ATTR_INT(TLS_ATTR_SELECTOR, tp->selector),
		       SEND_ATTR_INT(TLS_ATTR_MTYPE, tp->mtype),
		       SEND_ATTR_DATA(TLS_ATTR_DATA, tp->length, tp->data),
		       ATTR_TYPE_END);

    /* Do not flush the stream. */
    if (msg_verbose)
	msg_info("tls_proxy_client_tlsa_print ret=%d", count);
    return (ret);
}

/* tls_proxy_client_dane_print - send TLS_DANE over stream */

static int tls_proxy_client_dane_print(ATTR_PRINT_COMMON_FN print_fn,
			            VSTREAM *fp, int flags, const void *ptr)
{
    const TLS_DANE *dane = (const TLS_DANE *) ptr;
    int     ret;

    ret = print_fn(fp, flags | ATTR_FLAG_MORE,
		   SEND_ATTR_INT(TLS_ATTR_DANE, dane != 0),
		   ATTR_TYPE_END);
    if (msg_verbose)
	msg_info("tls_proxy_client_dane_print dane=%d", dane != 0);

#define STRING_OR_EMPTY(s) ((s) ? (s) : "")

    if (ret == 0 && dane != 0) {
	/* Send the base_domain and RRs, we don't need the other fields */
	ret = print_fn(fp, flags | ATTR_FLAG_MORE,
		       SEND_ATTR_STR(TLS_ATTR_DOMAIN,
				     STRING_OR_EMPTY(dane->base_domain)),
		       SEND_ATTR_FUNC(tls_proxy_client_tlsa_print,
				      (const void *) dane->tlsa),
		       ATTR_TYPE_END);
    }
    /* Do not flush the stream. */
    if (msg_verbose)
	msg_info("tls_proxy_client_dane_print ret=%d", ret);
    return (ret);
}

#ifdef USE_TLSRPT

/* tls_proxy_client_tlsrpt_print - send TLSRPT_WRAPPER over stream */

static int tls_proxy_client_tlsrpt_print(ATTR_PRINT_COMMON_FN print_fn,
			            VSTREAM *fp, int flags, const void *ptr)
{
    const TLSRPT_WRAPPER *trw = (const TLSRPT_WRAPPER *) ptr;
    int     have_trw = trw != 0;
    int     ret;

    ret = print_fn(fp, flags | ATTR_FLAG_MORE,
		   SEND_ATTR_INT(TLS_ATTR_TLSRPT, have_trw),
		   ATTR_TYPE_END);
    if (msg_verbose)
	msg_info("tls_proxy_client_tlsrpt_print have_trw=%d", have_trw);

    if (ret == 0 && have_trw) {
	ret = print_fn(fp, flags | ATTR_FLAG_MORE,
		       SEND_ATTR_STR(TRW_RPT_SOCKET_NAME,
				     STRING_OR_EMPTY(trw->rpt_socket_name)),
		       SEND_ATTR_STR(TRW_RPT_POLICY_DOMAIN,
				   STRING_OR_EMPTY(trw->rpt_policy_domain)),
		       SEND_ATTR_STR(TRW_RPT_POLICY_STRING,
				   STRING_OR_EMPTY(trw->rpt_policy_string)),
		       SEND_ATTR_INT(TRW_TLS_POLICY_TYPE,
				     (int) trw->tls_policy_type),
		       SEND_ATTR_FUNC(argv_attr_print,
				    (const void *) trw->tls_policy_strings),
		       SEND_ATTR_STR(TRW_TLS_POLICY_DOMAIN,
				   STRING_OR_EMPTY(trw->tls_policy_domain)),
		       SEND_ATTR_FUNC(argv_attr_print,
				      (const void *) trw->mx_host_patterns),
		       SEND_ATTR_STR(TRW_SRC_MTA_ADDR,
				     STRING_OR_EMPTY(trw->snd_mta_addr)),
		       SEND_ATTR_STR(TRW_DST_MTA_NAME,
				     STRING_OR_EMPTY(trw->rcv_mta_name)),
		       SEND_ATTR_STR(TRW_DST_MTA_ADDR,
				     STRING_OR_EMPTY(trw->rcv_mta_addr)),
		       SEND_ATTR_STR(TRW_DST_MTA_EHLO,
				     STRING_OR_EMPTY(trw->rcv_mta_ehlo)),
		       SEND_ATTR_INT(TRW_SKIP_REUSED_HS,
				     trw->skip_reused_hs),
		       SEND_ATTR_INT(TRW_FLAGS,
				     trw->flags),
		       ATTR_TYPE_END);
    }
    /* Do not flush the stream. */
    if (msg_verbose)
	msg_info("tls_proxy_client_tlsrpt_print ret=%d", ret);
    return (ret);
}

#endif

/* tls_proxy_client_start_print - send TLS_CLIENT_START_PROPS over stream */

int     tls_proxy_client_start_print(ATTR_PRINT_COMMON_FN print_fn,
			            VSTREAM *fp, int flags, const void *ptr)
{
    const TLS_CLIENT_START_PROPS *props = (const TLS_CLIENT_START_PROPS *) ptr;
    int     ret;

    if (msg_verbose)
	msg_info("begin tls_proxy_client_start_print");

#define STRING_OR_EMPTY(s) ((s) ? (s) : "")

    ret = print_fn(fp, flags | ATTR_FLAG_MORE,
		   SEND_ATTR_INT(TLS_ATTR_TIMEOUT, props->timeout),
		   SEND_ATTR_INT(TLS_ATTR_ENABLE_RPK, props->enable_rpk),
		   SEND_ATTR_INT(TLS_ATTR_TLS_LEVEL, props->tls_level),
		   SEND_ATTR_STR(TLS_ATTR_NEXTHOP,
				 STRING_OR_EMPTY(props->nexthop)),
		   SEND_ATTR_STR(TLS_ATTR_HOST,
				 STRING_OR_EMPTY(props->host)),
		   SEND_ATTR_STR(TLS_ATTR_NAMADDR,
				 STRING_OR_EMPTY(props->namaddr)),
		   SEND_ATTR_STR(TLS_ATTR_SNI,
				 STRING_OR_EMPTY(props->sni)),
		   SEND_ATTR_STR(TLS_ATTR_SERVERID,
				 STRING_OR_EMPTY(props->serverid)),
		   SEND_ATTR_STR(TLS_ATTR_HELO,
				 STRING_OR_EMPTY(props->helo)),
		   SEND_ATTR_STR(TLS_ATTR_PROTOCOLS,
				 STRING_OR_EMPTY(props->protocols)),
		   SEND_ATTR_STR(TLS_ATTR_CIPHER_GRADE,
				 STRING_OR_EMPTY(props->cipher_grade)),
		   SEND_ATTR_STR(TLS_ATTR_CIPHER_EXCLUSIONS,
				 STRING_OR_EMPTY(props->cipher_exclusions)),
		   SEND_ATTR_FUNC(argv_attr_print,
				  (const void *) props->matchargv),
		   SEND_ATTR_STR(TLS_ATTR_MDALG,
				 STRING_OR_EMPTY(props->mdalg)),
		   SEND_ATTR_FUNC(tls_proxy_client_dane_print,
				  (const void *) props->dane),
#ifdef USE_TLSRPT
		   SEND_ATTR_FUNC(tls_proxy_client_tlsrpt_print,
				  (const void *) props->tlsrpt),
#endif
		   SEND_ATTR_STR(TLS_ATTR_FFAIL_TYPE,
				 STRING_OR_EMPTY(props->ffail_type)),
		   ATTR_TYPE_END);
    /* Do not flush the stream. */
    if (msg_verbose)
	msg_info("tls_proxy_client_start_print ret=%d", ret);
    return (ret);
}

/* tls_proxy_client_start_free - destroy TLS_CLIENT_START_PROPS structure */

void    tls_proxy_client_start_free(TLS_CLIENT_START_PROPS *props)
{
    myfree((void *) props->nexthop);
    myfree((void *) props->host);
    myfree((void *) props->namaddr);
    myfree((void *) props->sni);
    myfree((void *) props->serverid);
    myfree((void *) props->helo);
    myfree((void *) props->protocols);
    myfree((void *) props->cipher_grade);
    myfree((void *) props->cipher_exclusions);
    if (props->matchargv)
	argv_free((ARGV *) props->matchargv);
    myfree((void *) props->mdalg);
    if (props->dane)
	tls_dane_free((TLS_DANE *) props->dane);
#ifdef USE_TLSRPT
    if (props->tlsrpt)
	trw_free(props->tlsrpt);
#endif
    if (props->ffail_type)
	myfree(props->ffail_type);
    myfree((void *) props);
}

/* tls_proxy_client_tlsa_scan - receive TLS_TLSA from stream */

static int tls_proxy_client_tlsa_scan(ATTR_SCAN_COMMON_FN scan_fn,
				          VSTREAM *fp, int flags, void *ptr)
{
    static VSTRING *data;
    TLS_TLSA *head;
    int     count;
    int     ret;

    if (data == 0)
	data = vstring_alloc(64);

    ret = scan_fn(fp, flags | ATTR_FLAG_MORE,
		  RECV_ATTR_INT(TLS_ATTR_COUNT, &count),
		  ATTR_TYPE_END);
    if (ret == 1 && msg_verbose)
	msg_info("tls_proxy_client_tlsa_scan count=%d", count);

    for (head = 0; ret == 1 && count > 0; --count) {
	int     u, s, m;

	ret = scan_fn(fp, flags | ATTR_FLAG_MORE,
		      RECV_ATTR_INT(TLS_ATTR_USAGE, &u),
		      RECV_ATTR_INT(TLS_ATTR_SELECTOR, &s),
		      RECV_ATTR_INT(TLS_ATTR_MTYPE, &m),
		      RECV_ATTR_DATA(TLS_ATTR_DATA, data),
		      ATTR_TYPE_END);
	if (ret == 4) {
	    ret = 1;
	    /* This makes a copy of the static vstring content */
	    head = tlsa_prepend(head, u, s, m, (unsigned char *) STR(data),
				LEN(data));
	} else
	    ret = -1;
    }

    if (ret != 1) {
	tls_tlsa_free(head);
	head = 0;
    }
    *(TLS_TLSA **) ptr = head;
    if (msg_verbose)
	msg_info("tls_proxy_client_tlsa_scan ret=%d", ret);
    return (ret);
}

/* tls_proxy_client_dane_scan - receive TLS_DANE from stream */

static int tls_proxy_client_dane_scan(ATTR_SCAN_COMMON_FN scan_fn,
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

	dane = tls_dane_alloc();
	/* We only need the base domain and TLSA RRs */
	ret = scan_fn(fp, flags | ATTR_FLAG_MORE,
		      RECV_ATTR_STR(TLS_ATTR_DOMAIN, base_domain),
		      RECV_ATTR_FUNC(tls_proxy_client_tlsa_scan,
				     &dane->tlsa),
		      ATTR_TYPE_END);

	/* Always construct a well-formed structure. */
	dane->base_domain = vstring_export(base_domain);
	ret = (ret == 2 ? 1 : -1);
	if (ret != 1) {
	    tls_dane_free(dane);
	    dane = 0;
	}
    }
    *(TLS_DANE **) ptr = dane;
    if (msg_verbose)
	msg_info("tls_proxy_client_dane_scan ret=%d", ret);
    return (ret);
}

#define EXPORT_OR_NULL(str, vstr) do { \
	if (LEN(vstr) > 0) { \
	    (str) = vstring_export(vstr); \
	} else { \
	    (str) = 0; \
	    vstring_free(vstr); \
	} \
    } while (0)

#ifdef USE_TLSRPT

/* tls_proxy_client_tlsrpt_scan - receive TLSRPT_WRAPPER from stream */

static int tls_proxy_client_tlsrpt_scan(ATTR_SCAN_COMMON_FN scan_fn,
				          VSTREAM *fp, int flags, void *ptr)
{
    TLSRPT_WRAPPER *trw = 0;
    int     ret;
    int     have_tlsrpt = 0;

    ret = scan_fn(fp, flags | ATTR_FLAG_MORE,
		  RECV_ATTR_INT(TLS_ATTR_TLSRPT, &have_tlsrpt),
		  ATTR_TYPE_END);
    if (msg_verbose)
	msg_info("tls_proxy_client_tlsrpt_scan have_tlsrpt=%d", have_tlsrpt);

    if (ret == 1 && have_tlsrpt) {
	VSTRING *rpt_socket_name = vstring_alloc(100);
	VSTRING *rpt_policy_domain = vstring_alloc(100);
	VSTRING *rpt_policy_string = vstring_alloc(100);
	int     tls_policy_type;
	ARGV   *tls_policy_strings = 0;
	VSTRING *tls_policy_domain = vstring_alloc(100);
	ARGV   *mx_host_patterns = 0;
	VSTRING *snd_mta_addr = vstring_alloc(100);
	VSTRING *rcv_mta_name = vstring_alloc(100);
	VSTRING *rcv_mta_addr = vstring_alloc(100);
	VSTRING *rcv_mta_ehlo = vstring_alloc(100);
	int     skip_reused_hs;
	int     trw_flags;

	ret = scan_fn(fp, flags | ATTR_FLAG_MORE,
		      RECV_ATTR_STR(TRW_RPT_SOCKET_NAME, rpt_socket_name),
		    RECV_ATTR_STR(TRW_RPT_POLICY_DOMAIN, rpt_policy_domain),
		    RECV_ATTR_STR(TRW_RPT_POLICY_STRING, rpt_policy_string),
		      RECV_ATTR_INT(TRW_TLS_POLICY_TYPE, &tls_policy_type),
		      RECV_ATTR_FUNC(argv_attr_scan, &tls_policy_strings),
		    RECV_ATTR_STR(TRW_TLS_POLICY_DOMAIN, tls_policy_domain),
		      RECV_ATTR_FUNC(argv_attr_scan, &mx_host_patterns),
		      RECV_ATTR_STR(TRW_SRC_MTA_ADDR, snd_mta_addr),
		      RECV_ATTR_STR(TRW_DST_MTA_NAME, rcv_mta_name),
		      RECV_ATTR_STR(TRW_DST_MTA_ADDR, rcv_mta_addr),
		      RECV_ATTR_STR(TRW_DST_MTA_EHLO, rcv_mta_ehlo),
		      RECV_ATTR_INT(TRW_SKIP_REUSED_HS, &skip_reused_hs),
		      RECV_ATTR_INT(TRW_FLAGS, &trw_flags),
		      ATTR_TYPE_END);

	/* Always construct a well-formed structure. */
	trw = (TLSRPT_WRAPPER *) mymalloc(sizeof(*trw));
	trw->rpt_socket_name = vstring_export(rpt_socket_name);
	trw->rpt_policy_domain = vstring_export(rpt_policy_domain);
	trw->rpt_policy_string = vstring_export(rpt_policy_string);
	trw->tls_policy_type = tls_policy_type;
	trw->tls_policy_strings = tls_policy_strings;
	EXPORT_OR_NULL(trw->tls_policy_domain, tls_policy_domain);
	trw->mx_host_patterns = mx_host_patterns;
	EXPORT_OR_NULL(trw->snd_mta_addr, snd_mta_addr);
	EXPORT_OR_NULL(trw->rcv_mta_name, rcv_mta_name);
	EXPORT_OR_NULL(trw->rcv_mta_addr, rcv_mta_addr);
	EXPORT_OR_NULL(trw->rcv_mta_ehlo, rcv_mta_ehlo);
	trw->skip_reused_hs = skip_reused_hs;
	trw->flags = trw_flags;
	ret = (ret == 13 ? 1 : -1);
	if (ret != 1) {
	    trw_free(trw);
	    trw = 0;
	}
    }
    *(TLSRPT_WRAPPER **) ptr = trw;
    if (msg_verbose)
	msg_info("tls_proxy_client_tlsrpt_scan ret=%d", ret);
    return (ret);
}

#endif

/* tls_proxy_client_start_scan - receive TLS_CLIENT_START_PROPS from stream */

int     tls_proxy_client_start_scan(ATTR_SCAN_COMMON_FN scan_fn, VSTREAM *fp,
				            int flags, void *ptr)
{
    TLS_CLIENT_START_PROPS *props
    = (TLS_CLIENT_START_PROPS *) mymalloc(sizeof(*props));
    int     ret;
    VSTRING *nexthop = vstring_alloc(25);
    VSTRING *host = vstring_alloc(25);
    VSTRING *namaddr = vstring_alloc(25);
    VSTRING *sni = vstring_alloc(25);
    VSTRING *serverid = vstring_alloc(25);
    VSTRING *helo = vstring_alloc(25);
    VSTRING *protocols = vstring_alloc(25);
    VSTRING *cipher_grade = vstring_alloc(25);
    VSTRING *cipher_exclusions = vstring_alloc(25);
    VSTRING *mdalg = vstring_alloc(25);
    VSTRING *ffail_type = vstring_alloc(25);

#ifdef USE_TLSRPT
#define EXPECT_START_SCAN_RETURN	17
#else
#define EXPECT_START_SCAN_RETURN	16
#endif

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
		  RECV_ATTR_INT(TLS_ATTR_ENABLE_RPK, &props->enable_rpk),
		  RECV_ATTR_INT(TLS_ATTR_TLS_LEVEL, &props->tls_level),
		  RECV_ATTR_STR(TLS_ATTR_NEXTHOP, nexthop),
		  RECV_ATTR_STR(TLS_ATTR_HOST, host),
		  RECV_ATTR_STR(TLS_ATTR_NAMADDR, namaddr),
		  RECV_ATTR_STR(TLS_ATTR_SNI, sni),
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
#ifdef USE_TLSRPT
		  RECV_ATTR_FUNC(tls_proxy_client_tlsrpt_scan,
				 &props->tlsrpt),
#endif
		  RECV_ATTR_STR(TLS_ATTR_FFAIL_TYPE, ffail_type),
		  ATTR_TYPE_END);
    /* Always construct a well-formed structure. */
    props->nexthop = vstring_export(nexthop);
    props->host = vstring_export(host);
    props->namaddr = vstring_export(namaddr);
    props->sni = vstring_export(sni);
    props->serverid = vstring_export(serverid);
    props->helo = vstring_export(helo);
    props->protocols = vstring_export(protocols);
    props->cipher_grade = vstring_export(cipher_grade);
    props->cipher_exclusions = vstring_export(cipher_exclusions);
    props->mdalg = vstring_export(mdalg);
    EXPORT_OR_NULL(props->ffail_type, ffail_type);
    ret = (ret == EXPECT_START_SCAN_RETURN ? 1 : -1);
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
