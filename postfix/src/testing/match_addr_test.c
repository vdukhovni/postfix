 /*
  * Test program to exercise make_addr functions including logging. See
  * comments in ptest_main.h and pmock_expect_test.c for a documented
  * example.
  */

 /*
  * System library.
  */
#include <sys_defs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <wrap_netdb.h>
#include <string.h>

 /*
  * Test library.
  */
#include <make_addr.h>
#include <match_addr.h>
#include <ptest.h>

typedef struct PTEST_CASE {
    const char *testname;		/* Human-readable description */
    void    (*action) (PTEST_CTX *t, const struct PTEST_CASE *);
} PTEST_CASE;

static void test_eq_addrinfo_diff(PTEST_CTX *t, const PTEST_CASE *unused)
{
    struct addrinfo hints;
    struct addrinfo *want_addrinfo;
    struct addrinfo *other_addrinfo;

    /*
     * Set up expectations.
     */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    want_addrinfo = make_addrinfo(&hints, "localhost", "127.0.0.1", 25);
    other_addrinfo = make_addrinfo(&hints, "localhost", "127.0.0.2", 25);

    /*
     * Verify that the two addrinfos don't match. Do not count this mismatch
     * as an error. Instead, count the absence of the mismatch as an error.
     */
    expect_ptest_error(t, " ai_addr: "
		       "got {AF_INET, 127.0.0.2, 25}, "
		       "want {AF_INET, 127.0.0.1, 25}");
    if (eq_addrinfo(t, "test_eq_addrinfo", other_addrinfo, want_addrinfo))
	ptest_error(t, "eq_addrinfo() returned true for different objects");

    /*
     * Clean up.
     */
    freeaddrinfo(want_addrinfo);
    freeaddrinfo(other_addrinfo);
}

static void test_eq_addrinfo_null(PTEST_CTX *t, const PTEST_CASE *unused)
{
    struct addrinfo hints;
    struct addrinfo *want_addrinfo;
    struct addrinfo *other_addrinfo = 0;

    /*
     * Set up expectations.
     */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    want_addrinfo = make_addrinfo(&hints, "localhost", "127.0.0.1", 25);

    /*
     * Verify that the two addrinfos don't match. Do not count this mismatch
     * as an error. Instead, count the absence of the mismatch as an error.
     */
    expect_ptest_error(t, "test_eq_addrinfo_null: got (null), "
		       "want {0, PF_INET, SOCK_STREAM, 0, 16, "
		       "{AF_INET, 127.0.0.1, 25}, localhost}");
    if (eq_addrinfo(t, "test_eq_addrinfo_null", other_addrinfo, want_addrinfo))
	ptest_error(t, "eq_addrinfo() returned true for different objects");

    /*
     * Clean up.
     */
    freeaddrinfo(want_addrinfo);
}

static void test_eq_sockaddr_diff(PTEST_CTX *t, const PTEST_CASE *unused)
{
    struct sockaddr *want_sockaddr;
    struct sockaddr *other_sockaddr;

    /*
     * Set up expectations.
     */
    want_sockaddr = make_sockaddr(AF_INET, "127.0.0.1", 25);
    other_sockaddr = make_sockaddr(AF_INET, "127.0.0.2", 25);

    /*
     * Verify that the two sockaddrs don't match. Do not count this mismatch
     * as an error. Instead, count the absence of the mismatch as an error.
     */
    expect_ptest_error(t, "test_eq_sockaddr_diff: "
		       "got {AF_INET, 127.0.0.2, 25}, "
		       "want {AF_INET, 127.0.0.1, 25}");
    if (eq_sockaddr(t, "test_eq_sockaddr_diff",
		    other_sockaddr, sizeof(struct sockaddr_in),
		    want_sockaddr, sizeof(struct sockaddr_in)))
	ptest_error(t, "eq_sockaddr() returned true for different objects");

    /*
     * Clean up.
     */
    free_sockaddr(want_sockaddr);
    free_sockaddr(other_sockaddr);
}

 /*
  * Test cases.
  */
const PTEST_CASE ptestcases[] = {
    {
	"Compare different IPv4 addrinfos", test_eq_addrinfo_diff,
    },
    {
	"Compare null and non-null IPv4 addrinfos", test_eq_addrinfo_null,
    },
    {
	"Compare different IPv4 sockaddrs", test_eq_sockaddr_diff,
    },
};

#include <ptest_main.h>
