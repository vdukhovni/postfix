 /*
  * Test program to exercise mystrtok.c. See ptest_main.h for a documented
  * example.
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

 /*
  * The following needs to be large enough to include a null terminator in
  * every ptestcase.want field.
  */
#define WANT_SIZE	5

typedef struct PTEST_CASE {
    const char *testname;
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
    const char *fname;
    const char *input;
    const char *want[WANT_SIZE];
} PTEST_CASE;

#define STR_OR_NULL(s)	((s) ? (s) : "null")

static void tester(PTEST_CTX *t, const struct PTEST_CASE *tp)
{
    char   *got;
    int     match;
    int     n;
    char   *saved_input = mystrdup(tp->input);
    char   *cp = saved_input;

    for (n = 0; n < WANT_SIZE; n++) {
	if (strcmp(tp->fname, "mystrtok") == 0) {
	    got = mystrtok(&cp, CHARS_SPACE);
	} else if (strcmp(tp->fname, "mystrtokq") == 0) {
	    got = mystrtokq(&cp, CHARS_SPACE, CHARS_BRACE);
	} else if (strcmp(tp->fname, "mystrtokdq") == 0) {
	    got = mystrtokdq(&cp, CHARS_SPACE);
	} else {
	    msg_panic("invalid function name: %s", tp->fname);
	}
	if ((match = (got && tp->want[n]) ?
	     (strcmp(got, tp->want[n]) == 0) :
	     (got == tp->want[n])) != 0) {
	    if (got == 0) {
		break;
	    }
	} else {
	    ptest_error(t, "got '%s', want '%s'",
			STR_OR_NULL(got), STR_OR_NULL(tp->want[n]));
	    break;
	}
    }
    if (n >= WANT_SIZE)
	msg_panic("need to increase WANT_SIZE");
    myfree(saved_input);
}

static const PTEST_CASE ptestcases[] = {
    {"mystrtok empty", tester,
    "mystrtok", ""},
    {"mystrtok >  foo  <", tester,
    "mystrtok", "  foo  ", {"foo"}},
    {"mystrtok >  foo  bar  <", tester,
    "mystrtok", "  foo  bar  ", {"foo", "bar"}},
    {"mystrtokq empty", tester,
    "mystrtokq", ""},
    {"mystrtokq >foo bar<", tester,
    "mystrtokq", "foo bar", {"foo", "bar"}},
    {"mystrtokq >{ bar }<", tester,
    "mystrtokq", "{ bar }  ", {"{ bar }"}},
    {"mystrtokq >foo { bar } baz<", tester,
    "mystrtokq", "foo { bar } baz", {"foo", "{ bar }", "baz"}},
    {"mystrtokq >foo{ bar } baz<", tester,
    "mystrtokq", "foo{ bar } baz", {"foo{ bar }", "baz"}},
    {"mystrtokq >foo { bar }baz<", tester,
    "mystrtokq", "foo { bar }baz", {"foo", "{ bar }baz"}},
    {"mystrtokdq empty", tester,
    "mystrtokdq", ""},
    {"mystrtokdq > foo  <", tester,
    "mystrtokdq", "  foo  ", {"foo"}},
    {"mystrtokdq >  foo  bar  <", tester,
    "mystrtokdq", "  foo  bar  ", {"foo", "bar"}},
    {"mystrtokdq >  foo\\ bar  <", tester,
    "mystrtokdq", "  foo\\ bar  ", {"foo\\ bar"}},
    {"mystrtokdq >  foo \\\" bar<", tester,
    "mystrtokdq", "  foo \\\" bar", {"foo", "\\\"", "bar"}},
    {"mystrtokdq > foo \" bar baz\"  <", tester,
    "mystrtokdq", "  foo \" bar baz\"  ", {"foo", "\" bar baz\""}},
};

#include <ptest_main.h>
