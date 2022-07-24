 /*
  * Test program to exercise mocks including logging. See pmock_expect_test.c
  * and ptest_main.h for a documented example.
  */

 /*
  * System library.
  */
#include <sys_defs.h>
#include <wrap_netdb.h>

 /*
  * Utility library.
  */
#include <msg.h>

 /*
  * Test library.
  */
#include <mock_servent.h>
#include <pmock_expect.h>
#include <ptest.h>

typedef struct PTEST_CASE {
    const char *testname;		/* Human-readable description */
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
} PTEST_CASE;

static void test_getservbyname_success(PTEST_CTX *t, const PTEST_CASE *unused)
{
    struct servent *got_ent = 0, *want_ent;

    /*
     * Set up expectations.
     */
    want_ent = make_servent("smtp", 25, "tcp");
    expect_getservbyname(1, want_ent, "smtp", "tcp");

    /*
     * Invoke the mock and verify results.
     */
    got_ent = getservbyname("smtp", "tcp");
    if (eq_servent(t, "getservbyname", got_ent, want_ent) == 0)
	ptest_error(t, "getservbyname: unexpected result mismatch");

    /*
     * Clean up.
     */
    free_servent(want_ent);
}

static void test_getservbyname_notexist(PTEST_CTX *t, const PTEST_CASE *unused)
{
    struct servent *got_ent, *want_ent = 0;

    /*
     * Set up expectations.
     */
    expect_getservbyname(1, want_ent, "noservice", "noproto");

    /*
     * Invoke the mock and verify results.
     */
    got_ent = getservbyname("noservice", "noproto");
    if (eq_servent(t, "getservbyname", got_ent, want_ent) == 0)
	ptest_error(t, "getservbyname: unexpected result mismatch");

    /*
     * Clean up.
     */
    if (got_ent)
	free_servent(got_ent);
}

static void test_getservbyname_unused(PTEST_CTX *t, const PTEST_CASE *unused)
{
    struct servent *want_ent = 0;

    /*
     * Create an expectation, without calling it. I does not matter what the
     * expectation is, so we use the one from test_getservbyname_notexist().
     */
    expect_getservbyname(1, want_ent, "noservice", "noproto");

    /*
     * We expect that there will be a 'missing call' error. If there is none
     * then the test will fail.
     */
    expect_ptest_error(t, "got 0 calls for getservbyname(\"noservice\", "
		       "\"noproto\"), want 1");
}

static void test_eq_servent_differ(PTEST_CTX *t, const PTEST_CASE *unused)
{

    struct servent *got_ent, *want_ent = 0;
    struct probes {
	const char *name;
	int     port;
	const char *proto;
	const char *want_error;
    };
    static struct probes probes[4] = {
	{"abc", 42, "def"},
	{"cba", 42, "def", "eq_servent: got name 'cba', want 'abc'"},
	{"abc", 24, "def", "eq_servent: got port 24, want 42"},
	{"abc", 42, "fed", "eq_servent: got proto 'fed', want 'def'"},
    };
    struct probes *pp;
    int     want_eq;

    pp = probes;
    want_ent = make_servent(pp->name, pp->port, pp->proto);
    for (pp = probes; pp < probes + sizeof(probes) / sizeof(probes[0]); pp++) {
	got_ent = make_servent(pp->name, pp->port, pp->proto);
	want_eq = !pp->want_error;
	if (pp->want_error)
	    expect_ptest_error(t, pp->want_error);
	if (eq_servent(t, "eq_servent", got_ent, want_ent) != want_eq)
	    ptest_error(t, "unexpected eq_servent result mismatch");
	free_servent(got_ent);
    }
    free_servent(want_ent);
}

static void test_setservent_match(PTEST_CTX *t, const PTEST_CASE *unused)
{

    /*
     * Set up expectations.
     */
    expect_setservent(1, 1);

    /*
     * Invoke the mock and verify results.
     */
    setservent(1);
}

static void test_setservent_nomatch(PTEST_CTX *t, const PTEST_CASE *unused)
{

    /*
     * Set up expectations.
     */
    expect_setservent(1, 1);

    /*
     * These errors are intentional. If they don't happen then the test
     * fails.
     */
    expect_ptest_error(t, "unexpected call: setservent(2)");
    expect_ptest_error(t, "got 0 calls for setservent(1), want 1");

    /*
     * Invoke the mock and verify results.
     */
    setservent(2);
}

static void test_endservent_match(PTEST_CTX *t, const PTEST_CASE *unused)
{

    /*
     * Set up expectations.
     */
    expect_endservent(1);

    /*
     * Invoke the mock and verify results.
     */
    endservent();
}

static void test_endservent_unused(PTEST_CTX *t, const PTEST_CASE *unused)
{

    /*
     * Set up expectations without making a call.
     */
    expect_endservent(1);

    /*
     * This error is intentional. If it does not happen the test fails.
     */
    expect_ptest_error(t, "got 0 calls for endservent(), want 1");

    /*
     * Verify results (this happens in the test infrastructure).
     */
}

 /*
  * Test cases. The "success" tests exercise the expectation match and apply
  * helpers, and "unused" tests exercise the print helpers.
  */
const PTEST_CASE ptestcases[] = {

    /*
     * getservbyname()
     */
    {
	"test getservbyname success", test_getservbyname_success,
    },
    {
	"test getservbyname notexist", test_getservbyname_notexist,
    },
    {
	"test getservbyname unused", test_getservbyname_unused,
    },

    /*
     * eq_servent()
     */
    {
	"test eq_servent differ", test_eq_servent_differ,
    },

    /*
     * setservent()
     */
    {
	"test setservent match", test_setservent_match,
    },
    {
	"test setservent nomatch", test_setservent_nomatch,
    },

    /*
     * endservent()
     */
    {
	"test endservent match", test_endservent_match,
    },
    {
	"test endservent unused", test_endservent_unused,
    },

};

#include <ptest_main.h>
