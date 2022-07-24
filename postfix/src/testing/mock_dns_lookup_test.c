 /*
  * Test program to exercise mocks including logging. See comments in
  * ptest_main.h and pmock_expect_test.c for a documented example.
  */

 /*
  * System library.
  */
#include <sys_defs.h>
#include <arpa/inet.h>

 /*
  * Utility library.
  */
#include <msg.h>
#include <vstring.h>

 /*
  * Test library.
  */
#include <mock_dns.h>
#include <pmock_expect.h>
#include <ptest.h>

typedef struct PTEST_CASE {
    const char *testname;		/* Human-readable description */
    void    (*action) (PTEST_CTX *t, const struct PTEST_CASE *);
} PTEST_CASE;

#define NO_RES_FLAGS	0

static void test_dns_lookup_x_success(PTEST_CTX *t, const PTEST_CASE *unused)
{
    VSTRING *got_fqdn = vstring_alloc(100), *want_fqdn = vstring_alloc(100);
    int     got_st, want_st = DNS_OK;
    int     got_herrval, want_herrval = 0;
    DNS_RR *got_rr = 0, *want_rr;
    int     got_rcode, want_rcode = NOERROR;
    struct in_addr sin_addr;
    const char *localhost = "localhost";

    /*
     * Set up expectations.
     */
    vstring_strcpy(want_fqdn, localhost);
    if (inet_pton(AF_INET, "127.0.0.1", &sin_addr) != 1)
	ptest_fatal(t, "inet_pton(AF_INET, \"127.0.0.1\", (ptr)): bad address");
    want_rr = make_dns_rr(localhost, localhost, T_A, C_IN,
			  10, 0, 0, &sin_addr, sizeof(sin_addr));
    expect_dns_lookup_x(1, want_herrval, want_st, localhost, T_A, NO_RES_FLAGS,
			want_rr, want_fqdn, (VSTRING *) 0,
			want_rcode, DNS_REQ_FLAG_NONE);

    /*
     * Invoke the mock and verify results.
     */
    got_st = dns_lookup_x("localhost", T_A, NO_RES_FLAGS, &got_rr,
			  got_fqdn, (VSTRING *) 0, &got_rcode,
			  DNS_REQ_FLAG_NONE);
    if (got_st != want_st) {
	ptest_error(t, "dns_lookup_x: got result %d, want %d", got_st, want_st);
    } else if (eq_dns_rr(t, "dns_lookup_x", got_rr, want_rr) == 0) {
	 /* warning is already logged */ ;
    } else if (strcmp(vstring_str(got_fqdn), vstring_str(want_fqdn)) != 0) {
	ptest_error(t, "dns_lookup_x: got fqdn '%s', want '%s'",
		    vstring_str(got_fqdn), vstring_str(want_fqdn));
    } else if (got_rcode != want_rcode) {
	ptest_error(t, "dns_lookup_x: got rcode %d, want %d", got_rcode, want_rcode);
    }
    got_herrval = dns_get_h_errno();
    if (got_herrval != want_herrval)
	ptest_error(t, "dns_get_h_errno: got %d, want %d",
		    got_herrval, want_herrval);

    /*
     * Clean up.
     */
    vstring_free(got_fqdn);
    vstring_free(want_fqdn);
    dns_rr_free(want_rr);
    if (got_rr)
	dns_rr_free(got_rr);
}

static void test_dns_lookup_x_notexist(PTEST_CTX *t, const PTEST_CASE *unused)
{
    VSTRING *got_why = vstring_alloc(100), *want_why = vstring_alloc(100);
    int     got_st, want_st = DNS_NOTFOUND;
    int     got_herrval, want_herrval = HOST_NOT_FOUND;
    int     got_rcode, want_rcode = NXDOMAIN;

    /*
     * Set up expectations.
     */
    vstring_strcpy(want_why, "Host or domain name not found."
	    " Name service error for name=notexist type=A: Host not found");
    expect_dns_lookup_x(1, want_herrval, want_st, "notexist", T_A, NO_RES_FLAGS,
			(DNS_RR *) 0, (VSTRING *) 0, want_why, want_rcode,
			DNS_REQ_FLAG_NONE);

    /*
     * Invoke the mock and verify results.
     */
    got_st = dns_lookup_x("notexist", T_A, NO_RES_FLAGS, (DNS_RR **) 0,
			  (VSTRING *) 0, got_why, &got_rcode,
			  DNS_REQ_FLAG_NONE);
    if (got_st != want_st) {
	ptest_error(t, "dns_lookup_x: got result %d, want %d", got_st, want_st);
    } else if (got_rcode != want_rcode) {
	ptest_error(t, "dns_lookup_x: got rcode %d, want %d", got_rcode, want_rcode);
    } else if (strcmp(vstring_str(got_why), vstring_str(want_why)) != 0) {
	ptest_error(t, "dns_lookup_x: got why '%s', want '%s'",
		    vstring_str(got_why), vstring_str(want_why));
    }
    got_herrval = dns_get_h_errno();
    if (got_herrval != want_herrval)
	ptest_error(t, "dns_get_h_errno: got %d, want %d",
		    got_herrval, want_herrval);

    /*
     * Clean up.
     */
    vstring_free(got_why);
    vstring_free(want_why);
}

static void test_dns_lookup_x_unused(PTEST_CTX *t, const PTEST_CASE *unused)
{

    /*
     * Create an expectation, without calling it. I does not matter what the
     * expectation is, so we use the one from test_dns_lookup_x_notexist().
     */
    expect_dns_lookup_x(1, HOST_NOT_FOUND, NXDOMAIN, "notexist", T_A, NO_RES_FLAGS,
			(DNS_RR *) 0, (VSTRING *) 0, (VSTRING *) 0, 0,
			DNS_REQ_FLAG_NONE);

    /*
     * We expect that there will be a 'missing call' error. If the error does
     * not happen then the test fails.
     */
    expect_ptest_error(t, "got 0 calls for dns_lookup_x(\"notexist\", A, "
		       "0, (ptr), (ptr), (ptr), (ptr), 0), want 1");
}

static void test_dns_set_h_errno_success(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static int want_herrval[] = {12345, 54321};
    int     got_herrval;
    int     n;

    for (n = 0; n < 2; n++) {
	dns_set_h_errno(want_herrval[n]);
	got_herrval = dns_get_h_errno();
	if (got_herrval != want_herrval[n])
	    ptest_error(t, "dns_get_h_errno: got %d, want %d",
			got_herrval, want_herrval[n]);
    }
}

static void test_eq_dns_rr_differ(PTEST_CTX *t, const PTEST_CASE *unused)
{
    DNS_RR *got_rr, *want_rr;
    struct in_addr sin_addr;
    const char *localhost = "localhost";

    if (inet_pton(AF_INET, "127.0.0.1", &sin_addr) != 1)
	ptest_fatal(t, "inet_pton(AF_INET, \"127.0.0.1\", (ptr)): bad address");
    want_rr = make_dns_rr(localhost, localhost, T_A, C_IN,
			  10, 0, 0, &sin_addr, sizeof(sin_addr));

    if (inet_pton(AF_INET, "127.0.0.2", &sin_addr) != 1)
	ptest_fatal(t, "inet_pton(AF_INET, \"127.0.0.2\", (ptr)): bad address");
    got_rr = make_dns_rr(localhost, localhost, T_A, C_IN,
			 10, 0, 0, &sin_addr, sizeof(sin_addr));

    expect_ptest_error(t, "eq_dns_rr: got data 7F:00:00:02, want 7F:00:00:01");
    if (eq_dns_rr(t, "eq_dns_rr", got_rr, want_rr))
	ptest_error(t, "eq_dns_rr: Unexpected match");
    dns_rr_free(got_rr);
    dns_rr_free(want_rr);
}

 /*
  * Test cases. The "success" tests exercise the expectation match and apply
  * helpers, and "unused" tests exercise the print helpers.
  */
const PTEST_CASE ptestcases[] = {
    {
	"test_dns_lookup_x success", test_dns_lookup_x_success,
    },
    {
	"test_dns_lookup_x notexist", test_dns_lookup_x_notexist,
    },
    {
	"test_dns_lookup_x unused", test_dns_lookup_x_unused,
    },
    {
	"dns_set_h_errno success", test_dns_set_h_errno_success,
    },
    {
	"test_eq_dns_rr differ", test_eq_dns_rr_differ,
    },
};

#include <ptest_main.h>
