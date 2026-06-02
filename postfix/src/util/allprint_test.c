 /*
  * Test program to exercise allprint.c. See PTEST_README for documentation.
  */

 /*
  * System library.
  */
#include <sys_defs.h>
#include <ctype.h>

 /*
  * Utility library.
  */
#include <stringops.h>

 /*
  * Test library.
  */
#include <ptest.h>

typedef struct PTEST_CASE {
    const char *testname;		/* identifies test case */
    void    (*action) (PTEST_CTX *t, const struct PTEST_CASE *tp);
} PTEST_CASE;

static void test_allprint(PTEST_CTX *t, const PTEST_CASE *tp)
{
    if (allprint != all_isprint)
	ptest_error(t, "allprint %p != all_isprint %p",
		    (void *) allprint, (void *) all_isprint);
}

static void test_all_isprint(PTEST_CTX *t, const PTEST_CASE *tp)
{
    unsigned char long_buf[256];
    int     code;
    int     pos;

    for (pos = 0, code = 1; code < 256; code++) {
	unsigned char short_buf[] = {code, 0};

	if (ISPRINT(code)) {
	    long_buf[pos++] = code;
	    if (!all_isprint((char *) short_buf)) 
		ptest_error(t, "all_isprint(\"\\x%x\"): got 0, want 1", code);
	} else {
	    if (all_isprint((char *) short_buf))
		ptest_error(t, "all_isprint(\"\\x%x\"): got 1, want 0", code);
	}
    }
    long_buf[pos] = 0;
    if (!all_isprint((char *) long_buf))
	ptest_error(t, "all_isprint(t, \"%s\"): got 0, want 1", long_buf);
}

static void test_all_isprint_tab(PTEST_CTX *t, const PTEST_CASE *tp)
{
    unsigned char long_buf[256];
    int     code;
    int     pos;

    for (pos = 0, code = 1; code < 256; code++) {
        unsigned char short_buf[] = {code, 0};

        if (ISPRINT(code) || code == '\t') {
            long_buf[pos++] = code;
            if (!all_isprint_tab((char *) short_buf))
                ptest_error(t, "all_isprint_tab(\"\\x%x\"): got 0, want 1",
                             code);
        } else {
            if (all_isprint_tab((char *) short_buf))
                ptest_error(t, "all_isprint_tab(\"\\x%x\"): got 1, want 0",
                            code);
        }
    }
    long_buf[pos] = 0;
    if (!all_isprint_tab((char *) long_buf))
        ptest_error(t, "all_isprint(t, \"%s\"): got 0, want 1", long_buf);
}

 /*
  * The test cases.
  */
static const PTEST_CASE ptestcases[] = {
    {.testname = "test allprint",.action = test_allprint,},
    {.testname = "test all_isprint",.action = test_all_isprint,},
    {.testname = "test all_isprint_tab",.action = test_all_isprint_tab,},
};

#include <ptest_main.h>
