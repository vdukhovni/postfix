 /*
  * Test program for the myaddrinfo module. The purpose is to verify that the
  * myaddrinfo functions make the expected calls, and that they forward the
  * expected results. See comments in ptest_main.h and pmock_expect_test.c
  * for a documented example.
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
#include <inet_proto.h>

 /*
  * Test library.
  */
#include <ptest.h>
#include <mock_getaddrinfo.h>

#define STR     vstring_str

typedef struct PTEST_CASE {
    const char *testname;		/* Human-readable description */
    void    (*action) (PTEST_CTX *t, const struct PTEST_CASE *);
} PTEST_CASE;

static void test_hostname_to_sockaddr_host(PTEST_CTX *t, const PTEST_CASE *tp)
{
    int     got_st, want_st = 0;
    struct addrinfo *got_info = 0, *want_info;
    struct addrinfo req_hints;
    const char *hostname = "belly.porcupine.org";

    inet_proto_init(tp->testname, "all");

    /*
     * Set up expectations.
     */
    memset(&req_hints, 0, sizeof(req_hints));
    req_hints.ai_family = PF_INET;
    want_info = make_addrinfo(&req_hints, (char *) 0, "168.100.3.6", 0);
    req_hints.ai_family = PF_INET6;
    want_info->ai_next = make_addrinfo(&req_hints, (char *) 0,
				       "2604:8d00:189::6", 0);
    req_hints.ai_family = inet_proto_info()->ai_family;
    req_hints.ai_socktype = SOCK_STREAM;	/* XXX */
    expect_getaddrinfo(1, want_st, hostname, (char *) 0,
		       &req_hints, want_info);

    /*
     * Call the mock and verify the results.
     */
    got_st = hostname_to_sockaddr(hostname, (char *) 0, 0, &got_info);
    if (got_st != want_st) {
	ptest_error(t, "hostname_to_sockaddr status: got %d, want %d",
		    got_st, want_st);
    } else if (!eq_addrinfo(t, "hostname_to_sockaddr addrinfo",
			    got_info, want_info)) {
	/* already logged by eq_addrinfo() */
    }

    /*
     * Clean up.
     */
    freeaddrinfo(want_info);
    if (got_info)
	freeaddrinfo(got_info);
}

static void test_hostname_to_sockaddr_v4host(PTEST_CTX *t, const PTEST_CASE *tp)
{
    int     got_st, want_st = 0;
    struct addrinfo *got_info = 0, *want_info;
    struct addrinfo req_hints;
    const char *hostname = "belly.porcupine.org";

    inet_proto_init(tp->testname, "ipv4");

    /*
     * Set up expectations.
     */
    memset(&req_hints, 0, sizeof(req_hints));
    req_hints.ai_family = PF_INET;
    want_info = make_addrinfo(&req_hints, (char *) 0, "168.100.3.6", 0);
    req_hints.ai_family = inet_proto_info()->ai_family;
    req_hints.ai_socktype = SOCK_STREAM;	/* XXX */
    expect_getaddrinfo(1, want_st, hostname, (char *) 0,
		       &req_hints, want_info);

    /*
     * Call the mock and verify the results.
     */
    got_st = hostname_to_sockaddr(hostname, (char *) 0, 0, &got_info);
    if (got_st != want_st) {
	ptest_error(t, "hostname_to_sockaddr status: got %d, want %d",
		    got_st, want_st);
    } else if (!eq_addrinfo(t, "hostname_to_sockaddr addrinfo",
			    got_info, want_info)) {
	/* already logged by eq_addrinfo() */
    }

    /*
     * Clean up.
     */
    freeaddrinfo(want_info);
    if (got_info)
	freeaddrinfo(got_info);
}


static void test_hostaddr_to_sockaddr_host(PTEST_CTX *t, const PTEST_CASE *tp)
{
    int     got_st, want_st = 0;
    struct addrinfo *got_info, *want_info;
    struct addrinfo req_hints;
    const char *req_hostaddr = "168.100.3.2";

    /*
     * Set up expectations.
     */
    memset(&req_hints, 0, sizeof(req_hints));
    req_hints.ai_family = PF_INET;
    want_info = make_addrinfo(&req_hints, (char *) 0, req_hostaddr, 0);
    req_hints.ai_family = inet_proto_info()->ai_family;
    req_hints.ai_socktype = SOCK_STREAM;	/* XXX */
    req_hints.ai_flags = AI_NUMERICHOST;
    expect_getaddrinfo(1, want_st, req_hostaddr, (char *) 0,
		       &req_hints, want_info);

    /*
     * Call the mock indirectly, and verify the results.
     */
    got_st = hostaddr_to_sockaddr(req_hostaddr, (char *) 0, 0, &got_info);
    if (got_st != want_st) {
	ptest_error(t, "hostaddr_to_sockaddr status: got %d, want %d",
		    got_st, want_st);
    } else if (!eq_addrinfo(t, "hostname_to_sockaddr addrinfo",
			    got_info, want_info)) {
	/* already logged by eq_addrinfo() */
    }

    /*
     * Clean up.
     */
    freeaddrinfo(want_info);
    if (got_info)
	freeaddrinfo(got_info);
}

static void test_hostname_to_sockaddr_nxhost(PTEST_CTX *t, const PTEST_CASE *tp)
{
    int     got_st, want_st = EAI_NONAME;
    struct addrinfo *got_info = 0, *want_info = 0;
    struct addrinfo req_hints;
    const char *hostname = "null.porcupine.org";

    inet_proto_init(tp->testname, "all");

    /*
     * Set up expectations.
     */
    memset(&req_hints, 0, sizeof(req_hints));
    req_hints.ai_family = inet_proto_info()->ai_family;
    req_hints.ai_socktype = SOCK_STREAM;	/* XXX */
    expect_getaddrinfo(1, want_st, hostname, (char *) 0, &req_hints, want_info);

    /*
     * Call the mock and verify the results.
     */
    got_st = hostname_to_sockaddr(hostname, (char *) 0, 0, &got_info);
    if (got_st != want_st) {
	ptest_error(t, "hostname_to_sockaddr status: got %d, want %d",
		    got_st, want_st);
    } else if (!eq_addrinfo(t, "hostname_to_sockaddr addrinfo",
			    got_info, want_info)) {
	/* already logged by eq_addrinfo() */
    }

    /*
     * Clean up.
     */
    if (got_info)
	freeaddrinfo(got_info);
}

 /*
  * Test cases.
  */
const PTEST_CASE ptestcases[] = {
    {
	 /* name */ "test hostname_to_sockaddr host only",
	 /* action */ test_hostname_to_sockaddr_host,
    },
    {
	 /* name */ "test hostname_to_sockaddr v4host only",
	 /* action */ test_hostname_to_sockaddr_v4host,
    },
    {
	 /* name */ "test hostaddr_to_sockaddr host only",
	 /* action */ test_hostaddr_to_sockaddr_host,
    },
    {
	 /* name */ "test hostname_to_sockaddr non-existent host only",
	 /* action */ test_hostname_to_sockaddr_nxhost,
    },
    /* TODO: sockadddr_to_hostaddr, sockaddr_to_hostname. */
};

#include <ptest_main.h>
