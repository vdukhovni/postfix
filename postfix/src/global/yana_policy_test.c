 /*
  * Test program to exercise yana_policy.c. See ptest_main.h for a documented
  * example.
  */

 /*
  * System library.
  */
#include <sys_defs.h>
#include <string.h>

 /*
  * Utility library.
  */
#include <dict.h>

 /*
  * Global library.
  */
#include <yana_policy.h>

 /*
  * Test library.
  */
#include <ptest.h>

typedef struct PTEST_CASE {
    const char *testname;
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);

    /*
     * yana_policy_create() inputs.
     */
    const char *policy;
    int     match_parent;

    /*
     * yana_policy_lookup() inputs.
     */
    const char *client_name;
    const char *client_addr;

    /*
     * yana_policy_lookup() outputs.
     */
    const char *want_warning;		/* expected warning or null */
    const char *want_found;		/* expected lookup result or null */
    int     want_error;			/* expected DICT_ERR_XXX value */
} PTEST_CASE;

static void test_yana_policy(PTEST_CTX *t, const PTEST_CASE *tp)
{
    YANA_POLICY *policy;
    const char *got_found;

#define STR_OR_NULL(s) ((s) ? (s) : "(null)")

    /*
     * Setup.
     */
    policy = yana_policy_create(tp->testname, tp->policy, tp->match_parent);
    if (tp->want_warning)
	expect_ptest_log_event(t, tp->want_warning);

    /*
     * Detonate.
     */
    got_found = yana_policy_lookup(policy, tp->client_name, tp->client_addr);

    /*
     * Verify.
     */
    if (policy->error != tp->want_error) {
	ptest_error(t, "unexpected policy error result: got '%d', want '%d'",
		    policy->error, tp->want_error);
    }
    if (!!got_found != !!tp->want_found) {
	ptest_error(t, "unexpected policy lookup result: got '%s', want '%s'",
		    STR_OR_NULL(got_found), STR_OR_NULL(tp->want_found));
    }
    if (got_found && tp->want_found) {
	if (strcmp(got_found, tp->want_found) != 0) {
	    ptest_error(t, "unexpected policy result: got '%s', want '%s'",
			got_found, tp->want_found);
	}
    }
    yana_policy_free(policy);
}

static const PTEST_CASE ptestcases[] = {
    {"good-fullname-match", test_yana_policy,
	.policy = "inline:{mx.example.com=name-match, 1.2.3.4=addr-match}",
	.client_name = "mx.example.com",
	.client_addr = "1.2.3.4",
	.want_found = "name-match",
    },
    {"good-full-v6-match", test_yana_policy,
	.policy = "inline:{mx.example.com=name-match, 1:2::3:4=addr-match}",
	.client_name = "other.example.com",
	.client_addr = "1:2::3:4",
	.want_found = "addr-match",
    },
    {"good-full-v4-match", test_yana_policy,
	.policy = "inline:{mx.example.com=name-match, 1.2.3.4=addr-match}",
	.client_name = "other.example.com",
	.client_addr = "1.2.3.4",
	.want_found = "addr-match",
    },
    {"dunno-overrides", test_yana_policy,
	.policy = "inline:{mx.example.com=dunno, 1.2.3.4=addr-match}",
	.client_name = "mx.example.com",
	.client_addr = "1.2.3.4",
    },
    {"good-non-match", test_yana_policy,
	.policy = "inline:{mx.example.com=name-match, 1.2.3.4=addr-match}",
	.client_name = "other.example.com",
	.client_addr = "1.2.3.5",
    },
    {"propagates-name-lookup-error", test_yana_policy,
	.policy = "fail:whatever",
	.client_name = "mx.example.com",
	.want_warning = "warning: fail:whatever lookup error",
	.want_error = DICT_ERR_RETRY,
    },
    {"propagates-addr-lookup-error", test_yana_policy,
	.policy = "fail:whatever",
	.client_addr = "1.2.3.4",
	.want_warning = "warning: fail:whatever lookup error",
	.want_error = DICT_ERR_RETRY,
    },
    {"good-parent-match", test_yana_policy,
	.policy = "inline:{.example.com=name-match, 1.2.3.4=addr-match}",
	.client_name = "mx.example.com",
	.client_addr = "1.2.3.4",
	.want_found = "name-match",
    },
    {"good-tld-match", test_yana_policy,
	.policy = "inline:{.com=name-match, 1.2.3.4=addr-match}",
	.client_name = "mx.example.com",
	.client_addr = "1.2.3.4",
	.want_found = "name-match",
    },
    {"good-v4-subnet-match", test_yana_policy,
	.policy = "inline:{example.com=name-match, 1.2=addr-match}",
	.client_name = "other.example",
	.client_addr = "1.2.3.4",
	.want_found = "addr-match",
    },
    {"good-v4-subnet-non-match", test_yana_policy,
	.policy = "inline:{example.com=name-match, 1.2=addr-match}",
	.client_name = "other.example",
	.client_addr = "2.2.3.4",
    },
    {"good-v6-subnet-match", test_yana_policy,
	.policy = "inline:{example.com=name-match, 1:2=addr-match}",
	.client_name = "other.example",
	.client_addr = "1:2::3:4",
	.want_found = "addr-match",
    },
    {"good-v6-subnet-non-match", test_yana_policy,
	.policy = "inline:{example.com=name-match, 1:2=addr-match}",
	.client_name = "other.example",
	.client_addr = "2:2::3:4",
    },
    {"good-regexp-match", test_yana_policy,
	.policy = "regexp:{{/\\.example\\.com$$/ name-match}}",
	.client_name = "mx.example.com",
	.client_addr = "1:2::3:4",
	.want_found = "name-match",
    },
    {"good-regexp-non-match", test_yana_policy,
	.policy = "regexp:{{/\\.example\\.com/ name-match}}",
	.client_name = "other.example",
	.client_addr = "1:2::3:4",
    },
    {"good-cidr-match", test_yana_policy,
	.policy = "cidr:{{1.2.3.4 addr-match}}",
	.client_addr = "1.2.3.4",
	.want_found = "addr-match",
    },
    {"good-cidr-non-match", test_yana_policy,
	.policy = "cidr:{{1.2.3.4 addr-match}}",
	.client_addr = "1.2.3.5",
    },
    {"null-data", test_yana_policy,
	.policy = "fail:whatever",
    },
};

#include <ptest_main.h>
