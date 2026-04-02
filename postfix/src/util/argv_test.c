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

#define MAX_ARR_LEN	(10)

 /*
  * All test data can fit in the same TEST_DATA structure.
  */
typedef struct PTEST_CASE {
    const char *testname;		/* identifies test case */
    void    (*action) (PTEST_CTX *t, const struct PTEST_CASE *tp);
} PTEST_CASE;

typedef struct TEST_DATA {
    const char *inputs[MAX_ARR_LEN];	/* input strings */
    int     flags;			/* see below */
    const char *want_panic_msg;		/* expected panic */
    ssize_t want_argc;			/* expected array length */
    const char *want_argv[MAX_ARR_LEN];	/* expected array content */
    int     join_delim;			/* argv_join() delimiter */
} TEST_DATA;

#define ARGV_TEST_FLAG_TERMINATE	(1<<0)

/* verify_argv - verify result */

static void verify_argv(PTEST_CTX *t, const TEST_DATA *tp, ARGV *argvp)
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

static ARGV *populate_argv(PTEST_CTX *t, const TEST_DATA *tp)
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

static void test_argv_basic(PTEST_CTX *t, const TEST_DATA *tp)
{
    ARGV   *argvp = populate_argv(t, tp);

    verify_argv(t, tp, argvp);
}

static void test_argv_multiple_unterminated(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "baz", "bar", 0},
	.want_argc = 3,
	.want_argv = {"foo", "baz", "bar", 0},
    };

    test_argv_basic(t, &test_data);
}

static void test_argv_multiple_terminated(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "baz", "bar", 0},
	.flags = ARGV_TEST_FLAG_TERMINATE,
	.want_argc = 3,
	.want_argv = {"foo", "baz", "bar", 0},
    };

    test_argv_basic(t, &test_data);
}

/* test_argv_sort - populate and sort result */

static void test_argv_sort(PTEST_CTX *t, const TEST_DATA *tp)
{
    ARGV   *argvp = populate_argv(t, tp);

    argv_qsort(argvp, (ARGV_COMPAR_FN) 0);
    verify_argv(t, tp, argvp);
}

/* test_argv_sort_distinct - populate and sort distinct strings */

static void test_argv_sort_distinct(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "baz", "bar", 0},
	.want_argc = 3,
	.want_argv = {"bar", "baz", "foo", 0},
    };

    test_argv_sort(t, &test_data);
}

/* test_argv_sort_duplicate - populate and sort duplicate strings	*/

static void test_argv_sort_duplicate(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "baz", "baz", "bar", 0},
	.want_argc = 4,
	.want_argv = {"bar", "baz", "baz", "foo", 0},
    };

    test_argv_sort(t, &test_data);
}

/* test_argv_sort_uniq - populate, sort, uniq result */

static void test_argv_sort_uniq(PTEST_CTX *t, const TEST_DATA *tp)
{

    /*
     * This also tests argv_delete().
     */
    ARGV   *argvp = populate_argv(t, tp);

    argv_qsort(argvp, (ARGV_COMPAR_FN) 0);
    argv_uniq(argvp, (ARGV_COMPAR_FN) 0);
    verify_argv(t, tp, argvp);
}

static void test_argv_sort_uniq_middle(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "baz", "baz", "bar", 0},
	.want_argc = 3,
	.want_argv = {"bar", "baz", "foo", 0},
    };

    test_argv_sort_uniq(t, &test_data);
}

static void test_argv_sort_uniq_first(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "bar", "baz", "bar", 0},
	.want_argc = 3,
	.want_argv = {"bar", "baz", "foo", 0},
    };

    test_argv_sort_uniq(t, &test_data);
}

static void test_argv_sort_uniq_last(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "foo", "baz", "bar", 0},
	.want_argc = 3,
	.want_argv = {"bar", "baz", "foo", 0},
    };

    test_argv_sort_uniq(t, &test_data);
}

/* test_argv_truncate - populate and truncate to good size */

static void test_argv_truncate(PTEST_CTX *t, const TEST_DATA *tp)
{
    ARGV   *argvp = populate_argv(t, tp);

    argv_truncate(argvp, tp->want_argc);
    verify_argv(t, tp, argvp);
}

/* test_argv_good_truncate_one - populate and truncate one */

static void test_argv_good_truncate_one(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "baz", "bar", 0},
	.want_argc = 2,
	.want_argv = {"foo", "baz", 0},
    };

    test_argv_truncate(t, &test_data);
}

/* test_argv_good_truncate_whole_array - populate and truncate all */

static void test_argv_good_truncate_all(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "baz", "bar", 0},
	.want_argc = 0,
    };

    test_argv_truncate(t, &test_data);
}

/* test_argv_bad_truncate - populate and truncate to bad size */

static void test_argv_bad_truncate(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "baz", "bar", 0},
	.want_panic_msg = "argv_truncate: bad length -1",
    };
    ARGV   *argvp = populate_argv(t, &test_data);

    argv_truncate(argvp, -1);
    ptest_fatal(t, "argv_delete() returned");
}

/* test_argv_good_insert - populate and insert at good position */

static void test_argv_good_insert(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "baz", "bar", 0},
	.want_argc = 4,
	.want_argv = {"foo", "new", "baz", "bar", 0},
    };
    ARGV   *argvp = populate_argv(t, &test_data);

    argv_insert_one(argvp, 1, "new");
    verify_argv(t, &test_data, argvp);
}

/* test_argv_bad_insert1 - populate and insert at bad position */

static void test_argv_bad_insert1(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "baz", "bar", 0},
	.want_panic_msg = "argv_insert_one bad position: -1",
    };
    ARGV   *argvp = populate_argv(t, &test_data);

    argv_insert_one(argvp, -1, "new");
    ptest_fatal(t, "argv_delete() returned");
}

/* test_argv_bad_insert2 - populate and insert at bad position */

static void test_argv_bad_insert2(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "baz", "bar", 0},
	.want_panic_msg = "argv_insert_one bad position: 100",
    };
    ARGV   *argvp = populate_argv(t, &test_data);

    argv_insert_one(argvp, 100, "new");
    ptest_fatal(t, "argv_delete() returned");
}

/* test_argv_good_replace - populate and replace at good position */

static void test_argv_good_replace(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "baz", "bar", 0},
	.want_argc = 3,
	.want_argv = {"foo", "new", "bar", 0},
    };
    ARGV   *argvp = populate_argv(t, &test_data);

    argv_replace_one(argvp, 1, "new");
    verify_argv(t, &test_data, argvp);
}

/* test_argv_bad_replace1 - populate and replace at bad position */

static void test_argv_bad_replace1(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "baz", "bar", 0},
	.want_panic_msg = "argv_replace_one bad position: -1",
    };
    ARGV   *argvp = populate_argv(t, &test_data);

    argv_replace_one(argvp, -1, "new");
    ptest_fatal(t, "argv_delete() returned");
}

/* test_argv_bad_replace2 - populate and replace at bad position */

static void test_argv_bad_replace2(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "baz", "bar", 0},
	.want_panic_msg = "argv_replace_one bad position: 100",
    };
    ARGV   *argvp = populate_argv(t, &test_data);

    argv_replace_one(argvp, 100, "new");
    ptest_fatal(t, "argv_delete() returned");
}

/* test_argv_bad_delete1 - populate and delete at bad position */

static void test_argv_bad_delete1(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "baz", "bar", 0},
	.want_panic_msg = "argv_delete bad range: (start=-1 count=1)",
    };
    ARGV   *argvp = populate_argv(t, &test_data);

    argv_delete(argvp, -1, 1);
    ptest_fatal(t, "argv_delete() returned");
}

/* test_argv_bad_delete2 - populate and delete at bad position */

static void test_argv_bad_delete2(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "baz", "bar", 0},
	.want_panic_msg = "argv_delete bad range: (start=0 count=-1)",
    };
    ARGV   *argvp = populate_argv(t, &test_data);

    argv_delete(argvp, 0, -1);
    ptest_fatal(t, "argv_delete() returned");
}

/* test_argv_bad_delete3 - populate and delete at bad position */

static void test_argv_bad_delete3(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "baz", "bar", 0},
	.want_panic_msg = "argv_delete bad range: (start=100 count=1)",
    };
    ARGV   *argvp = populate_argv(t, &test_data);

    argv_delete(argvp, 100, 1);
    ptest_fatal(t, "argv_delete() returned");
}

/* test_argv_join - populate, join, and overwrite */

static void test_argv_join(PTEST_CTX *t, const TEST_DATA *tp)
{
    ARGV   *argvp = populate_argv(t, tp);
    VSTRING *buf = vstring_alloc(100);

    /*
     * Impedance mismatch: argv_join() produces output to VSTRING, but the
     * test fixture wants output to ARGV.
     */
    argv_join(buf, argvp, tp->join_delim);
    argv_delete(argvp, 0, argvp->argc);
    argv_add(argvp, vstring_str(buf), ARGV_END);
    vstring_free(buf);
    verify_argv(t, tp, argvp);
}

/* test_argv_join_multiple - join multiple strings */

static void test_argv_join_multiple(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "baz", "bar", 0},
	.want_argc = 1,
	.want_argv = {"foo:baz:bar", 0},
	.join_delim = ':',
    };

    test_argv_join(t, &test_data);
}

/* test_argv_join_single - handle single-string case */

static void test_argv_join_single(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", 0},
	.want_argc = 1,
	.want_argv = {"foo", 0},
	.join_delim = ':',
    };

    test_argv_join(t, &test_data);
}

/* test_argv_join_empty - handle empty-string case */

static void test_argv_join_empty(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {0},
	.want_argc = 1,
	.want_argv = {"", 0},
	.join_delim = ':',
    };

    test_argv_join(t, &test_data);
}

/* test_argv_addv_appends - populate result */

static void test_argv_addv_appends(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "baz", "bar", 0},
	.want_argc = 3,
	.want_argv = {"foo", "baz", "bar", 0},
    };
    const TEST_DATA *tp = &test_data;
    ARGV   *argvp = argv_alloc(1);

    argvp = argv_addv(argvp, tp->inputs);
    ptest_defer(t, wrap_argv_free, (void *) argvp);
    verify_argv(t, tp, argvp);
}

/* test_argv_addv_creates_appends - populate result */

static void test_argv_addv_creates(PTEST_CTX *t, const PTEST_CASE *unused)
{
    static const TEST_DATA test_data = {
	.inputs = {"foo", "baz", "bar", 0},
	.want_argc = 3,
	.want_argv = {"foo", "baz", "bar", 0},
    };
    const TEST_DATA *tp = &test_data;
    ARGV   *argvp;

    argvp = argv_addv((ARGV *) 0, tp->inputs);
    ptest_defer(t, wrap_argv_free, (void *) argvp);
    verify_argv(t, tp, argvp);
}

 /*
  * The test cases. TODO: argv_addn with good and bad string length.
  */
static const PTEST_CASE ptestcases[] = {
    {.testname = "multiple strings, unterminated array",
	.action = test_argv_multiple_unterminated,
    },
    {.testname = "multiple strings, terminated array",
	.action = test_argv_multiple_terminated,
    },
    {.testname = "distinct strings, sorted array",
	.action = test_argv_sort_distinct,
    },
    {.testname = "duplicate strings, sorted array",
	.action = test_argv_sort_duplicate,
    },
    {.testname = "duplicate strings, sorted, uniqued-middle elements",
	.action = test_argv_sort_uniq_middle,
    },
    {.testname = "duplicate strings, sorted, uniqued-first elements",
	.action = test_argv_sort_uniq_first,
    },
    {.testname = "duplicate strings, sorted, uniqued-last elements",
	.action = test_argv_sort_uniq_last,
    },
    {.testname = "multiple strings, truncate array by one",
	.action = test_argv_good_truncate_one,
    },
    {.testname = "multiple strings, truncate whole array",
	.action = test_argv_good_truncate_all,
    },
    {.testname = "multiple strings, bad truncate",
	.action = test_argv_bad_truncate,
    },
    {.testname = "multiple strings, insert one at good position",
	.action = test_argv_good_insert,
    },
    {.testname = "multiple strings, insert one at bad position",
	.action = test_argv_bad_insert1,
    },
    {.testname = "multiple strings, insert one at bad position",
	.action = test_argv_bad_insert2,
    },
    {.testname = "multiple strings, replace one at good position",
	.action = test_argv_good_replace,
    },
    {.testname = "multiple strings, replace one at bad position",
	.action = test_argv_bad_replace1,
    },
    {.testname = "multiple strings, replace one at bad position",
	.action = test_argv_bad_replace2,
    },
    {.testname = "multiple strings, delete one at negative position",
	.action = test_argv_bad_delete1,
    },
    {.testname = "multiple strings, delete with bad range end",
	.action = test_argv_bad_delete2,
    },
    {.testname = "multiple strings, delete at too large position",
	.action = test_argv_bad_delete3,
    },
    {.testname = "argv_join, multiple strings",
	.action = test_argv_join_multiple,
    },
    {.testname = "argv_join, one string",
	.action = test_argv_join_single,
    },
    {.testname = "argv_join, empty",
	.action = test_argv_join_empty,
    },
    {.testname = "argv_addv appends to ARGV",
	.action = test_argv_addv_appends,
    },
    {.testname = "argv_addv creates ARGV",
	.action = test_argv_addv_creates,
    },
};

#include <ptest_main.h>
