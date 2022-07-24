 /*
  * Tests to verify malloc sanity checks. See comments in ptest_main.h for a
  * documented example. The test code depends on the real mymalloc library,
  * so we can 't do destructive tests.
  */

 /*
  * System library.
  */
#include <sys_defs.h>
#include <string.h>

 /*
  * Utility library.
  */
#include <msg.h>
#include <mymalloc.h>

 /*
  * Test library.
  */
#include <ptest.h>

 /*
  * See <ptest_main.h>
  */
typedef struct PTEST_CASE {
    const char *testname;		/* Human-readable description */
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
} PTEST_CASE;

/* Test functions. */

static void test_mymalloc_normal(PTEST_CTX *t, const PTEST_CASE *tp)
{
    void   *ptr;

    ptr = mymalloc(100);
    myfree(ptr);
}

static void test_mymalloc_panic_too_small(PTEST_CTX *t, const PTEST_CASE *tp)
{
    expect_ptest_log_event(t, "panic: mymalloc: requested length 0");
    (void) mymalloc(0);
    ptest_fatal(t, "mymalloc(0) returned");
}

static void test_mymalloc_fatal_out_of_mem(PTEST_CTX *t, const PTEST_CASE *tp)
{
    if (sizeof(size_t) <= 4)
	ptest_skip(t);
    expect_ptest_log_event(t, "fatal: mymalloc: insufficient memory for");
    (void) mymalloc(SSIZE_T_MAX - 100);
    ptest_fatal(t, "mymalloc(SSIZE_T_MAX-100) returned");
}

static void test_myfree_panic_double_free(PTEST_CTX *t, const PTEST_CASE *tp)
{
    void   *ptr;

    expect_ptest_log_event(t, "panic: myfree: corrupt or unallocated memory block");
    ptr = mymalloc(100);
    myfree(ptr);
    /* The next call unavoidably reads unallocated memory */
    myfree(ptr);
    ptest_fatal(t, "double myfree(_) returned");
}

static void test_myfree_panic_null(PTEST_CTX *t, const PTEST_CASE *tp)
{
    expect_ptest_log_event(t, "panic: myfree: null pointer input");
    myfree((void *) 0);
    ptest_fatal(t, "myfree(0) returned");
}

static void test_myrealloc_normal(PTEST_CTX *t, const PTEST_CASE *tp)
{
    void   *ptr;

    ptr = mymalloc(100);
    ptr = myrealloc(ptr, 200);
    myfree(ptr);
}

static void test_myrealloc_panic_too_small(PTEST_CTX *t, const PTEST_CASE *tp)
{
    void   *ptr;

    expect_ptest_log_event(t, "panic: myrealloc: requested length 0");
    ptr = mymalloc(100);
    ptest_defer(t, myfree, ptr);
    (void) myrealloc(ptr, 0);
    ptest_fatal(t, "myrealloc(_, 0) returned");
}

static void test_myrealloc_fatal_out_of_mem(PTEST_CTX *t, const PTEST_CASE *tp)
{
    void   *ptr;

    if (sizeof(size_t) <= 4)
	ptest_skip(t);

    /*
     * Unlike the previous test, this test can't use test_defer(t, myfree,
     * ptr), because myrealloc() clears the memory block's signature field
     * before it calls realloc().
     */
    expect_ptest_log_event(t, "fatal: myrealloc: insufficient memory for");
    ptr = mymalloc(100);
    (void) myrealloc(ptr, SSIZE_T_MAX - 100);
    ptest_fatal(t, "myrealloc(_, SSIZE_T_MAX-100) returned");
}

static void test_myrealloc_panic_unallocated(PTEST_CTX *t, const PTEST_CASE *tp)
{
    void   *ptr;

    expect_ptest_log_event(t, "panic: myrealloc: corrupt or unallocated memory block");
    ptr = mymalloc(100);
    myfree(ptr);
    ptr = myrealloc(ptr, 200);
    ptest_fatal(t, "myrealloc() after free() returned");
}

static void test_myrealloc_panic_null(PTEST_CTX *t, const PTEST_CASE *tp)
{
    expect_ptest_log_event(t, "panic: myrealloc: null pointer input");
    (void) myrealloc((void *) 0, 200);
    ptest_fatal(t, "myrealloc(0, _) returned");
}

static void test_mystrdup_normal(PTEST_CTX *t, const PTEST_CASE *tp)
{
    void   *ptr;

    ptr = mystrdup("foo");
    myfree(ptr);
}

static void test_mystrdup_panic_null(PTEST_CTX *t, const PTEST_CASE *tp)
{
    expect_ptest_log_event(t, "panic: mystrdup: null pointer argument");
    (void) mystrdup((char *) 0);
    ptest_fatal(t, "mystrdup(0) returned");
}

static void test_mystrdup_static_empty(PTEST_CTX *t, const PTEST_CASE *tp)
{
    char   *want;
    char   *got;

    /*
     * Repeated  mystrdup("") produce the same result.
     */
    want = mystrdup("");
    got = mystrdup("");
    if (got != want)
	ptest_error(t, "mystrndup: empty string results differ: got %p, want %p",
		    got, want);

    /*
     * myfree() is a NOOP.
     */
    myfree(want);
    myfree(got);
}

static void test_mystrndup_normal_short(PTEST_CTX *t, const PTEST_CASE *tp)
{
    char   *want = "foo";
    char   *got;

    got = mystrndup("foo", 5);
    if (strcmp(got, want) != 0)
	ptest_error(t, "mystrndup: got '%s', want '%s'", got, want);
    myfree(got);
}

static void test_mystrndup_normal_long(PTEST_CTX *t,
				               const PTEST_CASE *tp)
{
    char   *want = "fooba";
    char   *got;

    got = mystrndup("foobar", 5);
    if (strcmp(got, want) != 0)
	ptest_error(t, "mystrndup: got '%s', want '%s'", got, want);
    myfree(got);
}

static void test_mystrndup_panic_null(PTEST_CTX *t,
				              const PTEST_CASE *tp)
{
    expect_ptest_log_event(t, "panic: mystrndup: null pointer argument");
    (void) mystrndup((char *) 0, 5);
    ptest_fatal(t, "mystrndup(0, _) returned");
}

static void test_mystrndup_panic_too_small(PTEST_CTX *t,
					           const PTEST_CASE *tp)
{
    expect_ptest_log_event(t, "panic: mystrndup: requested length -1");
    (void) mystrndup("foo", -1);
    ptest_fatal(t, "mystrndup(0,	-1) returned");
}

static void test_mystrndup_static_empty(PTEST_CTX *t,
					        const PTEST_CASE *tp)
{
    char   *want;
    char   *got;

    /*
     * Different ways to save an empty string produce the same result.
     */
    want = mystrndup("", 10);
    got = mystrndup("foo", 0);
    if (got != want)
	ptest_error(t, "mystrndup: empty string results differ: got %p, want %p",
		    got, want);

    /*
     * myfree() is a NOOP.
     */
    myfree(want);
    myfree(got);
}

static void test_mymemdup_normal(PTEST_CTX *t, const PTEST_CASE *tp)
{
    void   *ptr;

    ptr = mymemdup("abcdef", 5);
    myfree(ptr);
}

static void test_mymemdup_panic_null(PTEST_CTX *t, const PTEST_CASE *tp)
{
    expect_ptest_log_event(t, "panic: mymemdup: null pointer argument");
    (void) mymemdup((void *) 0, 100);
    ptest_fatal(t, "mymemdup(0, _) returned");
}

static void test_mymemdup_panic_too_small(PTEST_CTX *t, const PTEST_CASE *tp)
{
    expect_ptest_log_event(t, "panic: mymalloc: requested length 0");
    (void) mymemdup("abcdef", 0);
    ptest_fatal(t, "mymemdup(_, 0) returned");
}

static void test_mymemdup_fatal_out_of_mem(PTEST_CTX *t, const PTEST_CASE *tp)
{
    if (sizeof(size_t) <= 4)
	ptest_skip(t);
    expect_ptest_log_event(t, "fatal: mymalloc: insufficient memory for");
    (void) mymemdup("abcdef", SSIZE_T_MAX - 100);
    ptest_fatal(t, "mymemdup(_, SSIZE_T_MAX-100) returned");
}

const PTEST_CASE ptestcases[] = {
    {"mymalloc + myfree normal case", test_mymalloc_normal,
    },
    {"mymalloc panic for too small request", test_mymalloc_panic_too_small,
    },
    {"mymalloc fatal for out of memory", test_mymalloc_fatal_out_of_mem,
    },
    {"myfree panic for double free", test_myfree_panic_double_free,
    },
    {"myfree panic for null input", test_myfree_panic_null,
    },
    {"myrealloc + myfree normal case", test_myrealloc_normal,
    },
    {"myrealloc panic for too small request", test_myrealloc_panic_too_small,
    },
    {"myrealloc fatal for out of memory", test_myrealloc_fatal_out_of_mem,
    },
    {"myrealloc panic for unallocated input", test_myrealloc_panic_unallocated,
    },
    {"myrealloc panic for null input", test_myrealloc_panic_null,
    },
    {"mystrdup + myfree normal case", test_mystrdup_normal,
    },
    {"mystrdup panic for null input", test_mystrdup_panic_null,
    },
    {"mystrdup static result for empty string", test_mystrdup_static_empty,
    },
    {"mystrndup + myfree normal short", test_mystrndup_normal_short,
    },
    {"mystrndup + myfree normal long", test_mystrndup_normal_long,
    },
    {"mystrndup panic for null input", test_mystrndup_panic_null,
    },
    {"mystrndup panic for for too small size", test_mystrndup_panic_too_small,
    },
    {"mystrndup static result for empty string", test_mystrndup_static_empty,
    },
    {"mymemdup normal case", test_mymemdup_normal,
    },
    {"mymemdup panic for null input", test_mymemdup_panic_null,
    },
    {"mymemdup panic for too small request", test_mymemdup_panic_too_small,
    },
    {"mymemdup fatal for out of memory", test_mymemdup_fatal_out_of_mem,
    },
};

#include <ptest_main.h>
