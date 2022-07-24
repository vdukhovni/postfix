 /*
  * Test program to exercise known_tcp_ports.c. See ptest_main.h for a
  * documented example.
  */

 /*
  * System library.
  */
#include <sys_defs.h>
#include <string.h>

 /*
  * Utility library
  */
#include <known_tcp_ports.h>
#include <msg.h>

 /*
  * Test library.
  */
#include <ptest.h>

struct association {
    const char *lhs;			/* service name */
    const char *rhs;			/* service port */
};

struct probe {
    const char *query;			/* query */
    const char *want_reply;		/* expected reply */
};

typedef struct PTEST_CASE {
    const char *testname;		/* identifies test case */
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
    struct association associations[10];
    const char *want_err;		/* expected error */
    const char *want_export;		/* expected export output */
    struct probe probes[10];
} PTEST_CASE;

static void test_known_tcp_ports(PTEST_CTX *t, const PTEST_CASE *tp)
{
    VSTRING *export_buf;
    const struct association *ap;
    const struct probe *pp;
    const char *got_err;
    const char *got_reply;
    const char *got_export;

#define STRING_OR_NULL(s) ((s) ? (s) : "(null)")

    export_buf = vstring_alloc(100);
    for (got_err = 0, ap = tp->associations; got_err == 0 && ap->lhs != 0; ap++)
	got_err = add_known_tcp_port(ap->lhs, ap->rhs);
    if (!got_err != !tp->want_err) {
	ptest_error(t, "got error '%s', want '%s'",
		    STRING_OR_NULL(got_err), STRING_OR_NULL(tp->want_err));
    } else if (got_err != 0) {
	if (strcmp(got_err, tp->want_err) != 0) {
	    ptest_error(t, "got err '%s', want '%s'", got_err, tp->want_err);
	}
    } else {
	got_export = export_known_tcp_ports(export_buf);
	if (strcmp(got_export, tp->want_export) != 0) {
	    ptest_error(t, "got export '%s', want '%s'",
			got_export, tp->want_export);
	}
	for (pp = tp->probes; pp->query != 0; pp++) {
	    got_reply = filter_known_tcp_port(pp->query);
	    if (strcmp(got_reply, pp->want_reply) != 0) {
		ptest_error(t, "got reply '%s', want '%s'",
			    got_reply, pp->want_reply);
		break;
	    }
	}
    }
    clear_known_tcp_ports();
    vstring_free(export_buf);
}

const PTEST_CASE ptestcases[] = {
    {"good", test_known_tcp_ports,
	 /* association */ {{"smtp", "25"}, {"lmtp", "24"}, 0},
	 /* error */ 0,
	 /* export */ "lmtp=24 smtp=25",
	 /* probe */ {{"smtp", "25"}, {"1", "1"}, {"x", "x"}, {"lmtp", "24"}, 0}
    },
    {"duplicate lhs", test_known_tcp_ports,
	 /* association */ {{"smtp", "25"}, {"smtp", "100"}, 0},
	 /* error */ "duplicate service name"
    },
    {"numerical lhs", test_known_tcp_ports,
	 /* association */ {{"100", "100"}, 0},
	 /* error */ "numerical service name"
    },
    {"symbolic rhs", test_known_tcp_ports,
	 /* association */ {{"smtp", "lmtp"}, 0},
	 /* error */ "non-numerical service port"
    },
    {"uninitialized", test_known_tcp_ports,
	 /* association */ {0},
	 /* error */ 0,
	 /* export */ "",
	 /* probe */ {{"smtp", "smtp"}, {"1", "1"}, {"x", "x"}, 0}
    },
    {"too large", test_known_tcp_ports,
	 /* association */ {{"one", "65535"}, {"two", "65536"}, 0},
	 /* error */ "port number out of range",
    },
};

 /*
  * Test library.
  */
#include <ptest_main.h>
