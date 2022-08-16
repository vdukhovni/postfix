 /*
  * Test program to exercise argv.c. See PTEST_README for documentation.
  */

 /*
  * System library.
  */
#include <sys_defs.h>
#include <string.h>

 /*
  * Utility library.
  */
#include <argv.h>

 /*
  * Test library.
  */
#include <ptest.h>

#define ARRAY_LEN	(10)

 /*
  * All test data can fit in the same PTEST_CASE structure.
  */
typedef struct PTEST_CASE {
    const char *testname;		/* identifies test case */
    void    (*action) (PTEST_CTX *t, const struct PTEST_CASE *tp);
    const char *inputs[ARRAY_LEN];	/* input strings */
    int     flags;			/* see below */
    const char *want_panic_msg;		/* expected panic */
    ssize_t want_argc;			/* expected array length */
    const char *want_argv[ARRAY_LEN];	/* expected array content */
} PTEST_CASE;

#define ARGV_TEST_FLAG_TERMINATE	(1<<0)

/* verify_argv - verify result */

static void verify_argv(PTEST_CTX *t, const PTEST_CASE *tp, ARGV *argvp)
{
    int     idx;

    /*
     * We don't use eq_argv() here. We must at least once compare argv.c
     * output against data that was created without using any argv.c code.
     */
    if (argvp->argc != tp->want_argc)
	ptest_fatal(t, "got argc %ld, want %ld",
		    (long) argvp->argc, (long) tp->want_argc);
    if (argvp->argv[argvp->argc] != 0 && (tp->flags & ARGV_TEST_FLAG_TERMINATE))
	ptest_error(t, "got unterminated, want terminated");
    for (idx = 0; idx < argvp->argc; idx++) {
	if (strcmp(argvp->argv[idx], tp->want_argv[idx]) != 0) {
	    ptest_error(t, "index %d: got '%s', want '%s'",
			idx, argvp->argv[idx], tp->want_argv[idx]);
	}
    }
}

/* wrap_argv_free - in case (void *) != (struct *) */

static void wrap_argv_free(void *context)
{
    argv_free((ARGV *) context);
}

/* populate_argv - populate result, optionally terminate */

static ARGV *populate_argv(PTEST_CTX *t, const PTEST_CASE *tp)
{
    ARGV   *argvp = argv_alloc(1);
    const char *const * cpp;

    /*
     * Some tests use longjmp instead of returning normally, so we use
     * deferred execution to clean up allocated memory.
     */
    ptest_defer(t, wrap_argv_free, (void *) argvp);
    if (tp->want_panic_msg != 0)
	expect_ptest_log_event(t, tp->want_panic_msg);
    for (cpp = tp->inputs; *cpp; cpp++)
	argv_add(argvp, *cpp, (char *) 0);
    if (tp->flags & ARGV_TEST_FLAG_TERMINATE)
	argv_terminate(argvp);
    return (argvp);
}

/* test_argv_basic - populate and verify */

static void test_argv_basic(PTEST_CTX *t, const PTEST_CASE *tp)
{
    ARGV   *argvp = populate_argv(t, tp);

    verify_argv(t, tp, argvp);
}

/* test_argv_sort - populate and sort result */

static void test_argv_sort(PTEST_CTX *t, const PTEST_CASE *tp)
{
    ARGV   *argvp = populate_argv(t, tp);

    argv_qsort(argvp, (ARGV_COMPAR_FN) 0);
    verify_argv(t, tp, argvp);
}

/* test_argv_sort_uniq - populate, sort, uniq result */

static void test_argv_sort_uniq(PTEST_CTX *t, const PTEST_CASE *tp)
{

    /*
     * This also tests argv_delete().
     */
    ARGV   *argvp = populate_argv(t, tp);

    argv_qsort(argvp, (ARGV_COMPAR_FN) 0);
    argv_uniq(argvp, (ARGV_COMPAR_FN) 0);
    verify_argv(t, tp, argvp);
}

/* test_argv_good_truncate - populate and truncate to good size */

static void test_argv_good_truncate(PTEST_CTX *t, const PTEST_CASE *tp)
{
    ARGV   *argvp = populate_argv(t, tp);

    argv_truncate(argvp, tp->want_argc);
    verify_argv(t, tp, argvp);
}

/* test_argv_bad_truncate - populate and truncate to bad size */

static void test_argv_bad_truncate(PTEST_CTX *t, const PTEST_CASE *tp)
{
    ARGV   *argvp = populate_argv(t, tp);

    argv_truncate(argvp, -1);
    ptest_fatal(t, "argv_delete() returned");
}

/* test_argv_good_insert - populate and insert at good position */

static void test_argv_good_insert(PTEST_CTX *t, const PTEST_CASE *tp)
{
    ARGV   *argvp = populate_argv(t, tp);

    argv_insert_one(argvp, 1, "new");
    verify_argv(t, tp, argvp);
}

/* test_argv_bad_insert1 - populate and insert at bad position */

static void test_argv_bad_insert1(PTEST_CTX *t, const PTEST_CASE *tp)
{
    ARGV   *argvp = populate_argv(t, tp);

    argv_insert_one(argvp, -1, "new");
    ptest_fatal(t, "argv_delete() returned");
}

/* test_argv_bad_insert2 - populate and insert at bad position */

static void test_argv_bad_insert2(PTEST_CTX *t, const PTEST_CASE *tp)
{
    ARGV   *argvp = populate_argv(t, tp);

    argv_insert_one(argvp, 100, "new");
    ptest_fatal(t, "argv_delete() returned");
}

/* test_argv_good_replace - populate and replace at good position */

static void test_argv_good_replace(PTEST_CTX *t, const PTEST_CASE *tp)
{
    ARGV   *argvp = populate_argv(t, tp);

    argv_replace_one(argvp, 1, "new");
    verify_argv(t, tp, argvp);
}

/* test_argv_bad_replace1 - populate and replace at bad position */

static void test_argv_bad_replace1(PTEST_CTX *t, const PTEST_CASE *tp)
{
    ARGV   *argvp = populate_argv(t, tp);

    argv_replace_one(argvp, -1, "new");
    ptest_fatal(t, "argv_delete() returned");
}

/* test_argv_bad_replace2 - populate and replace at bad position */

static void test_argv_bad_replace2(PTEST_CTX *t, const PTEST_CASE *tp)
{
    ARGV   *argvp = populate_argv(t, tp);

    argv_replace_one(argvp, 100, "new");
    ptest_fatal(t, "argv_delete() returned");
}

/* test_argv_bad_delete1 - populate and delete at bad position */

static void test_argv_bad_delete1(PTEST_CTX *t, const PTEST_CASE *tp)
{
    ARGV   *argvp = populate_argv(t, tp);

    argv_delete(argvp, -1, 1);
    ptest_fatal(t, "argv_delete() returned");
}

/* test_argv_bad_delete2 - populate and delete at bad position */

static void test_argv_bad_delete2(PTEST_CTX *t, const PTEST_CASE *tp)
{
    ARGV   *argvp = populate_argv(t, tp);

    argv_delete(argvp, 0, -1);
    ptest_fatal(t, "argv_delete() returned");
}

/* test_argv_bad_delete3 - populate and delete at bad position */

static void test_argv_bad_delete3(PTEST_CTX *t, const PTEST_CASE *tp)
{
    ARGV   *argvp = populate_argv(t, tp);

    argv_delete(argvp, 100, 1);
    ptest_fatal(t, "argv_delete() returned");
}

 /*
  * The test cases. TODO: argv_addn with good and bad string length.
  */
static const PTEST_CASE ptestcases[] = {
    {"multiple strings, unterminated array", test_argv_basic,
	{"foo", "baz", "bar", 0}, 0,
	0, 3, {"foo", "baz", "bar", 0}
    },
    {"multiple strings, terminated array", test_argv_basic,
	{"foo", "baz", "bar", 0}, ARGV_TEST_FLAG_TERMINATE,
	0, 3, {"foo", "baz", "bar", 0}
    },
    {"distinct strings, sorted array", test_argv_sort,
	{"foo", "baz", "bar", 0}, 0,
	0, 3, {"bar", "baz", "foo", 0}
    },
    {"duplicate strings, sorted array", test_argv_sort,
	{"foo", "baz", "baz", "bar", 0}, 0,
	0, 4, {"bar", "baz", "baz", "foo", 0}
    },
    {"duplicate strings, sorted, uniqued-middle elements", test_argv_sort_uniq,
	{"foo", "baz", "baz", "bar", 0}, 0,
	0, 3, {"bar", "baz", "foo", 0}
    },
    {"duplicate strings, sorted, uniqued-first elements", test_argv_sort_uniq,
	{"foo", "bar", "baz", "bar", 0}, 0,
	0, 3, {"bar", "baz", "foo", 0}
    },
    {"duplicate strings, sorted, uniqued-last elements", test_argv_sort_uniq,
	{"foo", "foo", "baz", "bar", 0}, 0,
	0, 3, {"bar", "baz", "foo", 0}
    },
    {"multiple strings, truncate array by one", test_argv_good_truncate,
	{"foo", "baz", "bar", 0}, 0,
	0, 2, {"foo", "baz", 0}
    },
    {"multiple strings, truncate whole array", test_argv_good_truncate,
	{"foo", "baz", "bar", 0}, 0,
	0, 0, {"foo", "baz", 0}
    },
    {"multiple strings, bad truncate", test_argv_bad_truncate,
	{"foo", "baz", "bar", 0}, 0,
	"argv_truncate: bad length -1"
    },
    {"multiple strings, insert one at good position", test_argv_good_insert,
	{"foo", "baz", "bar", 0}, 0,
	0, 4, {"foo", "new", "baz", "bar", 0}
    },
    {"multiple strings, insert one at bad position", test_argv_bad_insert1,
	{"foo", "baz", "bar", 0}, 0,
	"argv_insert_one bad position: -1"
    },
    {"multiple strings, insert one at bad position", test_argv_bad_insert2,
	{"foo", "baz", "bar", 0}, 0,
	"argv_insert_one bad position: 100"
    },
    {"multiple strings, replace one at good position", test_argv_good_replace,
	{"foo", "baz", "bar", 0}, 0,
	0, 3, {"foo", "new", "bar", 0}
    },
    {"multiple strings, replace one at bad position", test_argv_bad_replace1,
	{"foo", "baz", "bar", 0}, 0,
	"argv_replace_one bad position: -1"
    },
    {"multiple strings, replace one at bad position", test_argv_bad_replace2,
	{"foo", "baz", "bar", 0}, 0,
	"argv_replace_one bad position: 100"
    },
    {"multiple strings, delete one at negative position", test_argv_bad_delete1,
	{"foo", "baz", "bar", 0}, 0,
	"argv_delete bad range: (start=-1 count=1)"
    },
    {"multiple strings, delete with bad range end", test_argv_bad_delete2,
	{"foo", "baz", "bar", 0}, 0,
	"argv_delete bad range: (start=0 count=-1)"
    },
    {"multiple strings, delete at too large position", test_argv_bad_delete3,
	{"foo", "baz", "bar", 0}, 0,
	"argv_delete bad range: (start=100 count=1)"
    },
};

#include <ptest_main.h>
