/*++
/* NAME
/*	tls_proxy_client_init_proto_test 1t
/* SUMMARY
/*	tls_proxy_client_init_proto unit test
/* SYNOPSIS
/*	./tls_proxy_client_init_proto_test
/* DESCRIPTION
/*	tls_proxy_client_init_proto_test runs and logs each configured test, reports if
/*	a test is a PASS or FAIL, and returns an exit status of zero if
/*	all tests are a PASS.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>

 /*
  * Utility library.
  */

 /*
  * Global library.
  */
#include <mail_params.h>

 /*
  * TLS library.
  */
#include <tls_proxy_client_init_proto.h>
#include <tls_proxy_attr.h>

 /*
  * Test libraries.
  */
#include <ptest.h>
#include <make_attr.h>
#include <match_attr.h>

 /*
  * Test structure.
  */
typedef struct PTEST_CASE {
    const char *testname;
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
} PTEST_CASE;

#ifdef USE_TLS

 /*
  * Scaffolding: configuration parameters.
  */
char   *var_smtp_tls_loglevel;
int     var_smtp_tls_scert_vd;
static char *cache_type;
char   *var_smtp_tls_chain_files;
char   *var_smtp_tls_cert_file;
char   *var_smtp_tls_key_file;
char   *var_smtp_tls_dcert_file;
char   *var_smtp_tls_dkey_file;
char   *var_smtp_tls_eccert_file;
char   *var_smtp_tls_eckey_file;
char   *var_smtp_tls_CAfile;
char   *var_smtp_tls_CApath;
char   *var_smtp_tls_fpt_dgst;

static void init_global_params(void)
{
    var_smtp_tls_loglevel = DEF_SMTP_TLS_LOGLEVEL;
    var_smtp_tls_scert_vd = DEF_SMTP_TLS_SCERT_VD;
    cache_type = TLS_MGR_SCACHE_SMTP;
    var_smtp_tls_chain_files = DEF_SMTP_TLS_CHAIN_FILES;
    var_smtp_tls_cert_file = DEF_SMTP_TLS_CERT_FILE;
    var_smtp_tls_key_file = DEF_SMTP_TLS_CERT_FILE;
    var_smtp_tls_dcert_file = DEF_SMTP_TLS_DCERT_FILE;
    var_smtp_tls_dkey_file = DEF_SMTP_TLS_DCERT_FILE;
    var_smtp_tls_eccert_file = DEF_SMTP_TLS_ECCERT_FILE;
    var_smtp_tls_eckey_file = DEF_SMTP_TLS_ECCERT_FILE;
    var_smtp_tls_CAfile = DEF_SMTP_TLS_CA_FILE;
    var_smtp_tls_CApath = DEF_SMTP_TLS_CA_PATH;
    var_smtp_tls_fpt_dgst = DEF_SMTP_TLS_FPT_DGST;
}

static void setup_reference_unserialized_init_props(TLS_CLIENT_INIT_PROPS *props)
{
    TLS_PROXY_CLIENT_INIT_PROPS(props,
				log_param = VAR_SMTP_TLS_LOGLEVEL,
				log_level = var_smtp_tls_loglevel,
				verifydepth = var_smtp_tls_scert_vd,
				cache_type = cache_type,
				chain_files = var_smtp_tls_chain_files,
				cert_file = var_smtp_tls_cert_file,
				key_file = var_smtp_tls_key_file,
				dcert_file = var_smtp_tls_dcert_file,
				dkey_file = var_smtp_tls_dkey_file,
				eccert_file = var_smtp_tls_eccert_file,
				eckey_file = var_smtp_tls_eckey_file,
				CAfile = var_smtp_tls_CAfile,
				CApath = var_smtp_tls_CApath,
				mdalg = var_smtp_tls_fpt_dgst);
}

static VSTRING *setup_reference_serialized_init_props(TLS_CLIENT_INIT_PROPS *props)
{

    /*
     * Note: this code is used to verify tls_proxy_client_init_print(), so we
     * do not use that function here.
     */
#define STRING_OR_EMPTY(s) ((s) ? (s) : "")

    return (make_attr(attr_vprint, ATTR_FLAG_NONE,
		      SEND_ATTR_STR(TLS_ATTR_LOG_PARAM,
				    STRING_OR_EMPTY(props->log_param)),
		      SEND_ATTR_STR(TLS_ATTR_LOG_LEVEL,
				    STRING_OR_EMPTY(props->log_level)),
		      SEND_ATTR_INT(TLS_ATTR_VERIFYDEPTH,
				    props->verifydepth),
		      SEND_ATTR_STR(TLS_ATTR_CACHE_TYPE,
				    STRING_OR_EMPTY(props->cache_type)),
		      SEND_ATTR_STR(TLS_ATTR_CHAIN_FILES,
				    STRING_OR_EMPTY(props->chain_files)),
		      SEND_ATTR_STR(TLS_ATTR_CERT_FILE,
				    STRING_OR_EMPTY(props->cert_file)),
		      SEND_ATTR_STR(TLS_ATTR_KEY_FILE,
				    STRING_OR_EMPTY(props->key_file)),
		      SEND_ATTR_STR(TLS_ATTR_DCERT_FILE,
				    STRING_OR_EMPTY(props->dcert_file)),
		      SEND_ATTR_STR(TLS_ATTR_DKEY_FILE,
				    STRING_OR_EMPTY(props->dkey_file)),
		      SEND_ATTR_STR(TLS_ATTR_ECCERT_FILE,
				    STRING_OR_EMPTY(props->eccert_file)),
		      SEND_ATTR_STR(TLS_ATTR_ECKEY_FILE,
				    STRING_OR_EMPTY(props->eckey_file)),
		      SEND_ATTR_STR(TLS_ATTR_CAFILE,
				    STRING_OR_EMPTY(props->CAfile)),
		      SEND_ATTR_STR(TLS_ATTR_CAPATH,
				    STRING_OR_EMPTY(props->CApath)),
		      SEND_ATTR_STR(TLS_ATTR_MDALG,
				    STRING_OR_EMPTY(props->mdalg)),
		      ATTR_TYPE_END));
}

#endif

/* Note: this also tests tls_proxy_client_init_print() */

static void test_tls_proxy_client_init_serialize(PTEST_CTX *t,
				            const struct PTEST_CASE *unused)
{
#ifdef USE_TLS
    TLS_CLIENT_INIT_PROPS ref_unserialized_init_props;
    VSTRING *got_serialized_init_props;
    VSTRING *ref_serialized_init_props;

    init_global_params();

    setup_reference_unserialized_init_props(&ref_unserialized_init_props);

    ref_serialized_init_props = setup_reference_serialized_init_props(
					      &ref_unserialized_init_props);

    tls_proxy_client_init_serialize(attr_print,
			     got_serialized_init_props = vstring_alloc(100),
			       (const void *) &ref_unserialized_init_props);

    (void) eq_attr(t, "tls_proxy_client_init_serialize",
		   got_serialized_init_props, ref_serialized_init_props);
#else
    ptest_skip(t);
#endif
}

/* Note: this also tests tls_proxy_client_init_scan() */

static void test_tls_proxy_client_init_from_string(PTEST_CTX *t,
				            const struct PTEST_CASE *unused)
{
#ifdef USE_TLS
    TLS_CLIENT_INIT_PROPS ref_unserialized_init_props;
    VSTRING *ref_serialized_init_props;
    VSTRING *got_serialized_init_props;
    TLS_CLIENT_INIT_PROPS *deserialized_init_props;

    init_global_params();

    setup_reference_unserialized_init_props(&ref_unserialized_init_props);

    ref_serialized_init_props = setup_reference_serialized_init_props(
					      &ref_unserialized_init_props);

    deserialized_init_props = tls_proxy_client_init_from_string(attr_scan,
						 ref_serialized_init_props);
    if (deserialized_init_props == 0)
	ptest_fatal(t, "tls_proxy_client_init_from_string failed");

    tls_proxy_client_init_serialize(attr_print,
			     got_serialized_init_props = vstring_alloc(100),
				    deserialized_init_props);

    eq_attr(t, "tls_proxy_client_init_from_string",
	    got_serialized_init_props, ref_serialized_init_props);

    vstring_free(ref_serialized_init_props);
    vstring_free(got_serialized_init_props);
#else
    ptest_skip(t);
#endif
}

 /*
  * The list of test cases.
  */
static const PTEST_CASE ptestcases[] = {
    "test_tls_proxy_client_init_serialize", test_tls_proxy_client_init_serialize,
    "test_tls_proxy_client_init_from_string", test_tls_proxy_client_init_from_string,
};

#include <ptest_main.h>
