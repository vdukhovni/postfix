/*++
/* NAME
/*      pol_stats_test 1t
/* SUMMARY
/*      pol_stats unit tests
/* SYNOPSIS
/*      ./pol_stats_test
/* DESCRIPTION
/*      pol_stats_test runs and logs each configured test, reports if a
/*      test is a PASS or FAIL, and returns an exit status of zero
/*      if all tests are a PASS.
/* LICENSE
/* .ad
/* .fi
/*      The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*      Wietse Venema
/*      porcupine.org
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>
#include <stdlib.h>
#include <string.h>

 /*
  * Utility library.
  */
#include <msg.h>
#include <msg_vstream.h>
#include <stringops.h>
#include <vstring.h>

 /*
  * Application-specific.
  */
#include <pol_stats.h>

 /*
  * A level-based feature is implicitly relaxed when the initial feature name
  * differs from the final (actually realized) name. The initial enforcement
  * is always POL_STAT_ENF_FULL and is not explicitly specified in test data.
  */
#define pol_stat_activate_lev(stats, name) \
        pol_stat_activate((stats), 0, (name), POL_STAT_ENF_FULL)
#define pol_stat_decide_lev(stats, name, status) \
        pol_stat_decide((stats), 0, (name), (status))

struct lev_test_data {
    const char *target_name;
    const char *final_name;
    int     final_status;
};

 /*
  * Other features may start as relaxed and may or may not change names. Here
  * the initial enforcement level is explicitly specified in test data.
  */
#define pol_stat_activate_misc(stats, name, enforce) \
        pol_stat_activate((stats), 1, (name), (enforce))
#define pol_stat_decide_misc(stats, name, status) \
        pol_stat_decide((stats), 1, (name), (status))

struct misc_test_data {
    const char *target_name;
    const char *final_name;
    bool    initial_enforce;
    int     final_status;
};

 /*
  * Tests cases and test data.
  */
typedef struct TEST_CASE {
    const char *label;
    const char *want;
    int     (*action) (const struct TEST_CASE *);
    struct lev_test_data lev_data;
    struct misc_test_data misc_data;
} TEST_CASE;

static POL_STATS *pstats;
static VSTRING *buf;

static int test_pol_stats(const TEST_CASE *tp)
{
    char   *got;
    int     errors = 0;

    pol_stats_revert(pstats);
    VSTRING_RESET(buf);

    if (tp->lev_data.target_name) {
	pol_stat_activate_lev(pstats, tp->lev_data.target_name);
	if (tp->lev_data.final_name)
	    pol_stat_decide_lev(pstats, tp->lev_data.final_name,
				tp->lev_data.final_status);
    }
    if (tp->misc_data.target_name) {
	pol_stat_activate_misc(pstats, tp->misc_data.target_name,
			       tp->misc_data.initial_enforce);
	if (tp->misc_data.final_name)
	    pol_stat_decide_misc(pstats, tp->misc_data.final_name,
				 tp->misc_data.final_status);
    }
    pol_stats_format(buf, pstats);
    got = vstring_str(buf);
    if (strcmp(got, tp->want) != 0) {
	msg_warn("got '%s', want '%s", got, tp->want);
	errors++;
    }
    return (errors == 0);
}

static TEST_CASE test_cases[] = {
    /* Tests for names that explicitly indicate status. */
    {"high_full_compliant", "high", test_pol_stats,
	{"high", "high", POL_STAT_COMPLIANT}, {},
    },
    {"high_full_undecided", "high?", test_pol_stats,
	{"high", 0, POL_STAT_COMPLIANT}, {},
    },
    {"high_full_violation", "!high", test_pol_stats,
	{"high", "high", POL_STAT_VIOLATION}, {},
    },
    {"high_may_explicit_relaxed_compliant", "high:none", test_pol_stats,
	{"high", "none", POL_STAT_COMPLIANT}, {},
    },
    /* Tests for names that need () to indicate relaxation. */
    {"misc_full_compliant", "misc", test_pol_stats,
	{}, {"misc", "misc", POL_STAT_ENF_FULL, POL_STAT_COMPLIANT},
    },
    {"misc_explicit_relaxed_compliant", "(misc)", test_pol_stats,
	{}, {"misc", "misc", POL_STAT_ENF_RELAXED, POL_STAT_COMPLIANT},
    },
    {"misc_explicit_relaxed_violation", "!(misc)", test_pol_stats,
	{}, {"misc", "misc", POL_STAT_ENF_RELAXED, POL_STAT_VIOLATION},
    },
    {"misc_explicit_relaxed_undecided", "(misc)?", test_pol_stats,
	{}, {"misc", 0, POL_STAT_ENF_RELAXED, POL_STAT_VIOLATION},
    },
    {"misc_none_explicit_relaxed_compliant", "(misc:none)", test_pol_stats,
	{}, {"misc", "none", POL_STAT_ENF_RELAXED, POL_STAT_COMPLIANT},
    },
    /* Combined policy status tests. */
    {"multi_feature_full_compliant", "high/misc", test_pol_stats,
	{"high", "high", POL_STAT_COMPLIANT},
	{"misc", "misc", POL_STAT_ENF_FULL, POL_STAT_COMPLIANT},
    },
    0,
};

int     main(int argc, char **argv)
{
    const TEST_CASE *tp;
    int     pass = 0;
    int     fail = 0;

    msg_vstream_init(sane_basename((VSTRING *) 0, argv[0]), VSTREAM_ERR);
    pstats = pol_stats_create();
    buf = vstring_alloc(100);

    for (tp = test_cases; tp->label != 0; tp++) {
	msg_info("RUN  %s", tp->label);
	if (tp->action(tp) == 0) {
	    fail++;
	    msg_info("FAIL %s", tp->label);
	} else {
	    msg_info("PASS %s", tp->label);
	    pass++;
	}
    }
    pol_stats_free(pstats);
    vstring_free(buf);

    msg_info("PASS=%d FAIL=%d", pass, fail);
    exit(fail != 0);
}
