 /*
  * Test program to exercise normalize_mailhost_addr.c. See ptest_main.h for
  * a documented example.
  */

 /*
  * System library.
  */
#include <sys_defs.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

 /*
  * Utility library.
  */
#include <mymalloc.h>
#include <inet_proto.h>

 /*
  * Global library.
  */
#include <normalize_mailhost_addr.h>

 /*
  * Test library.
  */
#include <ptest.h>

typedef struct PTEST_CASE {
    const char *testname;
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
    const char *inet_protocols;
    const char *mailhost_addr;
    int     want_return;
    const char *want_mailhost_addr;
    char   *want_bare_addr;
    int     want_addr_family;
} PTEST_CASE;

static void test_normalize_mailhost_addr(PTEST_CTX *t, const PTEST_CASE *tp)
{
    /* Actual results. */
    int     got_return;
    char   *got_mailhost_addr = mystrdup("initial_mailhost_addr");
    char   *got_bare_addr = mystrdup("initial_bare_addr");
    int     got_addr_family = 0xdeadbeef;

#define CLEANUP_AND_RETURN() do { \
	if (got_mailhost_addr) \
	    myfree(got_mailhost_addr); \
	got_mailhost_addr = 0; \
	if (got_bare_addr) \
	    myfree(got_bare_addr); \
	got_bare_addr = 0; \
    } while (0)

    inet_proto_init(tp->testname, tp->inet_protocols);
    got_return = normalize_mailhost_addr(tp->mailhost_addr,
					 tp->want_mailhost_addr ?
					 &got_mailhost_addr : (char **) 0,
					 tp->want_bare_addr ?
					 &got_bare_addr : (char **) 0,
					 tp->want_addr_family >= 0 ?
					 &got_addr_family : (int *) 0);
    if (got_return != tp->want_return) {
	ptest_error(t, "return value: got %d, want %d",
		    got_return, tp->want_return);
	CLEANUP_AND_RETURN();
    }
    if (tp->want_return != 0)
	CLEANUP_AND_RETURN();
    if (tp->want_mailhost_addr
	&& strcmp(tp->want_mailhost_addr, got_mailhost_addr)) {
	ptest_error(t, "mailhost_addr value: got '%s', want '%s'",
		    got_mailhost_addr, tp->want_mailhost_addr);
    }
    if (tp->want_bare_addr && strcmp(tp->want_bare_addr, got_bare_addr)) {
	ptest_error(t, "bare_addr value: got '%s', want '%s'",
		    got_bare_addr, tp->want_bare_addr);
    }
    if (tp->want_addr_family > 0 && tp->want_addr_family != got_addr_family) {
	ptest_error(t, "addr_family: got 0x%x, want 0x%x",
		    got_addr_family, tp->want_addr_family);
    }
    CLEANUP_AND_RETURN();
}

static PTEST_CASE ptestcases[] = {
    {
	"IPv4 in IPv6 #1", test_normalize_mailhost_addr,
	 /* inet_protocols */ "ipv4, ipv6",
	 /* mailhost_addr */ "ipv6:::ffff:1.2.3.4",
	 /* want_return */ 0,
	 /* want_mailhost_addr */ "1.2.3.4",
	 /* want_bare_addr */ "1.2.3.4",
	 /* want_addr_family */ AF_INET
    }, {
	"IPv4 in IPv6 #2", test_normalize_mailhost_addr,
	 /* inet_protocols */ "ipv6",
	 /* mailhost_addr */ "ipv6:::ffff:1.2.3.4",
	 /* want_return */ 0,
	 /* want_mailhost_addr */ "IPv6:::ffff:1.2.3.4",
	 /* want_bare_addr */ "::ffff:1.2.3.4",
	 /* want_addr_family */ AF_INET6
    }, {
	"Pass IPv4 or IPV6 #1", test_normalize_mailhost_addr,
	 /* inet_protocols */ "ipv4, ipv6",
	 /* mailhost_addr */ "ipv6:fc00::1",
	 /* want_return */ 0,
	 /* want_mailhost_addr */ "IPv6:fc00::1",
	 /* want_bare_addr */ "fc00::1",
	 /* want_addr_family */ AF_INET6
    }, {
	"Pass IPv4 or IPV6 #2", test_normalize_mailhost_addr,
	 /* inet_protocols */ "ipv4, ipv6",
	 /* mailhost_addr */ "1.2.3.4",
	 /* want_return */ 0,
	 /* want_mailhost_addr */ "1.2.3.4",
	 /* want_bare_addr */ "1.2.3.4",
	 /* want_addr_family */ AF_INET
    }, {
	"Normalize IPv4 or IPV6 #1", test_normalize_mailhost_addr,
	 /* inet_protocols */ "ipv4, ipv6",
	 /* mailhost_addr */ "ipv6:fc00::0",
	 /* want_return */ 0,
	 /* want_mailhost_addr */ "IPv6:fc00::",
	 /* want_bare_addr */ "fc00::",
	 /* want_addr_family */ AF_INET6
    }, {
	"Normalize IPv4 or IPV6 #2", test_normalize_mailhost_addr,
	 /* inet_protocols */ "ipv4, ipv6",
	 /* mailhost_addr */ "01.02.03.04",
	 /* want_return */ 0,
	 /* want_mailhost_addr */ "1.2.3.4",
	 /* want_bare_addr */ "1.2.3.4",
	 /* want_addr_family */ AF_INET
    }, {
	"Suppress specific outputs #1", test_normalize_mailhost_addr,
	 /* inet_protocols */ "ipv4, ipv6",
	 /* mailhost_addr */ "ipv6:fc00::1",
	 /* want_return */ 0,
	 /* want_mailhost_addr */ 0,
	 /* want_bare_addr */ "fc00::1",
	 /* want_addr_family */ AF_INET6
    }, {
	"Suppress specific outputs #2", test_normalize_mailhost_addr,
	 /* inet_protocols */ "ipv4, ipv6",
	 /* mailhost_addr */ "ipv6:fc00::1",
	 /* want_return */ 0,
	 /* want_mailhost_addr */ "IPv6:fc00::1",
	 /* want_bare_addr */ 0,
	 /* want_addr_family */ AF_INET6
    }, {
	"Suppress specific outputs #3", test_normalize_mailhost_addr,
	 /* inet_protocols */ "ipv4, ipv6",
	 /* mailhost_addr */ "ipv6:fc00::1",
	 /* want_return */ 0,
	 /* want_mailhost_addr */ "IPv6:fc00::1",
	 /* want_bare_addr */ "fc00::1", -1
    }, {
	"Address type mismatch #1", test_normalize_mailhost_addr,
	 /* inet_protocols */ "ipv4, ipv6",
	 /* mailhost_addr */ "::ffff:1.2.3.4",
	 /* want_return */ -1
    }, {
	"Address type mismatch #2", test_normalize_mailhost_addr,
	 /* inet_protocols */ "ipv4",
	 /* mailhost_addr */ "ipv6:fc00::1",
	 /* want_return */ -1
    }, {
	"Address type mismatch #3", test_normalize_mailhost_addr,
	 /* inet_protocols */ "ipv6",
	 /* mailhost_addr */ "1.2.3.4",
	 /* want_return */ -1
    },
};

#include <ptest_main.h>
