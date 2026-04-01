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
} PTEST_CASE;

typedef struct TEST_DATA {
    const char *test_label;
    const char *map_names;
    const char *ext_delimiters;
    const char *null_sender;
    const char *wildcard;
    const char *login_name;
    const char *sender_addr;
    int     want_return;
    const char *want_logging;
} TEST_DATA;

static const TEST_DATA test_data[] = {
    {"wildcard works",
	"inline:{root=*, {foo = foo,foo@example.com}, bar=<>}",
	.ext_delimiters = "+-",
	.null_sender = "<>",
	.wildcard = "*",
	.login_name = "root",
	.sender_addr = "anything",
	.want_return = LSM_STAT_FOUND,
    },
    {"unknown user",
	"inline:{root=*, {foo = foo,foo@example.com}, bar=<>}",
	.ext_delimiters = "+-",
	.null_sender = "<>",
	.wildcard = "*",
	.login_name = "toor",
	.sender_addr = "anything",
	.want_return = LSM_STAT_NOTFOUND,
    },
    {"unknown user -> error",
	"inline:{root=*, {foo = foo,foo@example.com}, bar=<>}, fail:sorry",
	.ext_delimiters = "+-",
	.null_sender = "<>",
	.wildcard = "*",
	.login_name = "toor",
	.sender_addr = "anything",
	.want_return = LSM_STAT_RETRY,
	.want_logging = "fail:sorry lookup error"
    },
    {"bare user",
	"inline:{root=*, {foo = foo,foo@example.com}, bar=<>}",
	.ext_delimiters = "+-",
	.null_sender = "<>",
	.wildcard = "*",
	.login_name = "foo",
	.sender_addr = "foo",
	.want_return = LSM_STAT_FOUND,
    },
    {"user@domain",
	"inline:{root=*, {foo = foo,foo@example.com}, bar=<>}",
	.ext_delimiters = "+-",
	.null_sender = "<>",
	.wildcard = "*",
	.login_name = "foo",
	.sender_addr = "foo@example.com",
	.want_return = LSM_STAT_FOUND,
    },
    {"user+ext@domain",
	"inline:{root=*, {foo = foo,foo@example.com}, bar=<>}",
	.ext_delimiters = "+-",
	.null_sender = "<>",
	.wildcard = "*",
	.login_name = "foo",
	.sender_addr = "foo+bar@example.com",
	.want_return = LSM_STAT_FOUND,
    },
    {"wrong sender",
	"inline:{root=*, {foo = foo,foo@example.com}, bar=<>}",
	.ext_delimiters = "+-",
	.null_sender = "<>",
	.wildcard = "*",
	.login_name = "foo",
	.sender_addr = "bar@example.com",
	.want_return = LSM_STAT_NOTFOUND,
    },
    {"@domain",
	"inline:{root=*, {foo = @example.com}, bar=<>}",
	.ext_delimiters = "+-",
	.null_sender = "<>",
	.wildcard = "*",
	.login_name = "foo",
	.sender_addr = "anyone@example.com",
	.want_return = LSM_STAT_FOUND,
    },
    {"wrong @domain",
	"inline:{root=*, {foo = @example.com}, bar=<>}",
	.ext_delimiters = "+-",
	.null_sender = "<>",
	.wildcard = "*",
	.login_name = "foo",
	.sender_addr = "anyone@example.org",
	.want_return = LSM_STAT_NOTFOUND,
    },
    {"null sender",
	"inline:{root=*, {foo = foo,foo@example.com}, bar=<>}",
	.ext_delimiters = "+-",
	.null_sender = "<>",
	.wildcard = "*",
	.login_name = "bar",
	.sender_addr = "",
	.want_return = LSM_STAT_FOUND,
    },
    {"wrong null sender",
	"inline:{root=*, {foo = foo,foo@example.com}, bar=<>}",
	.ext_delimiters = "+-",
	.null_sender = "<>",
	.wildcard = "*",
	"baz",
	.sender_addr = "",
	.want_return = LSM_STAT_NOTFOUND,
    },
    {"error",
	"inline:{root=*}, fail:sorry",
	.ext_delimiters = "+-",
	.null_sender = "<>",
	.wildcard = "*",
	.login_name = "baz",
	.sender_addr = "whatever",
	.want_return = LSM_STAT_RETRY,
	.want_logging = "fail:sorry lookup error"
    },
    {"no error",
	"inline:{root=*}, fail:sorry",
	.ext_delimiters = "+-",
	.null_sender = "<>",
	.wildcard = "*",
	.login_name = "root",
	.sender_addr = "whatever",
	.want_return = LSM_STAT_FOUND,
    },
    {"unknown uid:number",
	"inline:{root=*, {uid:12345 = foo,foo@example.com}, bar=<>}",
	.ext_delimiters = "+-",
	.null_sender = "<>",
	.wildcard = "*",
	.login_name = "uid:54321",
	.sender_addr = "foo",
	.want_return = LSM_STAT_NOTFOUND,
    },
    {"known uid:number",
	"inline:{root=*, {uid:12345 = foo,foo@example.com}, bar=<>}",
	.ext_delimiters = "+-",
	.null_sender = "<>",
	.wildcard = "*",
	.login_name = "uid:12345",
	.sender_addr = "foo",
	.want_return = LSM_STAT_FOUND,
    },
    {"unknown \"other last\"",
	"inline:{root=*, {foo = \"first last\",\"first last\"@example.com}, bar=<>}",
	.ext_delimiters = "+-",
	.null_sender = "<>",
	.wildcard = "*",
	.login_name = "foo",
	.sender_addr = "other last",
	.want_return = LSM_STAT_NOTFOUND,
    },
    {"bare \"first last\"",
	"inline:{root=*, {foo = \"first last\",\"first last\"@example.com}, bar=<>}",
	.ext_delimiters = "+-",
	.null_sender = "<>",
	.wildcard = "*",
	.login_name = "foo",
	.sender_addr = "first last",
	.want_return = LSM_STAT_FOUND,
    },
    {"\"first last\"@domain",
	"inline:{root=*, {foo = \"first last\",\"first last\"@example.com}, bar=<>}",
	.ext_delimiters = "+-",
	.null_sender = "<>",
	.wildcard = "*",
	.login_name = "foo",
	.sender_addr = "first last@example.com",
	.want_return = LSM_STAT_FOUND,
    },
};

static void test_login_sender_match(PTEST_CTX *t, const PTEST_CASE *unused)
{
    const TEST_DATA *tp;

    for (tp = test_data; tp < test_data + PTEST_NROF(test_data); tp++) {
	PTEST_RUN(t, tp->test_label, {
	    int     got_return;
	    LOGIN_SENDER_MATCH *lsm;

	    /*
	     * Fake variable settings.
	     */
	    var_double_bounce_sender = DEF_DOUBLE_BOUNCE;
	    var_ownreq_special = DEF_OWNREQ_SPECIAL;

#if 0
	    pest_info(t, "name=%s", tp->title);
	    pest_info(t, "map_names=%s", tp->map_names);
	    pest_info(t, "ext_delimiters=%s", tp->ext_delimiters);
	    pest_info(t, "null_sender=%s", tp->null_sender);
	    pest_info(t, "wildcard=%s", tp->wildcard);
	    pest_info(t, "login_name=%s", tp->login_name);
	    pest_info(t, "sender_addr=%s", tp->sender_addr);
	    pest_info(t, "want_return=%d", tp->exp_return);
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
	});
    }
}

static const PTEST_CASE ptestcases[] = {
    {"test_login_sender_match", test_login_sender_match,},
};

#include <ptest_main.h>
