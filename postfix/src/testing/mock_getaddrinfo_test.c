 /*
  * Test program for the mock_getaddrinfo module. See comments in
  * ptest_main.h and pmock_expect_test.c for a documented example.
  */

 /*
  * System library.
  */
#include <sys_defs.h>
#include <wrap_netdb.h>

 /*
  * Utility library.
  */
#include <vstring.h>

 /*
  * Test library.
  */
#include <mock_getaddrinfo.h>
#include <pmock_expect.h>
#include <ptest.h>

#define STR	vstring_str

typedef struct PTEST_CASE {
    const char *testname;		/* Human-readable description */
    void    (*action) (PTEST_CTX *t, const struct PTEST_CASE *);
} PTEST_CASE;

static void test_getaddrinfo_success(PTEST_CTX *t, const PTEST_CASE *unused)
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
    expect_getaddrinfo(1, want_st, "localhost", "smtp", &hints, want_addrinfo);

    /*
     * Invoke the mock and verify results.
     */
    got_st = getaddrinfo("localhost", "smtp", &hints, &got_addrinfo);
    if (got_st != want_st) {
	ptest_error(t, "getaddrinfo: got %d, want %d", got_st, want_st);
    } else if (eq_addrinfo(t, "getaddrinfo", got_addrinfo,
			   want_addrinfo) == 0) {
	VSTRING *got_buf = vstring_alloc(100);
	VSTRING *want_buf = vstring_alloc(100);

	ptest_error(t, "getaddrinfo: got %s, want %s",
		    append_addrinfo_to_string(got_buf, got_addrinfo),
		    append_addrinfo_to_string(want_buf, want_addrinfo));
	vstring_free(got_buf);
	vstring_free(want_buf);
    }

    /*
     * Clean up.
     */
    freeaddrinfo(want_addrinfo);
    if (got_addrinfo)
	freeaddrinfo(got_addrinfo);
}

static void test_getaddrinfo_failure(PTEST_CTX *t, const PTEST_CASE *unused)
{
    struct addrinfo hints;
    struct addrinfo *got_addrinfo = 0;
    struct addrinfo *want_addrinfo = 0;
    int     got_st, want_st = EAI_FAIL;
    VSTRING *event_buf = vstring_alloc(100);
    VSTRING *hints_buf = vstring_alloc(100);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;

    /*
     * The missing expectation is intentional. Do not count this as an error.
     */
    vstring_sprintf(event_buf, "unexpected call: "
		    "getaddrinfo(\"notexist\", \"smtp\", %s, (ptr))",
		    addrinfo_hints_to_string(hints_buf, &hints));
    expect_ptest_error(t, STR(event_buf));
    vstring_free(event_buf);
    vstring_free(hints_buf);

    /*
     * Invoke the mock and verify results.
     */
    got_st = getaddrinfo("notexist", "smtp", &hints, &got_addrinfo);
    if (got_st != want_st) {
	ptest_error(t, "getaddrinfo: got %d, want %d", got_st, want_st);
    } else if (eq_addrinfo(t, "getaddrinfo", got_addrinfo,
			   want_addrinfo) == 0) {
	VSTRING *got_buf = vstring_alloc(100);

	ptest_error(t, "getaddrinfo: got %s, want (null)",
		    append_addrinfo_to_string(got_buf, got_addrinfo));
	vstring_free(got_buf);
    }

    /*
     * Clean up.
     */
    if (got_addrinfo)
	freeaddrinfo(got_addrinfo);
}

static void test_getnameinfo_numeric_success(PTEST_CTX *t,
					           const PTEST_CASE *unused)
{
    struct sockaddr *req_sockaddr = make_sockaddr(AF_INET, "127.0.0.1", 25);
    size_t  req_sockaddrlen = sizeof(struct sockaddr_in);
    int     got_st, want_st = 0;
    MAI_HOSTADDR_STR want_hostaddr = {"127.0.0.1"};
    MAI_SERVPORT_STR want_servport = {"25"};
    MAI_HOSTADDR_STR got_hostaddr;
    MAI_SERVPORT_STR got_servport;
    int     req_flags = NI_NUMERICHOST | NI_NUMERICSERV;

    /*
     * Set up expectations.
     */
    expect_getnameinfo(1, want_st, req_sockaddr, req_sockaddrlen,
		       want_hostaddr.buf, sizeof(want_hostaddr),
		       want_servport.buf, sizeof(want_servport),
		       req_flags);

    /*
     * Invoke the mock and verify results.
     */
    got_st = getnameinfo(req_sockaddr, req_sockaddrlen,
			 got_hostaddr.buf, sizeof(got_hostaddr),
			 got_servport.buf, sizeof(got_servport),
			 req_flags);

    if (got_st != want_st) {
	ptest_error(t, "getnameinfo: got %d, want %d", got_st, want_st);
    } else if (strcmp(got_hostaddr.buf, want_hostaddr.buf) != 0) {
	ptest_error(t, "getnameinfo hostaddr: got '%s', want '%s'",
		    got_hostaddr.buf, want_hostaddr.buf);
    } else if (strcmp(got_servport.buf, want_servport.buf) != 0) {
	ptest_error(t, "getnameinfo servport: got '%s', want '%s'",
		    got_servport.buf, want_servport.buf);
    }

    /*
     * Clean up.
     */
    free_sockaddr(req_sockaddr);
}

static void test_getnameinfo_numeric_failure(PTEST_CTX *t,
					           const PTEST_CASE *unused)
{
    struct sockaddr *req_sockaddr = make_sockaddr(AF_INET, "127.0.0.1", 25);
    size_t  req_sockaddrlen = sizeof(struct sockaddr_in);
    int     req_flags = NI_NUMERICHOST | NI_NUMERICSERV;
    int     got_st, want_st = EAI_FAIL;
    VSTRING *event_buf = vstring_alloc(100);
    VSTRING *ni_flags_buf = vstring_alloc(100);

    /*
     * The missing expectation is intentional. Do not count this as an error.
     */
    vstring_sprintf(event_buf, "unexpected call: "
		    "getnameinfo({AF_INET, 127.0.0.1, 25}, %ld, "
		    "(ptr), (len), (ptr), (len), %s",
		    (long) req_sockaddrlen,
		    ni_flags_to_string(ni_flags_buf, req_flags));
    expect_ptest_error(t, STR(event_buf));

    /*
     * Invoke the mock and verify results.
     */
    got_st = getnameinfo(req_sockaddr, req_sockaddrlen,
			 (char *) 0, (size_t) 0,
			 (char *) 0, (size_t) 0,
			 req_flags);
    if (got_st != want_st)
	ptest_error(t, "getnameinfo return: got %d, want %d", got_st, want_st);

    /*
     * Clean up.
     */
    vstring_free(event_buf);
    vstring_free(ni_flags_buf);
    free_sockaddr(req_sockaddr);
}

 /*
  * Test cases. The "success" tests exercise the expectation match and apply
  * helpers, and "failure" tests exercise the print helpers. All tests
  * exercise the expectation free helpers.
  */
const PTEST_CASE ptestcases[] = {
    {
	"getaddrinfo success", test_getaddrinfo_success,
    },
    {
	"getaddrinfo failure", test_getaddrinfo_failure,
    },
    {
	"getnameinfo_numeric success", test_getnameinfo_numeric_success,
    },
    {
	"getnameinfo_numeric failure", test_getnameinfo_numeric_failure,
    },
};

#include <ptest_main.h>
