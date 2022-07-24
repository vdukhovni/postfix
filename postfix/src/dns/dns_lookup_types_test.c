 /*
  * Test program to mocks including logging. See pmock_expect_test.c and
  * ptest_main.h for a documented example.
  */

 /*
  * System library.
  */
#include <sys_defs.h>

 /*
  * Utility library.
  */
#include <msg.h>
#include <vstring.h>

 /*
  * DNS library.
  */
#include <dns.h>

 /*
  * Test library.
  */
#include <mock_dns.h>
#include <ptest.h>

typedef struct PTEST_CASE {
    const char *testname;		/* Human-readable description */
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
} PTEST_CASE;

 /*
  * dns_lookup_rl() forwards all calls to dns_lookup_rv(), therefore most
  * tests will focus on dns_lookup_rl().
  */
#define NO_RFLAGS	0
#define NO_LFLAGS	0

static void test_dns_lookup_rl_success(PTEST_CTX *t, const PTEST_CASE *unused)
{
    int     got_st, want_st = DNS_OK;
    DNS_RR *got_rr = 0, *want_rr;
    int     got_rcode, want_rcode = NOERROR;
    int     got_herrval, want_herrval = 0;

    /*
     * Set up expectations and prepared responses.
     */
    want_rr = make_dns_rr("example.com", "example.com", T_MX, C_IN,
			  5, 0, 10, "m1.example.com", 14);
    expect_dns_lookup_x(1, want_herrval, DNS_OK, "example.com", T_MX,
			NO_RFLAGS, want_rr, (VSTRING *) 0, (VSTRING *) 0,
			NOERROR, NO_LFLAGS);

    got_st = dns_lookup_rl("example.com", NO_RFLAGS, &got_rr, (VSTRING *) 0,
			   (VSTRING *) 0, &got_rcode, NO_LFLAGS, T_MX, 0);
    if (got_st != want_st) {
	ptest_error(t, "dns_lookup_rl: got result %d, want %d",
		    got_st, want_st);
    } else if (got_rcode != want_rcode) {
	ptest_error(t, "dns_lookup_rl: got rcode %d, want %d",
		    got_rcode, want_rcode);
    } else {
	(void) eq_dns_rr(t, "dns_lookup_rl", got_rr, want_rr);
    }

    got_herrval = dns_get_h_errno();
    if (got_herrval != want_herrval)
	ptest_error(t, "dns_get_h_errno: got %d, want %d",
		    got_herrval, want_herrval);

    /*
     * Cleanup.
     */
    dns_rr_free(want_rr);
    if (got_rr)
	dns_rr_free(got_rr);
}

static void test_dns_lookup_rv_success(PTEST_CTX *t, const PTEST_CASE *unused)
{
    int     got_st, want_st = DNS_OK;
    DNS_RR *got_rr = 0, *want_rr;
    int     got_rcode, want_rcode = NOERROR;
    int     got_herrval, want_herrval = 0;
    static const unsigned rr_types[2] = {T_MX, 0};

    /*
     * Set up expectations and prepared responses,
     */
    want_rr = make_dns_rr("example.com", "example.com", T_MX, C_IN,
			  5, 0, 10, "m1.example.com", 14);
    expect_dns_lookup_x(1, want_herrval, DNS_OK, "example.com", T_MX,
			NO_RFLAGS, want_rr, (VSTRING *) 0, (VSTRING *) 0,
			NOERROR, NO_LFLAGS);

    got_st = dns_lookup_rv("example.com", NO_RFLAGS, &got_rr, (VSTRING *) 0,
			   (VSTRING *) 0, &got_rcode, NO_LFLAGS, rr_types);
    if (got_st != want_st) {
	ptest_error(t, "dns_lookup_rv: got result %d, want %d",
		    got_st, want_st);
    } else if (got_rcode != want_rcode) {
	ptest_error(t, "dns_lookup_rv: got rcode %d, want %d",
		    got_rcode, want_rcode);
    } else {
	(void) eq_dns_rr(t, "dns_lookup_rv", got_rr, want_rr);
    }

    got_herrval = dns_get_h_errno();
    if (got_herrval != want_herrval)
	ptest_error(t, "dns_get_h_errno: got %d, want %d",
		    got_herrval, want_herrval);

    /*
     * Cleanup.
     */
    dns_rr_free(want_rr);
    if (got_rr)
	dns_rr_free(got_rr);
}

static void test_dns_lookup_rv_error_ladder(PTEST_CTX *t,
					            const PTEST_CASE *unused)
{
    int     got_st;
    int     got_herrval;
    struct step {
	int     want_st;
	int     want_herrval;
    };
    struct step ladder[] = {
	DNS_OK, 0,
	DNS_POLICY, 0,
	DNS_RETRY, TRY_AGAIN,
	DNS_INVAL, 0,
	DNS_FAIL, NO_RECOVERY,
	DNS_NULLMX, 0,
	DNS_NOTFOUND, NO_DATA,
    };
    struct step *lp;
    VSTRING *label = vstring_alloc(100);

#define LADDER_SIZE	(sizeof(ladder)/sizeof(*ladder))

    for (lp = ladder; lp < ladder + LADDER_SIZE - 1; lp++) {

	vstring_sprintf(label, "%s precedence over %s",
			dns_status_to_string(lp->want_st),
			dns_status_to_string(lp[1].want_st));

	PTEST_RUN(t, vstring_str(label), {

	    /*
	     * Set up expectations and prepared responses.
	     */
	    expect_dns_lookup_x(1, lp->want_herrval, lp->want_st,
				"example.com", T_MX, NO_RFLAGS, (DNS_RR *) 0,
			  (VSTRING *) 0, (VSTRING *) 0, NOERROR, NO_LFLAGS);
	    expect_dns_lookup_x(1, lp[1].want_herrval, lp[1].want_st,
				"example.com", T_A, NO_RFLAGS, (DNS_RR *) 0,
			  (VSTRING *) 0, (VSTRING *) 0, NOERROR, NO_LFLAGS);

	    /*
	     * Call the mock and verify the results.
	     */
	    got_st = dns_lookup_rl("example.com", NO_RFLAGS, (DNS_RR **) 0,
				   (VSTRING *) 0, (VSTRING *) 0, (int *) 0,
				   NO_LFLAGS, T_MX, T_A, 0);
	    if (got_st != lp->want_st) {
		ptest_error(t, "dns_lookup_rv: got result %d, want %d",
			    got_st, lp->want_st);
	    }
	    got_herrval = dns_get_h_errno();
	    if (got_herrval != lp->want_herrval)
		ptest_error(t, "dns_get_h_errno: got %d, want %d",
			    got_herrval, lp->want_herrval);
	});
    }
    vstring_free(label);
}

 /*
  * Test cases. The "success" tests exercise the expectation match and apply
  * helpers, and "failure" tests exercise the print helpers. All tests
  * exercise the expectation free helpers.
  */
const PTEST_CASE ptestcases[] = {
    {
	"test_dns_lookup_rl success", test_dns_lookup_rl_success,
    },
    {
	"test_dns_lookup_rv success", test_dns_lookup_rv_success,
    },
    {
	"test_dns_lookup_rv error ladder", test_dns_lookup_rv_error_ladder,
    },
};

#include <ptest_main.h>
