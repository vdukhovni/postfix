 /*
  * Test program to exercise ptest_log functions including logging. See
  * comments in ptest_main.h and pmock_expect_test.c for a documented
  * example.
  */

 /*
  * System library.
  */
#include <sys_defs.h>

 /*
  * Test library.
  */
#include <ptest.h>

typedef struct PTEST_CASE {
    const char *testname;		/* Human-readable description */
    void    (*action) (PTEST_CTX *t, const struct PTEST_CASE *);
} PTEST_CASE;

static void ptest_log_non_error(PTEST_CTX *t, const PTEST_CASE *unused)
{
    /* This test passes if there is no error. */
    expect_ptest_log_event(t, "this is a non-error");
    msg_info("this is a non-error");
}

static void ptest_log_flags_unexpected_message(PTEST_CTX *t, const PTEST_CASE *unused)
{
    expect_ptest_error(t, "this is a forced 'Unexpected log event' error");
    msg_info("this is a forced 'Unexpected log event' error");
}

static void ptest_log_flags_missing_message(PTEST_CTX *t, const PTEST_CASE *unused)
{
    expect_ptest_error(t, "this is a forced 'Missing log event' error");
    expect_ptest_log_event(t, "this is a forced 'Missing log event' error");
}

 /*
  * Test cases.
  */
const PTEST_CASE ptestcases[] = {
    {
	"ptest_log_non_error", ptest_log_non_error,
    },
    {
	"ptest_log_flags_unexpected_message", ptest_log_flags_unexpected_message,
    },
    {
	"ptest_log_flags_missing_message", ptest_log_flags_missing_message,
    },
};

#include <ptest_main.h>
