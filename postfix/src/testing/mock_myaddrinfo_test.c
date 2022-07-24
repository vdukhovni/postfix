 /*
  * Test program to exercise mocks including logging. See comments in
  * ptest_main.h and pmock_expect_test.c for a documented example.
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
  * Test library.
  */
#include <mock_myaddrinfo.h>
#include <pmock_expect.h>
#include <ptest.h>

typedef struct PTEST_CASE {
    const char *testname;		/* Human-readable description */
    void    (*action) (PTEST_CTX *t, const struct PTEST_CASE *);
} PTEST_CASE;

static void test_hostname_to_sockaddr_success(PTEST_CTX *t,
					           const PTEST_CASE *unused)
{
    struct addrinfo hints;
    struct addrinfo *got_addrinfo;
    struct addrinfo *want_addrinfo;
    int     got_st, want_st = 0;

    /*
     * Set up expectations.
     */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    want_addrinfo = make_addrinfo(&hints, "localhost", "127.0.0.1", 25);
    expect_hostname_to_sockaddr_pf(1, want_st, "localhost", PF_UNSPEC, "smtp",
				   SOCK_STREAM, want_addrinfo);

    /*
     * Invoke the mock and verify results.
     */
    got_st = hostname_to_sockaddr_pf("localhost", PF_UNSPEC, "smtp",
				     SOCK_STREAM, &got_addrinfo);
    if (got_st != want_st) {
	ptest_error(t, "hostname_to_sockaddr: got %d, want %d", got_st, want_st);
    } else if (eq_addrinfo(t, "hostname_to_sockaddr", got_addrinfo,
			   want_addrinfo) == 0) {
	ptest_error(t, "hostname_to_sockaddr: unexpected result mismatch");
    }

    /*
     * Clean up.
     */
    freeaddrinfo(want_addrinfo);
    if (got_addrinfo)
	freeaddrinfo(got_addrinfo);
}

static void test_hostname_to_sockaddr_failure(PTEST_CTX *t,
					           const PTEST_CASE *unused)
{
    struct addrinfo *got_addrinfo = 0;
    struct addrinfo *want_addrinfo = 0;
    int     got_st, want_st = EAI_FAIL;

    /*
     * The missing expectation is intentional. Do not count this as an error.
     */
    expect_ptest_error(t, "unexpected call: "
	    "hostname_to_sockaddr_pf(\"notexist\", 0, \"smtp\", 1, (ptr))");

    /*
     * Invoke the mock and verify results.
     */
    got_st = hostname_to_sockaddr_pf("notexist", PF_UNSPEC, "smtp",
				     SOCK_STREAM, &got_addrinfo);
    if (got_st != want_st) {
	ptest_error(t, "hostname_to_sockaddr: got %d, want %d", got_st, want_st);
    } else if (eq_addrinfo(t, "hostname_to_sockaddr", got_addrinfo,
			   want_addrinfo) == 0) {
	ptest_error(t, "hostname_to_sockaddr: unexpected result mismatch");
    }

    /*
     * Clean up.
     */
    if (got_addrinfo)
	freeaddrinfo(got_addrinfo);
}

static void test_hostaddr_to_sockaddr_success(PTEST_CTX *t,
					           const PTEST_CASE *unused)
{
    struct addrinfo hints;
    struct addrinfo *got_addrinfo = 0;
    struct addrinfo *want_addrinfo;
    int     got_st, want_st = 0;

    /*
     * Set up expectations.
     */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    want_addrinfo = make_addrinfo(&hints, (char *) 0, "127.0.0.1", 25);
    expect_hostaddr_to_sockaddr(1, want_st, "127.0.0.1", "25", SOCK_STREAM,
				want_addrinfo);

    /*
     * Invoke the mock and verify results.
     */
    got_st = hostaddr_to_sockaddr("127.0.0.1", "25", SOCK_STREAM,
				  &got_addrinfo);
    if (got_st != want_st) {
	ptest_error(t, "hostaddr_to_sockaddr: got %d, want %d", got_st, want_st);
    } else if (eq_addrinfo(t, "hostaddr_to_sockaddr", got_addrinfo,
			   want_addrinfo) == 0) {
	ptest_error(t, "hostname_to_sockaddr: unexpected result mismatch");
    }

    /*
     * Clean up.
     */
    freeaddrinfo(want_addrinfo);
    if (got_addrinfo)
	freeaddrinfo(got_addrinfo);
}

static void test_hostaddr_to_sockaddr_failure(PTEST_CTX *t,
					           const PTEST_CASE *unused)
{
    struct addrinfo *got_addrinfo = 0;
    struct addrinfo *want_addrinfo = 0;
    int     got_st, want_st = EAI_FAIL;

    /*
     * The missing expectation is intentional. Do not count this as an error.
     */
    expect_ptest_error(t, "unexpected call: "
		       "hostaddr_to_sockaddr(\"127.0.0.1\", \"25\", "
		       "1, (ptr))");

    /*
     * Invoke the mock and verify results.
     */
    got_st = hostaddr_to_sockaddr("127.0.0.1", "25", SOCK_STREAM,
				  &got_addrinfo);
    if (got_st != want_st) {
	ptest_error(t, "hostaddr_to_sockaddr: got %d, want %d", got_st, want_st);
    } else if (eq_addrinfo(t, "hostaddr_to_sockaddr", got_addrinfo,
			   want_addrinfo) == 0) {
	ptest_error(t, "hostname_to_sockaddr: unexpected result mismatch");
    }

    /*
     * Clean up.
     */
    if (got_addrinfo)
	freeaddrinfo(got_addrinfo);
}

static void test_sockaddr_to_hostaddr_success(PTEST_CTX *t,
					           const PTEST_CASE *unused)
{
    struct sockaddr *sa;
    SOCKADDR_SIZE salen;
    int     got_st, want_st = 0;
    MAI_HOSTADDR_STR want_hostaddr;
    MAI_SERVPORT_STR want_portnum;
    MAI_HOSTADDR_STR got_hostaddr;
    MAI_SERVPORT_STR got_portnum;

    /*
     * Set up expectations.
     */
    sa = make_sockaddr(AF_INET, "127.0.0.1", 25);
    salen = sizeof(struct sockaddr_in);
    strncpy(want_hostaddr.buf, "127.0.0.1", sizeof(want_hostaddr.buf));
    strncpy(want_portnum.buf, "25", sizeof(want_portnum.buf));
    expect_sockaddr_to_hostaddr(1, want_st, sa, salen,
				&want_hostaddr, &want_portnum, 0);

    /*
     * Invoke the mock and verify results.
     */
    got_st = sockaddr_to_hostaddr(sa, salen, &got_hostaddr, &got_portnum, 0);
    if (got_st != want_st) {
	ptest_error(t, "sockaddr_to_hostaddr ret: got %d, want %d", got_st, want_st);
    } else if (strcmp(got_hostaddr.buf, want_hostaddr.buf) != 0) {
	ptest_error(t, "sockaddr_to_hostaddr hostaddr.buf: got %s, want %s",
		    got_hostaddr.buf, want_hostaddr.buf);
    } else if (strcmp(got_portnum.buf, want_portnum.buf) != 0) {
	ptest_error(t, "sockaddr_to_hostaddr portnum.buf: got %s, want %s",
		    got_portnum.buf, want_portnum.buf);
    }

    /*
     * Clean up.
     */
    free_sockaddr(sa);
}

static void test_sockaddr_to_hostaddr_failure(PTEST_CTX *t,
					           const PTEST_CASE *unused)
{
    struct sockaddr *sa;
    SOCKADDR_SIZE salen;
    int     got_st, want_st = EAI_FAIL;

    /*
     * The missing expectation is intentional. Do not count this as an error.
     */
    expect_ptest_error(t, "unexpected call: "
		       "sockaddr_to_hostaddr({AF_INET, 127.0.0.1, 25}, 16, "
		       "(ptr), (ptr))");

    /*
     * Invoke the mock and verify results.
     */
    sa = make_sockaddr(AF_INET, "127.0.0.1", 25);
    salen = sizeof(struct sockaddr_in);
    got_st = sockaddr_to_hostaddr(sa, salen, (MAI_HOSTADDR_STR *) 0,
				  (MAI_SERVPORT_STR *) 0, 0);
    if (got_st != want_st)
	ptest_error(t, "sockaddr_to_hostaddr ret: got %d, want %d", got_st, want_st);

    /*
     * Clean up.
     */
    free_sockaddr(sa);
}

static void test_sockaddr_to_hostname_success(PTEST_CTX *t,
					           const PTEST_CASE *unused)
{
    struct sockaddr *sa;
    SOCKADDR_SIZE salen;
    int     got_st, want_st = 0;
    MAI_HOSTNAME_STR want_hostname;
    MAI_SERVNAME_STR want_service;
    MAI_HOSTNAME_STR got_hostname;
    MAI_SERVNAME_STR got_service;

    /*
     * Set up expectations.
     */
    sa = make_sockaddr(AF_INET, "127.0.0.1", 25);
    salen = sizeof(struct sockaddr_in);
    strncpy(want_hostname.buf, "localhost", sizeof(want_hostname.buf));
    strncpy(want_service.buf, "smtp", sizeof(want_service.buf));
    expect_sockaddr_to_hostname(1, want_st, sa, salen, &want_hostname, &want_service, 0);

    /*
     * Invoke the mock and verify results.
     */
    got_st = sockaddr_to_hostname(sa, salen, &got_hostname, &got_service, 0);
    if (got_st != want_st) {
	ptest_error(t, "sockaddr_to_hostname ret: got %d, want %d", got_st, want_st);
    } else if (strcmp(got_hostname.buf, want_hostname.buf) != 0) {
	ptest_error(t, "sockaddr_to_hostname hostname.buf: got %s, want %s",
		    got_hostname.buf, want_hostname.buf);
    } else if (strcmp(got_service.buf, want_service.buf) != 0) {
	ptest_error(t, "sockaddr_to_hostname service.buf: got %s, want %s",
		    got_service.buf, want_service.buf);
    }

    /*
     * Clean up.
     */
    free_sockaddr(sa);
}

static void test_sockaddr_to_hostname_failure(PTEST_CTX *t,
					           const PTEST_CASE *unused)
{
    struct sockaddr *sa;
    SOCKADDR_SIZE salen;
    int     got_st, want_st = EAI_FAIL;

    /*
     * The missing expectation is intentional. Do not count this as an error.
     */
    expect_ptest_error(t, "unexpected call: "
		       "sockaddr_to_hostname({AF_INET, 127.0.0.1, 0}, 16, "
		       "(ptr), (ptr))");

    /*
     * Invoke the mock and verify results.
     */
    sa = make_sockaddr(AF_INET, "127.0.0.1", 65536);
    salen = sizeof(struct sockaddr_in);
    got_st = sockaddr_to_hostname(sa, salen, (MAI_HOSTNAME_STR *) 0,
				  (MAI_SERVNAME_STR *) 0, 0);
    if (got_st != want_st)
	ptest_error(t, "sockaddr_to_hostname ret: got %d, want %d", got_st, want_st);

    /*
     * Clean up.
     */
    free_sockaddr(sa);
}

 /*
  * Test cases. The "success" tests exercise the expectation match and apply
  * helpers, and "failure" tests exercise the print helpers. All tests
  * exercise the expectation free helpers.
  */
const PTEST_CASE ptestcases[] = {
    {
	"hostname_to_sockaddr success", test_hostname_to_sockaddr_success,
    },
    {
	"hostname_to_sockaddr failure", test_hostname_to_sockaddr_failure,
    },
    {
	"hostaddr_to_sockaddr success", test_hostaddr_to_sockaddr_success,
    },
    {
	"hostaddr_to_sockaddr failure", test_hostaddr_to_sockaddr_failure,
    },
    {
	"sockaddr_to_hostaddr success", test_sockaddr_to_hostaddr_success,
    },
    {
	"sockaddr_to_hostaddr failure", test_sockaddr_to_hostaddr_failure,
    },
    {
	"sockaddr_to_hostname success", test_sockaddr_to_hostname_success,
    },
    {
	"sockaddr_to_hostname failure", test_sockaddr_to_hostname_failure,
    },
};

#include <ptest_main.h>
