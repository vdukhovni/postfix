 /*
  * Test program to exercise mystrtok.c. See PTEST_README for documentation.
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
#include <stringops.h>

 /*
  * Test library.
  */
#include <ptest.h>

typedef struct PTEST_CASE {
    const char *testname;
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
} PTEST_CASE;

typedef struct TEST_DATA {
    const char *input;
    const char *const * want;
} TEST_DATA;

#define STR_OR_NULL(s)	((s) ? (s) : "null")
#define STR(x)		vstring_str(x)

static bool match_one(PTEST_CTX *t, const char *got, const char *want)
{
    bool    ret;

    ret = (got && want) ? (strcmp(got, want) == 0) : (got == want);
    if (!ret)
	ptest_error(t, "got '%s', want '%s'",
		    STR_OR_NULL(got), STR_OR_NULL(want));
    return (ret);
}

static void test_mystrtok(PTEST_CTX *t, const PTEST_CASE *unused)
{
    VSTRING *test_label = vstring_alloc(100);
    const TEST_DATA test_data[] = {
	{"", (const char *[]) {0},},
	{"  foo  ", (const char *[]) {"foo", 0}},
	{"  foo  bar  ", (const char *[]) {"foo", "bar", 0}},
    };
    const TEST_DATA *tp;

    for (tp = test_data; tp < test_data + PTEST_NROF(test_data); tp++) {
	vstring_sprintf(test_label, "(%s)", tp->input);
	PTEST_RUN(t, STR(test_label), {
	    char   *got;
	    char   *saved_input = mystrdup(tp->input);
	    char   *cp = saved_input;
	    int     n;

	    for (n = 0; /* See below */; n++) {
		got = mystrtok(&cp, CHARS_SPACE);
		if (!match_one(t, got, tp->want[n]) || got == 0)
		    break;
	    }
	    myfree(saved_input);
	});
    }
    vstring_free(test_label);
}

static void test_mystrtokq(PTEST_CTX *t, const PTEST_CASE *unused)
{
    VSTRING *test_label = vstring_alloc(100);
    const TEST_DATA test_data[] = {
	{"", (const char *[]) {0}},
	{"foo bar", (const char *[]) {"foo", "bar", 0}},
	{"{ bar }  ", (const char *[]) {"{ bar }", 0}},
	{"foo { bar } baz", (const char *[]) {"foo", "{ bar }", "baz", 0}},
	{"foo{ bar } baz", (const char *[]) {"foo{ bar }", "baz", 0}},
	{"foo { bar }baz", (const char *[]) {"foo", "{ bar }baz", 0}},
    };
    const TEST_DATA *tp;

    for (tp = test_data; tp < test_data + PTEST_NROF(test_data); tp++) {
	vstring_sprintf(test_label, "(%s)", tp->input);
	PTEST_RUN(t, STR(test_label), {
	    char   *got;
	    char   *saved_input = mystrdup(tp->input);
	    char   *cp = saved_input;
	    int     n;

	    for (n = 0; /* See below */; n++) {
		got = mystrtokq(&cp, CHARS_SPACE, CHARS_BRACE);
		if (!match_one(t, got, tp->want[n]) || got == 0)
		    break;
	    }
	    myfree(saved_input);
	});
    }
    vstring_free(test_label);
}

static void test_mystrtokdq(PTEST_CTX *t, const PTEST_CASE *unused)
{
    VSTRING *test_label = vstring_alloc(100);
    const TEST_DATA test_data[] = {
	{"", (const char *[]) {0, 0,}},
	{"  foo  ", (const char *[]) {"foo", 0,}},
	{"  foo  bar  ", (const char *[]) {"foo", "bar", 0,}},
	{"  foo\\ bar  ", (const char *[]) {"foo\\ bar", 0,}},
	{"  foo \\\" bar", (const char *[]) {"foo", "\\\"", "bar", 0,}},
	{"  foo \" bar baz\"  ", (const char *[]) {"foo", "\" bar baz\"", 0,}},
    };
    const TEST_DATA *tp;

    for (tp = test_data; tp < test_data + PTEST_NROF(test_data); tp++) {
	vstring_sprintf(test_label, "(%s)", tp->input);
	PTEST_RUN(t, STR(test_label), {
	    char   *got;
	    char   *saved_input = mystrdup(tp->input);
	    char   *cp = saved_input;
	    int     n;

	    for (n = 0; /* See below */; n++) {
		got = mystrtokdq(&cp, CHARS_SPACE);
		if (!match_one(t, got, tp->want[n]) || got == 0)
		    break;
	    }
	    myfree(saved_input);
	});
    }
    vstring_free(test_label);
}

static void test_mystrtok_cw(PTEST_CTX *t, const PTEST_CASE *unused)
{
    VSTRING *test_label = vstring_alloc(100);
    const TEST_DATA test_data[] = {
	{"#after text", (const char *[]) {0,}},
	{"before-text #after text", (const char *[]) {"before-text", 0,}},
    };
    const TEST_DATA *tp;

    for (tp = test_data; tp < test_data + PTEST_NROF(test_data); tp++) {
	vstring_sprintf(test_label, "(%s)", tp->input);
	PTEST_RUN(t, STR(test_label), {
	    char   *got;
	    char   *saved_input = mystrdup(tp->input);
	    char   *cp = saved_input;
	    int     n;

	    expect_ptest_log_event(t,
				"#comment after other text is not allowed");

	    for (n = 0; /* See below */; n++) {
		got = mystrtok_cw(&cp, CHARS_SPACE, "test");
		if (!match_one(t, got, tp->want[n]) || got == 0)
		    break;
	    }
	    myfree(saved_input);
	});
    }
    vstring_free(test_label);
}

static void test_mystrtokq_cw(PTEST_CTX *t, const PTEST_CASE *unused)
{
    VSTRING *test_label = vstring_alloc(100);
    const TEST_DATA test_data[] = {
	{"#after text", (const char *[]) {0,}},
	{"{ before text } #after text", (const char *[]) {"{ before text }", 0,}},
    };
    const TEST_DATA *tp;

    for (tp = test_data; tp < test_data + PTEST_NROF(test_data); tp++) {
	vstring_sprintf(test_label, "(%s)", tp->input);
	PTEST_RUN(t, STR(test_label), {
	    char   *got;
	    char   *saved_input = mystrdup(tp->input);
	    char   *cp = saved_input;
	    int     n;

	    expect_ptest_log_event(t,
				"#comment after other text is not allowed");

	    for (n = 0; /* See below */; n++) {
		got = mystrtokq_cw(&cp, CHARS_SPACE, CHARS_BRACE, "test");
		if (!match_one(t, got, tp->want[n]) || got == 0)
		    break;
	    }
	    myfree(saved_input);
	});
    }
    vstring_free(test_label);
}

static void test_mystrtokdq_cw(PTEST_CTX *t, const PTEST_CASE *unused)
{
    VSTRING *test_label = vstring_alloc(100);
    TEST_DATA test_data[] = {
	{"#after text", (const char *[]) {0,}},
	{"\"before text\" #after text", (const char *[]) {"\"before text\"", 0,}},
    };
    const TEST_DATA *tp;

    for (tp = test_data; tp < test_data + PTEST_NROF(test_data); tp++) {
	vstring_sprintf(test_label, "(%s)", tp->input);
	PTEST_RUN(t, STR(test_label), {
	    char   *got;
	    char   *saved_input = mystrdup(tp->input);
	    char   *cp = saved_input;
	    int     n;

	    expect_ptest_log_event(t,
				"#comment after other text is not allowed");

	    for (n = 0; /* See below */; n++) {
		got = mystrtokdq_cw(&cp, CHARS_SPACE, "test");
		if (!match_one(t, got, tp->want[n]) || got == 0)
		    break;
	    }
	    myfree(saved_input);
	});
    }
    vstring_free(test_label);
}

static const PTEST_CASE ptestcases[] = {
    {"test_mystrtok", test_mystrtok},
    {"test_mystrtokq", test_mystrtokq},
    {"test_mystrtokdq", test_mystrtokdq},
    {"test_mystrtok_cw", test_mystrtok_cw},
    {"test_mystrtokq_cw", test_mystrtokq_cw},
    {"test_mystrtokdq_cw", test_mystrtokdq_cw},
};

#include <ptest_main.h>
