 /*
  * Test program to exercise haproxy_srvr.c. See ptest_main.h for a
  * documented example.
  */

 /*
  * System library.
  */
#include <sys_defs.h>
#include <string.h>

 /*
  * Utility library.
  */
#include <sock_addr.h>
#include <vstring.h>

 /*
  * Global library.
  */
#define HAPROXY_SRVR_INTERNAL
#include <haproxy_srvr.h>

 /*
  * Test library.
  */
#include <ptest.h>

 /*
  * Test cases with inputs and expected outputs. A request may contain
  * trailing garbage, and it may be too short. A v1 request may also contain
  * malformed address or port information.
  */
typedef struct BASE_TEST_CASE {
    const char *haproxy_request;	/* v1 or v2 request including thrash */
    ssize_t haproxy_req_len;		/* request length including thrash */
    ssize_t want_req_len;		/* parsed request length */
    int     want_non_proxy;		/* request is not proxied */
    const char *want_return;		/* expected error string */
    const char *want_client_addr;	/* expected client address string */
    const char *want_server_addr;	/* expected client port string */
    const char *want_client_port;	/* expected client address string */
    const char *want_server_port;	/* expected server port string */
} BASE_TEST_CASE;

 /*
  * Initialize the haproxy_request, haproxy_req_len, and want_req_len
  * fields.
  */
#define STRING_AND_LENS(s) (s), (sizeof(s) - 1), (sizeof(s) - 1)

 /*
  * This table contains V1 protocol test cases that may also be converted
  * into V2 protocol test cases.
  */
static BASE_TEST_CASE v1_test_cases[] = {
    /* IPv6. */
    {STRING_AND_LENS("PROXY TCP6 fc:00:00:00:1:2:3:4 fc:00:00:00:4:3:2:1 123 321\n"), 0, 0, "fc::1:2:3:4", "fc::4:3:2:1", "123", "321"},
    {STRING_AND_LENS("PROXY TCP6 FC:00:00:00:1:2:3:4 FC:00:00:00:4:3:2:1 123 321\n"), 0, 0, "fc::1:2:3:4", "fc::4:3:2:1", "123", "321"},
    {STRING_AND_LENS("PROXY TCP6 1.2.3.4 4.3.2.1 123 321\n"), 0, "bad or missing client address"},
    {STRING_AND_LENS("PROXY TCP6 fc:00:00:00:1:2:3:4 4.3.2.1 123 321\n"), 0, "bad or missing server address"},
    /* IPv4 in IPv6. */
    {STRING_AND_LENS("PROXY TCP6 ::ffff:1.2.3.4 ::ffff:4.3.2.1 123 321\n"), 0, 0, "1.2.3.4", "4.3.2.1", "123", "321"},
    {STRING_AND_LENS("PROXY TCP6 ::FFFF:1.2.3.4 ::FFFF:4.3.2.1 123 321\n"), 0, 0, "1.2.3.4", "4.3.2.1", "123", "321"},
    {STRING_AND_LENS("PROXY TCP4 ::ffff:1.2.3.4 ::ffff:4.3.2.1 123 321\n"), 0, "bad or missing client address"},
    {STRING_AND_LENS("PROXY TCP4 1.2.3.4 ::ffff:4.3.2.1 123 321\n"), 0, "bad or missing server address"},
    /* IPv4. */
    {STRING_AND_LENS("PROXY TCP4 1.2.3.4 4.3.2.1 123 321\n"), 0, 0, "1.2.3.4", "4.3.2.1", "123", "321"},
    {STRING_AND_LENS("PROXY TCP4 01.02.03.04 04.03.02.01 123 321\n"), 0, 0, "1.2.3.4", "4.3.2.1", "123", "321"},
    {STRING_AND_LENS("PROXY TCP4 1.2.3.4 4.3.2.1 123456 321\n"), 0, "bad or missing client port"},
    {STRING_AND_LENS("PROXY TCP4 1.2.3.4 4.3.2.1 123 654321\n"), 0, "bad or missing server port"},
    {STRING_AND_LENS("PROXY TCP4 1.2.3.4 4.3.2.1 0123 321\n"), 0, "bad or missing client port"},
    {STRING_AND_LENS("PROXY TCP4 1.2.3.4 4.3.2.1 123 0321\n"), 0, "bad or missing server port"},
    /* Missing fields. */
    {STRING_AND_LENS("PROXY TCP6 fc:00:00:00:1:2:3:4 fc:00:00:00:4:3:2:1 123\n"), 0, "bad or missing server port"},
    {STRING_AND_LENS("PROXY TCP6 fc:00:00:00:1:2:3:4 fc:00:00:00:4:3:2:1\n"), 0, "bad or missing client port"},
    {STRING_AND_LENS("PROXY TCP6 fc:00:00:00:1:2:3:4\n"), 0, "bad or missing server address"},
    {STRING_AND_LENS("PROXY TCP6\n"), 0, "bad or missing client address"},
    {STRING_AND_LENS("PROXY TCP4 1.2.3.4 4.3.2.1 123\n"), 0, "bad or missing server port"},
    {STRING_AND_LENS("PROXY TCP4 1.2.3.4 4.3.2.1\n"), 0, "bad or missing client port"},
    {STRING_AND_LENS("PROXY TCP4 1.2.3.4\n"), 0, "bad or missing server address"},
    {STRING_AND_LENS("PROXY TCP4\n"), 0, "bad or missing client address"},
    /* Other. */
    {STRING_AND_LENS("PROXY BLAH\n"), 0, "bad or missing protocol type"},
    {STRING_AND_LENS("PROXY\n"), 0, "short protocol header"},
    {"BLAH\n", 0, 0, 0, "short protocol header"},
    {"\n", 0, 0, 0, "short protocol header"},
    {"", 0, 0, 0, "short protocol header"},
    0,
};

 /*
  * Limited V2-only tests. XXX Should we add error cases? Several errors are
  * already tested with mutations of V2 handshakes that were generated from
  * V1 handshakes.
  */
static struct proxy_hdr_v2 v2_local_request = {
    PP2_SIGNATURE, PP2_VERSION | PP2_CMD_LOCAL,
};
static BASE_TEST_CASE v2_non_proxy_test = {
    (char *) &v2_local_request, PP2_HEADER_LEN, PP2_HEADER_LEN, 1,
};

#define STR_OR_NULL(s)	((s) ? (s) : "(null)")
#define STR(x)		vstring_str(x)
#define LEN(x)		VSTRING_LEN(x)

/* evaluate_test_case - evaluate one test case (base or mutation) */

static void evaluate_test_case(PTEST_CTX *t,const char *test_label,
			              const BASE_TEST_CASE *test_case)
{
    PTEST_RUN(t, test_label, {
	const char *got_return;
	ssize_t got_req_len;
	int    got_non_proxy;
	MAI_HOSTADDR_STR got_smtp_client_addr;
	MAI_HOSTADDR_STR got_smtp_server_addr;
	MAI_SERVPORT_STR got_smtp_client_port;
	MAI_SERVPORT_STR got_smtp_server_port;

	if (msg_verbose)
	    msg_info("test case=%s want_client_addr=%s want_server_addr=%s "
		     "want_client_port=%s want_server_port=%s",
		     test_label, STR_OR_NULL(test_case->want_client_addr),
		     STR_OR_NULL(test_case->want_server_addr),
		     STR_OR_NULL(test_case->want_client_port),
		     STR_OR_NULL(test_case->want_server_port));

	/*
	 * Start the test.
	 */
	got_req_len = test_case->haproxy_req_len;
	got_return =
	    haproxy_srvr_parse(test_case->haproxy_request, &got_req_len,
			       &got_non_proxy,
			       &got_smtp_client_addr, &got_smtp_client_port,
			       &got_smtp_server_addr, &got_smtp_server_port);
	if (strcmp(STR_OR_NULL(got_return), STR_OR_NULL(test_case->want_return))) {
	    ptest_error(t, "haproxy_srvr_parse return got=%s want=%s",
		       STR_OR_NULL(got_return),
		       STR_OR_NULL(test_case->want_return));
	    ptest_return(t);
	}
	if (got_req_len != test_case->want_req_len) {
	    ptest_error(t, "haproxy_srvr_parse str_len got=%ld want=%ld",
		       (long) got_req_len,
		       (long) test_case->want_req_len);
	    ptest_return(t);
	}
	if (got_non_proxy != test_case->want_non_proxy) {
	    ptest_error(t, "haproxy_srvr_parse non_proxy got=%d want=%d",
		        got_non_proxy,
		       test_case->want_non_proxy);
	    ptest_return(t);
	}
	if (test_case->want_non_proxy || test_case->want_return != 0)
	    /* No expected address/port results. */
	    ptest_return(t);

	/*
	 * Compare address/port results against expected results.
	 */
	if (strcmp(test_case->want_client_addr, got_smtp_client_addr.buf)) {
	    ptest_error(t, "haproxy_srvr_parse client_addr got=%s want=%s",
		       got_smtp_client_addr.buf,
		       test_case->want_client_addr);
	}
	if (strcmp(test_case->want_server_addr, got_smtp_server_addr.buf)) {
	    ptest_error(t, "haproxy_srvr_parse server_addr got=%s want=%s",
		       got_smtp_server_addr.buf,
		       test_case->want_server_addr);
	}
	if (strcmp(test_case->want_client_port, got_smtp_client_port.buf)) {
	    ptest_error(t, "haproxy_srvr_parse client_port got=%s want=%s",
		       got_smtp_client_port.buf,
		       test_case->want_client_port);
	}
	if (strcmp(test_case->want_server_port, got_smtp_server_port.buf)) {
	    ptest_error(t, "haproxy_srvr_parse server_port got=%s want=%s",
		       got_smtp_server_port.buf,
		       test_case->want_server_port);
	}
    });
}

/* convert_v1_proxy_req_to_v2 - convert well-formed v1 proxy request to v2 */

static void convert_v1_proxy_req_to_v2(PTEST_CTX * t, VSTRING *buf,
				           const char *req, ssize_t req_len)
{
    const char myname[] = "convert_v1_proxy_req_to_v2";
    const char *err;
    int     non_proxy;
    MAI_HOSTADDR_STR smtp_client_addr;
    MAI_SERVPORT_STR smtp_client_port;
    MAI_HOSTADDR_STR smtp_server_addr;
    MAI_SERVPORT_STR smtp_server_port;
    struct proxy_hdr_v2 *hdr_v2;
    struct addrinfo *src_res;
    struct addrinfo *dst_res;

    /*
     * Allocate buffer space for the largest possible protocol header, so we
     * don't have to worry about hidden realloc() calls.
     */
    VSTRING_RESET(buf);
    VSTRING_SPACE(buf, sizeof(struct proxy_hdr_v2));
    hdr_v2 = (struct proxy_hdr_v2 *) STR(buf);

    /*
     * Fill in the header,
     */
    memcpy(hdr_v2->sig, PP2_SIGNATURE, PP2_SIGNATURE_LEN);
    hdr_v2->ver_cmd = PP2_VERSION | PP2_CMD_PROXY;
    if ((err = haproxy_srvr_parse(req, &req_len, &non_proxy, &smtp_client_addr,
				  &smtp_client_port, &smtp_server_addr,
				  &smtp_server_port)) != 0 || non_proxy)
	ptest_fatal(t, "%s: malformed or non-proxy request: %s",
		    myname, req);

    if (hostaddr_to_sockaddr(smtp_client_addr.buf, smtp_client_port.buf, 0,
			     &src_res) != 0)
	ptest_fatal(t, "%s: unable to convert source address %s port %s",
		    myname, smtp_client_addr.buf, smtp_client_port.buf);
    if (hostaddr_to_sockaddr(smtp_server_addr.buf, smtp_server_port.buf, 0,
			     &dst_res) != 0)
	ptest_fatal(t, "%s: unable to convert destination address %s port %s",
		    myname, smtp_server_addr.buf, smtp_server_port.buf);
    if (src_res->ai_family != dst_res->ai_family)
	ptest_fatal(t, "%s: mixed source/destination address families", myname);
#ifdef AF_INET6
    if (src_res->ai_family == PF_INET6) {
	hdr_v2->fam = PP2_FAM_INET6 | PP2_TRANS_STREAM;
	hdr_v2->len = htons(PP2_ADDR_LEN_INET6);
	memcpy(hdr_v2->addr.ip6.src_addr,
	       &SOCK_ADDR_IN6_ADDR(src_res->ai_addr),
	       sizeof(hdr_v2->addr.ip6.src_addr));
	hdr_v2->addr.ip6.src_port = SOCK_ADDR_IN6_PORT(src_res->ai_addr);
	memcpy(hdr_v2->addr.ip6.dst_addr,
	       &SOCK_ADDR_IN6_ADDR(dst_res->ai_addr),
	       sizeof(hdr_v2->addr.ip6.dst_addr));
	hdr_v2->addr.ip6.dst_port = SOCK_ADDR_IN6_PORT(dst_res->ai_addr);
    } else
#endif
    if (src_res->ai_family == PF_INET) {
	hdr_v2->fam = PP2_FAM_INET | PP2_TRANS_STREAM;
	hdr_v2->len = htons(PP2_ADDR_LEN_INET);
	hdr_v2->addr.ip4.src_addr = SOCK_ADDR_IN_ADDR(src_res->ai_addr).s_addr;
	hdr_v2->addr.ip4.src_port = SOCK_ADDR_IN_PORT(src_res->ai_addr);
	hdr_v2->addr.ip4.dst_addr = SOCK_ADDR_IN_ADDR(dst_res->ai_addr).s_addr;
	hdr_v2->addr.ip4.dst_port = SOCK_ADDR_IN_PORT(dst_res->ai_addr);
    } else {
	ptest_fatal(t, "unknown address family 0x%x", src_res->ai_family);
    }
    vstring_set_payload_size(buf, PP2_SIGNATURE_LEN + ntohs(hdr_v2->len));
    freeaddrinfo(src_res);
    freeaddrinfo(dst_res);
}

static void run_test_variants(PTEST_CTX *t, BASE_TEST_CASE *v1_test_case)
{
    VSTRING *test_label;
    BASE_TEST_CASE v2_test_case;
    BASE_TEST_CASE mutated_test_case;
    VSTRING *v2_request_buf;
    VSTRING *mutated_request_buf;

    test_label = vstring_alloc(100);
    v2_request_buf = vstring_alloc(100);
    mutated_request_buf = vstring_alloc(100);

    /*
     * Evaluate each v1 test case.
     */
    vstring_sprintf(test_label, "v1 baseline (%sformed)",
		    v1_test_case->want_return ? "mal" : "well");
    evaluate_test_case(t, STR(test_label), v1_test_case);

    /*
     * If the v1 test input is malformed, skip the mutation tests.
     */
    if (v1_test_case->want_return != 0)
	ptest_return(t);

    /*
     * Mutation test: a well-formed v1 test case should still pass after
     * appending a byte, and should return the actual parsed header length.
     * The test uses the implicit VSTRING null safety byte.
     */
    vstring_sprintf(test_label, "v1 mutated (one byte appended)");
	mutated_test_case = *v1_test_case;
    mutated_test_case.haproxy_req_len += 1;
    /* reuse v1_test_case->want_req_len */
    evaluate_test_case(t, STR(test_label), &mutated_test_case);

    /*
     * Mutation test: a well-formed v1 test case should fail after stripping
     * the terminator.
     */
    vstring_sprintf(test_label, "v1 mutated (last byte stripped)");
    mutated_test_case = *v1_test_case;
    mutated_test_case.want_return = "missing protocol header terminator";
    mutated_test_case.haproxy_req_len -= 1;
    mutated_test_case.want_req_len = mutated_test_case.haproxy_req_len;
    evaluate_test_case(t, STR(test_label), &mutated_test_case);

    /*
     * A 'well-formed' v1 test case should pass after conversion to v2.
     */
    vstring_sprintf(test_label, "v2 baseline (converted from v1)");
    v2_test_case = *v1_test_case;
    convert_v1_proxy_req_to_v2(t, v2_request_buf,
			       v1_test_case->haproxy_request,
			       v1_test_case->haproxy_req_len);
    v2_test_case.haproxy_request = STR(v2_request_buf);
    v2_test_case.haproxy_req_len = PP2_HEADER_LEN
	+ ntohs(((struct proxy_hdr_v2 *) STR(v2_request_buf))->len);
    v2_test_case.want_req_len = v2_test_case.haproxy_req_len;
    evaluate_test_case(t, STR(test_label), &v2_test_case);

    /*
     * Mutation test: a well-formed v2 test case should still pass after
     * appending a byte, and should return the actual parsed header length.
     * The test uses the implicit VSTRING null safety byte.
     */
    vstring_sprintf(test_label, "v2 mutated (one byte appended)");
    mutated_test_case = v2_test_case;
    mutated_test_case.haproxy_req_len += 1;
    /* reuse v2_test_case->want_req_len */
    evaluate_test_case(t, STR(test_label), &mutated_test_case);

    /*
     * Mutation test: a well-formed v2 test case should fail after stripping
     * one byte
     */
    vstring_sprintf(test_label, "v2 mutated (last byte stripped)");
    mutated_test_case = v2_test_case;
    mutated_test_case.haproxy_req_len -= 1;
    mutated_test_case.want_req_len = mutated_test_case.haproxy_req_len;
    mutated_test_case.want_return = "short version 2 protocol header";
    evaluate_test_case(t, STR(test_label), &mutated_test_case);

    /*
     * Clean up.
     */
    vstring_free(v2_request_buf);
    vstring_free(mutated_request_buf);
    vstring_free(test_label);
}

 /*
  * PTEST adapter.
  */
typedef struct PTEST_CASE {
    const char *testname;
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
} PTEST_CASE;

static void run_proxy_tests(PTEST_CTX *t, const PTEST_CASE *unused)
{
    BASE_TEST_CASE *v1_test_case;
    VSTRING *test_label;

    /*
     * Run all variants of one base case test together in a subtest.
     */
    test_label = vstring_alloc(100);
    for (v1_test_case = v1_test_cases;
	 v1_test_case->haproxy_request != 0; v1_test_case++) {
	vstring_sprintf(test_label, "%d",
			1 + (int) (v1_test_case - v1_test_cases));
	PTEST_RUN(t, STR(test_label), {
	    run_test_variants(t, v1_test_case);
	});
    }

    /*
     * Additional V2-only test.
     */
    vstring_sprintf(test_label, "%d",
		    1 + (int) (v1_test_case - v1_test_cases));
    PTEST_RUN(t, STR(test_label), {
	evaluate_test_case(t, "v2 non-proxy request", &v2_non_proxy_test);
    });
    vstring_free(test_label);
}

 /*
  * PTEST adapter.
  */
static const PTEST_CASE ptestcases[] = {
    "haproxy_srvr_test", run_proxy_tests,
};

#include <ptest_main.h>
