/*++
/* NAME
/*	tls_proxy_server_param_proto_test 1t
/* SUMMARY
/*	tls_proxy_server_param_proto unit test
/* SYNOPSIS
/*	./tls_proxy_server_param_proto_test
/* DESCRIPTION
/*	tls_proxy_server_param_proto_test runs and logs each configured test, reports if
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
#include <tls_proxy_server_param_proto.h>
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
char   *var_tls_cnf_file;
char   *var_tls_cnf_name;
char   *var_tls_high_clist;
char   *var_tls_medium_clist;
char   *var_tls_null_clist;
char   *var_tls_eecdh_auto;
char   *var_tls_eecdh_strong;
char   *var_tls_eecdh_ultra;
char   *var_tls_ffdhe_auto;
char   *var_tls_bug_tweaks;
char   *var_tls_ssl_options;
char   *var_tls_mgr_service;
char   *var_tls_tkt_cipher;
int     var_tls_daemon_rand_bytes;
bool    var_tls_append_def_CA;
bool    var_tls_preempt_clist;
bool    var_tls_multi_wildcard;
char   *var_tls_server_sni_maps;
bool    var_tls_fast_shutdown;
bool    var_tls_srvr_ccerts;

static void init_global_params(void)
{
    var_tls_cnf_file = DEF_TLS_CNF_FILE;
    var_tls_cnf_name = DEF_TLS_CNF_NAME;
    var_tls_high_clist = DEF_TLS_HIGH_CLIST;
    var_tls_medium_clist = DEF_TLS_MEDIUM_CLIST;
    var_tls_null_clist = DEF_TLS_NULL_CLIST;
    var_tls_eecdh_auto = DEF_TLS_EECDH_AUTO;
    var_tls_eecdh_strong = DEF_TLS_EECDH_STRONG;
    var_tls_eecdh_ultra = DEF_TLS_EECDH_ULTRA;
    var_tls_ffdhe_auto = DEF_TLS_FFDHE_AUTO;
    var_tls_bug_tweaks = DEF_TLS_BUG_TWEAKS;
    var_tls_ssl_options = DEF_TLS_SSL_OPTIONS;
    var_tls_mgr_service = DEF_TLS_MGR_SERVICE;
    var_tls_tkt_cipher = DEF_TLS_TKT_CIPHER;
    var_tls_daemon_rand_bytes = DEF_TLS_DAEMON_RAND_BYTES;
    var_tls_append_def_CA = DEF_TLS_APPEND_DEF_CA;
    var_tls_preempt_clist = DEF_TLS_PREEMPT_CLIST;
    var_tls_multi_wildcard = DEF_TLS_MULTI_WILDCARD;
    var_tls_server_sni_maps = DEF_TLS_SERVER_SNI_MAPS;
    var_tls_fast_shutdown = DEF_TLS_FAST_SHUTDOWN;
    var_tls_srvr_ccerts = DEF_TLS_SRVR_CCERTS;
}

static void setup_reference_unserialized_params(TLS_SERVER_PARAMS *params)
{
    TLS_PROXY_SERVER_PARAMS(params,
			    tls_cnf_file = var_tls_cnf_file,
			    tls_cnf_name = var_tls_cnf_name,
			    tls_high_clist = var_tls_high_clist,
			    tls_medium_clist = var_tls_medium_clist,
			    tls_null_clist = var_tls_null_clist,
			    tls_eecdh_auto = var_tls_eecdh_auto,
			    tls_eecdh_strong = var_tls_eecdh_strong,
			    tls_eecdh_ultra = var_tls_eecdh_ultra,
			    tls_ffdhe_auto = var_tls_ffdhe_auto,
			    tls_bug_tweaks = var_tls_bug_tweaks,
			    tls_ssl_options = var_tls_ssl_options,
			    tls_mgr_service = var_tls_mgr_service,
			    tls_tkt_cipher = var_tls_tkt_cipher,
			  tls_daemon_rand_bytes = var_tls_daemon_rand_bytes,
			    tls_append_def_CA = var_tls_append_def_CA,
			    tls_preempt_clist = var_tls_preempt_clist,
			    tls_multi_wildcard = var_tls_multi_wildcard,
			    tls_server_sni_maps = var_tls_server_sni_maps,
			    tls_fast_shutdown = var_tls_fast_shutdown,
			    tls_srvr_ccerts = var_tls_srvr_ccerts);
}

static VSTRING *setup_reference_serialized_params(TLS_SERVER_PARAMS *params)
{
    return (make_attr(attr_vprint, ATTR_FLAG_NONE,
		      SEND_ATTR_STR(TLS_ATTR_CNF_FILE, params->tls_cnf_file),
		      SEND_ATTR_STR(TLS_ATTR_CNF_NAME, params->tls_cnf_name),
		      SEND_ATTR_STR(VAR_TLS_HIGH_CLIST,
				    params->tls_high_clist),
		      SEND_ATTR_STR(VAR_TLS_MEDIUM_CLIST,
				    params->tls_medium_clist),
		      SEND_ATTR_STR(VAR_TLS_NULL_CLIST,
				    params->tls_null_clist),
		      SEND_ATTR_STR(VAR_TLS_EECDH_AUTO,
				    params->tls_eecdh_auto),
		      SEND_ATTR_STR(VAR_TLS_EECDH_STRONG,
				    params->tls_eecdh_strong),
		      SEND_ATTR_STR(VAR_TLS_EECDH_ULTRA,
				    params->tls_eecdh_ultra),
		      SEND_ATTR_STR(VAR_TLS_FFDHE_AUTO,
				    params->tls_ffdhe_auto),
		      SEND_ATTR_STR(VAR_TLS_BUG_TWEAKS,
				    params->tls_bug_tweaks),
		      SEND_ATTR_STR(VAR_TLS_SSL_OPTIONS,
				    params->tls_ssl_options),
		      SEND_ATTR_STR(VAR_TLS_MGR_SERVICE,
				    params->tls_mgr_service),
		      SEND_ATTR_STR(VAR_TLS_TKT_CIPHER,
				    params->tls_tkt_cipher),
		      SEND_ATTR_INT(VAR_TLS_DAEMON_RAND_BYTES,
				    params->tls_daemon_rand_bytes),
		      SEND_ATTR_INT(VAR_TLS_APPEND_DEF_CA,
				    params->tls_append_def_CA),
		      SEND_ATTR_INT(VAR_TLS_PREEMPT_CLIST,
				    params->tls_preempt_clist),
		      SEND_ATTR_INT(VAR_TLS_MULTI_WILDCARD,
				    params->tls_multi_wildcard),
		      SEND_ATTR_STR(VAR_TLS_SERVER_SNI_MAPS,
				    params->tls_server_sni_maps),
		      SEND_ATTR_INT(VAR_TLS_FAST_SHUTDOWN,
				    params->tls_fast_shutdown),
		      SEND_ATTR_INT(VAR_TLS_SRVR_CCERTS,
				    params->tls_srvr_ccerts),
		      ATTR_TYPE_END));
}

#endif

/* Note: this also tests tls_proxy_server_param_print() */

static void test_tls_proxy_server_param_serialize(PTEST_CTX *t,
				            const struct PTEST_CASE *unused)
{
#ifdef USE_TLS
    TLS_SERVER_PARAMS ref_unserialized_params;
    VSTRING *got_serialized_params;
    VSTRING *ref_serialized_params;

    init_global_params();

    setup_reference_unserialized_params(&ref_unserialized_params);

    ref_serialized_params = setup_reference_serialized_params(
						  &ref_unserialized_params);

    tls_proxy_server_param_serialize(attr_print,
				 got_serialized_params = vstring_alloc(100),
				   (const void *) &ref_unserialized_params);

    (void) eq_attr(t, "tls_proxy_server_param_serialize",
		   got_serialized_params, ref_serialized_params);
#else
    ptest_skip(t);
#endif
}

static void test_tls_proxy_server_param_from_config(PTEST_CTX *t,
				            const struct PTEST_CASE *unused)
{
#ifdef USE_TLS
    TLS_SERVER_PARAMS ref_unserialized_params;
    TLS_SERVER_PARAMS got_server_params;
    TLS_SERVER_PARAMS *p;
    VSTRING *want_server_params_serialized;
    VSTRING *got_server_params_serialized;

    init_global_params();

    setup_reference_unserialized_params(&ref_unserialized_params);

    tls_proxy_server_param_serialize(attr_print,
			 want_server_params_serialized = vstring_alloc(100),
				     &ref_unserialized_params);

    init_global_params();

    p = tls_proxy_server_param_from_config(&got_server_params);
    if (p != &got_server_params)
	ptest_fatal(t, "unexpected tls_proxy_server_param_from_config() result: got %p, and %p",
		    (void *) p, (void *) &got_server_params);

    tls_proxy_server_param_serialize(attr_print,
			  got_server_params_serialized = vstring_alloc(100),
				     &got_server_params);

    (void) eq_attr(t, "tls_proxy_server_param_from_config",
	       got_server_params_serialized, want_server_params_serialized);

    vstring_free(want_server_params_serialized);
    vstring_free(got_server_params_serialized);
#else
    ptest_skip(t);
#endif
}

/* Note: this also tests tls_proxy_server_param_scan() */

static void test_tls_proxy_server_param_from_string(PTEST_CTX *t,
				            const struct PTEST_CASE *unused)
{
#ifdef USE_TLS
    TLS_SERVER_PARAMS ref_unserialized_params;
    VSTRING *ref_serialized_params;
    VSTRING *got_serialized_params;
    TLS_SERVER_PARAMS *deserialized_params;

    init_global_params();

    setup_reference_unserialized_params(&ref_unserialized_params);

    ref_serialized_params = setup_reference_serialized_params(
						  &ref_unserialized_params);

    deserialized_params = tls_proxy_server_param_from_string(attr_scan,
						     ref_serialized_params);
    if (deserialized_params == 0)
	ptest_fatal(t, "tls_proxy_server_param_from_string failed");

    tls_proxy_server_param_serialize(attr_print,
				 got_serialized_params = vstring_alloc(100),
				     deserialized_params);

    eq_attr(t, "tls_proxy_server_param_from_string",
	    got_serialized_params, ref_serialized_params);

    vstring_free(ref_serialized_params);
    vstring_free(got_serialized_params);
#else
    ptest_skip(t);
#endif
}

 /*
  * The list of test cases.
  */
static const PTEST_CASE ptestcases[] = {
    "test_tls_proxy_server_param_serialize", test_tls_proxy_server_param_serialize,
    "test_tls_proxy_server_param_from_config", test_tls_proxy_server_param_from_config,
    "test_tls_proxy_server_param_from_string", test_tls_proxy_server_param_from_string,
};

#include <ptest_main.h>
