 /*
  * Test program to exercise find_inet_service.c. See pmock_expect_test.c and
  * PTEST_README for documentation.
  */

 /*
  * System library.
  */
#include <sys_defs.h>

 /*
  * Utility library
  */
#include <find_inet_service.h>
#include <known_tcp_ports.h>
#include <msg.h>

 /*
  * Test library.
  */
#include <mock_servent.h>
#include <ptest.h>

struct association {
    const char *lhs;			/* service name */
    const char *rhs;			/* service port */
};

typedef struct PTEST_CASE {
    const char *testname;		/* Human-readable description */
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
    struct association associations[10];
    const char *service;
    const char *proto;
    int     want_port;			/* expected port, host byte order */
    int     needs_mock;
} PTEST_CASE;

static void test_find_inet_service(PTEST_CTX *t, const PTEST_CASE *tp)
{
    struct servent *want_ent = 0;
    const struct association *ap;
    int     got_port;
    const char *err;

    /*
     * Set up expectations. Note that the test infrastructure will catch
     * fatal errors and panics for us.
     */
    clear_known_tcp_ports();
    for (err = 0, ap = tp->associations; err == 0 && ap->lhs != 0; ap++)
	err = add_known_tcp_port(ap->lhs, ap->rhs);
    if (err != 0)
	ptest_fatal(t, "add_known_tcp_port: got err '%s'", err);
    if (tp->needs_mock) {
	if (tp->want_port != -1)
	    want_ent = make_servent(tp->service, tp->want_port, tp->proto);
	else
	    want_ent = 0;
	expect_getservbyname(1, want_ent, tp->service, tp->proto);
    }

    /*
     * Make the call and verify the result. If the call fails with a fatal
     * error or panic, the test infrastructure will verify that the logging
     * is as expected.
     */
    got_port = find_inet_service(tp->service, tp->proto);
    if (got_port != tp->want_port) {
	ptest_error(t, "find_inet_service: got port %d, want %d",
		    got_port, tp->want_port);
    }
    if (want_ent)
	free_servent(want_ent);
}

const PTEST_CASE ptestcases[] = {
    {
	"good-symbolic",
	test_find_inet_service,
	 /* association */ {{"foobar", "25252"}, 0},
	 /* service */ "foobar",
	 /* proto */ "tcp",
	 /* want_port */ 25252,
	 /* needs mock */ 0,
    },
    {
	"good-numeric",
	test_find_inet_service,
	 /* association */ {{"foobar", "25252"}, 0},
	 /* service */ "25252",
	 /* proto */ "tcp",
	 /* want_port */ 25252,
	 /* needs mock */ 0,
    },
    {
	"bad-symbolic",
	test_find_inet_service,
	 /* association */ {{"foobar", "25252"}, 0},
	 /* service */ "an-impossible-name",
	 /* proto */ "tcp",
	 /* want_port */ -1,
	 /* needs mock */ 1,
    },
    {
	"bad-numeric",
	test_find_inet_service,
	 /* association */ {{"foobar", "25252"}, 0},
	 /* service */ "123456",
	 /* proto */ "tcp",
	 /* want_port */ -1,
	 /* needs mock */ 0,
    },
};

 /*
  * Test library.
  */
#include <ptest_main.h>
