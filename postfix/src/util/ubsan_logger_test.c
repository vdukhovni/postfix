 /*
  * Test program to exercise ubsan_logger.h. See PTEST_README for documentation.
  */

 /*
  * System library.
  */
#include <sys_defs.h>

 /*
  * Utility library.
  */
#include <msg.h>
#include <mymalloc.h>

 /*
  * Test library.
  */
#include <ptest.h>

typedef struct PTEST_CASE {
    const char *testname;
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
} PTEST_CASE;

static void test_counted_by(PTEST_CTX *t, const PTEST_CASE *tp)
{
#if defined(HAS_UBSAN) && __has_attribute(__counted_by__)
    struct foo {
	size_t  len;
	int    *data __counted_by(len);
    };
    struct foo *pfoo = mymalloc(sizeof(*pfoo));
    int     len = 2;

    pfoo->data = (int *) mymalloc(len * sizeof(pfoo->data[0]));
    pfoo->len = len;
    expect_ptest_log_event(t, "warning: ubsan_logger_test.c:");
    pfoo->data[2] = 0;
#else
    msg_info("Skipping this test: no UBSAN or no __counted_by__");
    ptest_skip(t);
#endif
}

static const PTEST_CASE ptestcases[] = {
    {"test __counted_by__ support", test_counted_by,},
};

#include <ptest_main.h>
