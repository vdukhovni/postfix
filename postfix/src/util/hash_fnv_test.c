 /*
  * Test program to exercise the hash_fnv implementation. See PTEST_README
  * for documentation.
  */

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
#include <hash_fnv.h>

 /*
  * Test library.
  */
#include <ptest.h>

typedef struct PTEST_CASE {
    const char *testname;
    void    (*action) (PTEST_CTX *t, const struct PTEST_CASE *tp);
    HASH_FNV_T want_hval;
    const char *str;
} PTEST_CASE;

static void setup_test(void)
{

    /*
     * Sanity check.
     */
#ifdef STRICT_FNV1A
    msg_fatal("This test requires no STRICT_FNV1A");
#endif

    /*
     * Force unseeded hash, to make tests predictable.
     */
    if (putenv("NORANDOMIZE=") != 0)
	msg_fatal("putenv(\"NORANDOMIZE=\"): %m");
}

static void test_known_input(PTEST_CTX *t, const PTEST_CASE *tp)
{
    HASH_FNV_T got_hval;

    setup_test();

    if ((got_hval = hash_fnvz(tp->str)) != tp->want_hval)
	ptest_error(t, "hash_fnvz(\"%s\") got %lu, want %lu",
		    tp->str, (unsigned long) got_hval,
		    (unsigned long) tp->want_hval);

    if ((got_hval = hash_fnv(tp->str, strlen(tp->str))) != tp->want_hval)
	ptest_error(t, "hash_fnv(\"%s\", strlen(\"%s\")) got %lu, want %lu",
		    tp->str, tp->str, (unsigned long) got_hval,
		    (unsigned long) tp->want_hval);
}


static const PTEST_CASE ptestcases[] =
{
#ifdef USE_FNV_32BIT
    "test_known_input_overdeeply", test_known_input, 0x1c00fc06UL, "overdeeply",
    "test_known_input_undescript", test_known_input, 0x1c00fc06UL, "undescript",
    "test_known_input_fanfold", test_known_input, 0x1e1e52a4UL, "fanfold",
    "test_known_input_phrensied", test_known_input, 0x1e1e52a4UL, "phrensied",
#else
    "test_known_input_overdeeply", test_known_input, 0xda19999ec0bda706ULL, "overdeeply",
    "test_known_input_undescript", test_known_input, 0xd7b9e43f26396a66ULL, "undescript",
    "test_known_input_fanfold", test_known_input, 0xa50c585d385a2604ULL, "fanfold",
    "test_known_input_phrensied", test_known_input, 0x1ec3ef9bb2b734a4ULL, "phrensied",
#endif
};

#include <ptest_main.h>
