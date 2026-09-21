 /*
  * Test program to exercise asan_logger.h. See PTEST_README for documentation.
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

static void test_oob_write(PTEST_CTX *t, const PTEST_CASE *tp)
{
#if defined(HAS_ASAN)
    struct foo {
	size_t  len;
	int    *data;
    };
    struct foo *pfoo = mymalloc(sizeof(*pfoo));
    int     len = 2;

    pfoo->data = (int *) mymalloc(len * sizeof(pfoo->data[0]));
    pfoo->len = len;
    expect_ptest_log_event(t, "AddressSanitizer: heap-buffer-overflow");
    pfoo->data[2] = 0;
    myfree((void *) pfoo->data);
    myfree((void *) pfoo);
#else
    msg_info("Skipping this test: no ASAN");
    ptest_skip(t);
#endif
}

static const PTEST_CASE ptestcases[] = {
    {"test out-of-bounds write", test_oob_write,},
};

#include <ptest_main.h>
