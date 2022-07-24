 /*
  * Test program to exercise login_sender_match.c. See and ptest_main.h for a
  * documented example.
  */

 /*
  * System library.
  */
#include <sys_defs.h>

 /*
  * Global library.
  */
#include <mail_params.h>
#include <login_sender_match.h>

 /*
  * Test library.
  */
#include <ptest.h>

typedef struct PTEST_CASE {
    const char *testname;
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *tp);
    const char *map_names;
    const char *ext_delimiters;
    const char *null_sender;
    const char *wildcard;
    const char *login_name;
    const char *sender_addr;
    int     want_return;
    const char *want_logging;
} PTEST_CASE;

static void tester(PTEST_CTX *t, const PTEST_CASE *tp)
{
    int     got_return;
    LOGIN_SENDER_MATCH *lsm;

    /*
     * Fake variable settings.
     */
    var_double_bounce_sender = DEF_DOUBLE_BOUNCE;
    var_ownreq_special = DEF_OWNREQ_SPECIAL;

#define NUM_TESTS	sizeof(ptestcases)/sizeof(ptestcases[0])

#if 0
    msg_info("name=%s", tp->title);
    msg_info("map_names=%s", tp->map_names);
    msg_info("ext_delimiters=%s", tp->ext_delimiters);
    msg_info("null_sender=%s", tp->null_sender);
    msg_info("wildcard=%s", tp->wildcard);
    msg_info("login_name=%s", tp->login_name);
    msg_info("sender_addr=%s", tp->sender_addr);
    msg_info("want_return=%d", tp->exp_return);
#endif
    lsm = login_sender_create("test map", tp->map_names,
			      tp->ext_delimiters, tp->null_sender,
			      tp->wildcard);
    if (tp->want_logging)
	expect_ptest_log_event(t, tp->want_logging);
    got_return = login_sender_match(lsm, tp->login_name, tp->sender_addr);
    if (got_return != tp->want_return)
	ptest_error(t, "login_sender_match() got %d, want %d",
		    got_return, tp->want_return);
    login_sender_free(lsm);
}

static const PTEST_CASE ptestcases[] = {
    {"wildcard works", tester,
	"inline:{root=*, {foo = foo,foo@example.com}, bar=<>}",
	"+-", "<>", "*", "root", "anything", LSM_STAT_FOUND
    },
    {"unknown user", tester,
	"inline:{root=*, {foo = foo,foo@example.com}, bar=<>}",
	"+-", "<>", "*", "toor", "anything", LSM_STAT_NOTFOUND
    },
    {"bare user", tester,
	"inline:{root=*, {foo = foo,foo@example.com}, bar=<>}",
	"+-", "<>", "*", "foo", "foo", LSM_STAT_FOUND
    },
    {"user@domain", tester,
	"inline:{root=*, {foo = foo,foo@example.com}, bar=<>}",
	"+-", "<>", "*", "foo", "foo@example.com", LSM_STAT_FOUND
    },
    {"user+ext@domain", tester,
	"inline:{root=*, {foo = foo,foo@example.com}, bar=<>}",
	"+-", "<>", "*", "foo", "foo+bar@example.com", LSM_STAT_FOUND
    },
    {"wrong sender", tester,
	"inline:{root=*, {foo = foo,foo@example.com}, bar=<>}",
	"+-", "<>", "*", "foo", "bar@example.com", LSM_STAT_NOTFOUND
    },
    {"@domain", tester,
	"inline:{root=*, {foo = @example.com}, bar=<>}",
	"+-", "<>", "*", "foo", "anyone@example.com", LSM_STAT_FOUND
    },
    {"wrong @domain", tester,
	"inline:{root=*, {foo = @example.com}, bar=<>}",
	"+-", "<>", "*", "foo", "anyone@example.org", LSM_STAT_NOTFOUND
    },
    {"null sender", tester,
	"inline:{root=*, {foo = foo,foo@example.com}, bar=<>}",
	"+-", "<>", "*", "bar", "", LSM_STAT_FOUND
    },
    {"wrong null sender", tester,
	"inline:{root=*, {foo = foo,foo@example.com}, bar=<>}",
	"+-", "<>", "*", "baz", "", LSM_STAT_NOTFOUND
    },
    {"error", tester,
	"inline:{root=*}, fail:sorry",
	"+-", "<>", "*", "baz", "whatever", LSM_STAT_RETRY,
	"fail:sorry lookup error"
    },
    {"no error", tester,
	"inline:{root=*}, fail:sorry",
	"+-", "<>", "*", "root", "whatever", LSM_STAT_FOUND
    },
    {"unknown uid:number", tester,
	"inline:{root=*, {uid:12345 = foo,foo@example.com}, bar=<>}",
	"+-", "<>", "*", "uid:54321", "foo", LSM_STAT_NOTFOUND
    },
    {"known uid:number", tester,
	"inline:{root=*, {uid:12345 = foo,foo@example.com}, bar=<>}",
	"+-", "<>", "*", "uid:12345", "foo", LSM_STAT_FOUND
    },
    {"unknown \"other last\"", tester,
	"inline:{root=*, {foo = \"first last\",\"first last\"@example.com}, bar=<>}",
	"+-", "<>", "*", "foo", "other last", LSM_STAT_NOTFOUND
    },
    {"bare \"first last\"", tester,
	"inline:{root=*, {foo = \"first last\",\"first last\"@example.com}, bar=<>}",
	"+-", "<>", "*", "foo", "first last", LSM_STAT_FOUND
    },
    {"\"first last\"@domain", tester,
	"inline:{root=*, {foo = \"first last\",\"first last\"@example.com}, bar=<>}",
	"+-", "<>", "*", "foo", "first last@example.com", LSM_STAT_FOUND
    },
};

#include <ptest_main.h>
