 /*
  * Test program to exercise map_search.c. See ptest_main.h for a documented
  * example.
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
#include <stringops.h>
#include <vstring.h>

 /*
  * Global library.
  */
#include <map_search.h>

 /*
  * Test library.
  */
#include <ptest.h>

 /*
  * Test search actions.
  */
#define TEST_NAME_1	"one"
#define TEST_NAME_2	"two"
#define TEST_CODE_1	1
#define TEST_CODE_2	2

#define BAD_NAME	"bad"

#define STR(x)	vstring_str(x)

static const NAME_CODE search_actions[] = {
    TEST_NAME_1, TEST_CODE_1,
    TEST_NAME_2, TEST_CODE_2,
    0, MAP_SEARCH_CODE_UNKNOWN,
};

 /*
  * Test library adaptor.
  */
typedef struct PTEST_CASE {
    const char *testname;
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
} PTEST_CASE;

/* Helpers to simplify tests. */

static const char *string_or_null(const char *s)
{
    return (s ? s : "(null)");
}

static char *escape_order(VSTRING *buf, const char *search_order)
{
    return (STR(escape(buf, search_order, strlen(search_order))));
}

#define MAX_WANT_LOG	5

static void test_map_search(PTEST_CTX *t, const struct PTEST_CASE *unused)
{
    /* Test cases with inputs and expected outputs. */
    struct test {
	const char *map_spec;
	int     want_return;		/* 0=fail, 1=success */
	const char *want_log[MAX_WANT_LOG];
	const char *want_map_type_name;	/* 0 or match */
	const char *want_search_order;	/* 0 or match */
    };
    static struct test test_cases[] = {
	{				/* 0 */
	    .map_spec = "type",
	    .want_return = 0,
	    .want_log = {
		"malformed map specification: 'type'",
		"expected maptype:mapname instead of 'type'",
	    },
	},
	{				/* 1 */
	    .map_spec = "type:name",
	    .want_return = 1,
	    .want_map_type_name = "type:name",
	},
	{				/* 2 */
	    .map_spec = "{type:name}",
	    .want_return = 1,
	    .want_map_type_name = "type:name",
	},
	{				/* 3 */
	    .map_spec = "{type:name",
	    .want_return = 0,
	    .want_log = {
		"missing '}' in \"{type:name\""
	    },
	},				/* } */
	{				/* 4 */
	    .map_spec = "{type}",
	    .want_return = 0,
	    .want_log = {
		"malformed map specification: '{type}'",
		"expected maptype:mapname instead of 'type'",
	    },
	},
	{				/* 5 */
	    .map_spec = "{type:name foo}",
	    .want_return = 0,
	    .want_log = {
		"missing '=' after attribute name",
	    },
	},
	{				/* 6 */
	    .map_spec = "{type:name foo=bar}",
	    .want_return = 0,
	    .want_log = {
		"warning: unknown map attribute in '{type:name foo=bar}': 'foo'",
	    },
	},
	{				/* 7 */
	    .map_spec = "{type:name search_order=}",
	    .want_return = 1,
	    .want_map_type_name = "type:name",
	    .want_search_order = "",
	},
	{				/* 8 */
	    .map_spec = "{type:name search_order=one, two}",
	    .want_return = 0,
	    .want_log = {
		"missing '=' after attribute name'",
	    },
	},
	{				/* 9 */
	    .map_spec = "{type:name {search_order=one, two}}",
	    .want_return = 1,
	    .want_map_type_name = "type:name",
	    .want_search_order = "\01\02",
	},
	{				/* 10 */
	    .map_spec = "{type:name {search_order=one, two, bad}}",
	    .want_return = 0,
	    .want_log = {
		"'bad' in '{type:name {search_order=one, two, bad}}'",
	    },
	},
	{				/* 11 */
	    .map_spec = "{inline:{a=b} {search_order=one, two}}",
	    .want_return = 1,
	    .want_map_type_name = "inline:{a=b}",
	    .want_search_order = "\01\02",
	},
	{				/* 12 */
	    .map_spec = "{inline:{a=b, c=d} {search_order=one, two}}",
	    .want_return = 1,
	    .want_map_type_name = "inline:{a=b, c=d}",
	    .want_search_order = "\01\02",
	},
	{0},
    };
    struct test *tp;
    const char *const * cpp;

    /* Actual results. */
    const MAP_SEARCH *map_search_from_create;
    const MAP_SEARCH *map_search_from_create_2nd;
    const MAP_SEARCH *map_search_from_lookup;

    /* Scratch */
    VSTRING *want_escaped = vstring_alloc(100);
    VSTRING *got_escaped = vstring_alloc(100);

    /* Other */
    VSTRING *test_label = vstring_alloc(100);

    map_search_init(search_actions);

    for (tp = test_cases; tp->map_spec; tp++) {
	vstring_sprintf(test_label, "test %d", (int) (tp - test_cases));
	PTEST_RUN(t, STR(test_label), {
	    for (cpp = tp->want_log; cpp < tp->want_log + MAX_WANT_LOG && *cpp; cpp++)
		expect_ptest_log_event(t, *cpp);
	    map_search_from_create = map_search_create(tp->map_spec);
	    if (!tp->want_return != !map_search_from_create) {
		if (map_search_from_create)
		    ptest_fatal(t, "return: got {%s, %s}, want '%s'",
				map_search_from_create->map_type_name,
				escape_order(got_escaped,
				      map_search_from_create->search_order),
				tp->want_return ? "success" : "fail");
		else
		    ptest_fatal(t, "return: got '%s', want '%s'",
				map_search_from_create ? "success" : "fail",
				"success");
	    }
	    if (tp->want_return == 0)
		ptest_return(t);
	    map_search_from_lookup = map_search_lookup(tp->map_spec);
	    if (map_search_from_create != map_search_from_lookup) {
		ptest_error(t, "map_search_lookup: got %p, want %p",
			    map_search_from_lookup, map_search_from_create);
	    }
	    map_search_from_create_2nd = map_search_create(tp->map_spec);
	    if (map_search_from_create != map_search_from_create_2nd) {
		ptest_error(t, "repeated map_search_create: got %p want %p",
			map_search_from_create_2nd, map_search_from_create);
	    }
	    if (strcmp(string_or_null(tp->want_map_type_name),
		   string_or_null(map_search_from_create->map_type_name))) {
		ptest_error(t, "map_type_name: got '%s', want '%s'",
		      string_or_null(map_search_from_create->map_type_name),
			    string_or_null(tp->want_map_type_name));
	    }
	    if (strcmp(string_or_null(tp->want_search_order),
		    string_or_null(map_search_from_create->search_order))) {
		ptest_error(t, "search_order: got '%s', want '%s'",
			    escape_order(got_escaped,
		      string_or_null(map_search_from_create->search_order)),
			    escape_order(want_escaped,
				     string_or_null(tp->want_search_order)));
	    }
	});
    }
    vstring_free(want_escaped);
    vstring_free(got_escaped);
    vstring_free(test_label);
}

 /*
  * Test library adaptor.
  */
static const PTEST_CASE ptestcases[] = {
    "test_map_search", test_map_search,
};

#include <ptest_main.h>
