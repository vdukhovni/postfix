 /*
  * This file contains two parts.
  * 
  * 1 - A trivial mock function, including code to set up expectations and to
  * respond to calls.
  * 
  * 2 - Test cases that exercise this mock function and the mock support
  * infrastructure.
  */

 /*
  * Part 1: This emulates a trivial function:
  * 
  * int foo(const char *arg_in, char **arg_out)
  * 
  * When the mock foo() function is called with an arg_in value that matches an
  * expected input (see below) then the mock foo() function stores a prepared
  * value through the arg_out argument, and returns a prepared function
  * result value.
  * 
  * The prepared response an result are set up with:
  * 
  * void expect_foo(const char *file, int line, int calls_expected, int retval,
  * const char *arg_in, const char *arg_out)
  * 
  * This saves deep copies of arg_in and arg_out, and the result value in
  * retval. The file name and line number are used to improve warning
  * messages; typically these are specified at the call site with __FILE__
  * and __LINE__.
  * 
  * The code below provides mock-specific helpers that match inputs against an
  * expectation and that output prepared responses. These are called by the
  * mock support infrastructure as needed.
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

 /*
  * Test library.
  */
#include <mymalloc.h>
#include <pmock_expect.h>
#include <ptest.h>

 /*
  * Deep copies of expected inputs and prepared outputs specified in an
  * 'expect_foo' call. This structure will also be used to capture shallow
  * copies of inputs for a 'foo' call.
  */
struct foo_expectation {
    MOCK_EXPECT mock_expect;		/* generic fields */
    char   *arg_in;			/* input arguments */
    int     retval;			/* result value */
    char   *arg_out;			/* output argument */
};

 /*
  * Pointers to the outputs for a 'foo' call.
  */
struct foo_targets {
    char  **arg_out;			/* output argument pointer */
    int    *retval;			/* result value pointer */
};

/* match_foo - match inputs against expectation */

static int match_foo(const MOCK_EXPECT *expect, const MOCK_EXPECT *inputs)
{
    struct foo_expectation *pe = (struct foo_expectation *) expect;
    struct foo_expectation *pi = (struct foo_expectation *) inputs;

    return (strcmp(pe->arg_in, pi->arg_in) == 0);
}

/* assign_foo - assign expected output */

static void assign_foo(const MOCK_EXPECT *expect, void *targets)
{
    struct foo_expectation *pe = (struct foo_expectation *) expect;
    struct foo_targets *pt = (struct foo_targets *) targets;

    *(pt->arg_out) = mystrdup(pe->arg_out);
    *(pt->retval) = pe->retval;
}

/* print_foo - print expected inputs */

static char *print_foo(const MOCK_EXPECT *expect, VSTRING *buf)
{
    struct foo_expectation *pe = (struct foo_expectation *) expect;

    vstring_sprintf(buf, "%s", pe->arg_in);
    return (vstring_str(buf));
}

/* free_foo - destructor */

static void free_foo(MOCK_EXPECT *expect)
{
    struct foo_expectation *pe = (struct foo_expectation *) expect;

    if (pe->arg_in)
	myfree(pe->arg_in);
    if (pe->arg_out)
	myfree(pe->arg_out);
    pmock_expect_free(expect);
}

 /*
  * The mock name and its helper callbacks in one place.
  */
static const MOCK_APPL_SIG foo_sig = {
    "foo",
    match_foo,
    assign_foo,
    print_foo,
    free_foo,
};

/* expect_foo - set up expectation */

static void expect_foo(const char *file, int line, int calls_expected,
		               int retval, const char *arg_in,
		               const char *arg_out)
{
    struct foo_expectation *pe;

    pe = (struct foo_expectation *)
	pmock_expect_create(&foo_sig, file, line, calls_expected, sizeof(*pe));
    pe->arg_in = mystrdup(arg_in);
    pe->retval = retval;
    pe->arg_out = mystrdup(arg_out);
}

/* foo - mock foo */

static int foo(const char *arg_in, char **arg_out)
{
    struct foo_expectation inputs;
    struct foo_targets targets;
    int     retval = -1;

    /*
     * Bundle the arguments to simplify handling.
     */
    inputs.arg_in = (char *) arg_in;
    targets.arg_out = arg_out;
    targets.retval = &retval;

    /*
     * TODO: bail out if there was no match?
     */
    (void) pmock_expect_apply(&foo_sig, &inputs.mock_expect, (void *) &targets);

    return (retval);
}

 /*
  * Part 2: Test cases. See ptest_main.h for a documented example.
  */

 /*
  * The ptestcase structure.
  */
typedef struct PTEST_CASE {
    const char *testname;		/* Human-readable description */
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
} PTEST_CASE;

static void test_unused_expectation_1_of_2(PTEST_CTX *t,
					           const PTEST_CASE *unused)
{
    const char *want_arg_out = "output";
    char   *got_arg_out = 0;
    int     got_retval, want_retval = 42;

    /*
     * Set up an expectation for two calls, but intentionally make only one.
     */
    expect_foo(__FILE__, __LINE__, 2, want_retval, "input", want_arg_out);
    got_retval = foo("input", &got_arg_out);
    if (got_arg_out == 0 || strcmp(got_arg_out, want_arg_out) != 0) {
	ptest_error(t, "foo: got '%s', want '%s'",
		    got_arg_out ? got_arg_out : "(null)", want_arg_out);
    } else if (got_retval != want_retval) {
	ptest_error(t, "foo: got retval %d, want %d", got_retval, want_retval);
    }

    /*
     * This error is intentional. Do not count as a failure. The error will
     * be logged after this test terminates.
     */
    expect_ptest_error(t, " got 1 call for foo(input), want 2");

    /*
     * Cleanup.
     */
    if (got_arg_out)
	myfree(got_arg_out);
}

static void test_unused_expectation_0_of_0_1(PTEST_CTX *t,
					           const PTEST_CASE *unused)
{
    int     want_retval = 42;

    /*
     * Give each expectation a unique line number. Here, we make zero calls
     * while expecting exactly one call, or one or more calls.
     */
    expect_foo(__FILE__, __LINE__, 1, want_retval, "input", "output");
    expect_foo(__FILE__, __LINE__, 0, want_retval, "input", "output");

    /*
     * These errors are intentional. Do not count as a failure.
     */
    expect_ptest_error(t, " got 0 calls for foo(input), want 1 or more");
    expect_ptest_error(t, " got 0 calls for foo(input), want 1");
}

 /*
  * Test cases. The "success" calls exercise the expectation match and apply
  * helpers, and "missing" tests exercise the print helpers. All tests
  * exercise the expectation free helpers.
  */
const PTEST_CASE ptestcases[] = {
    {
	"unused expectation 1 of 2", test_unused_expectation_1_of_2,
    },
    {
	"unused expectation 0 of 0-1", test_unused_expectation_0_of_0_1,
    },
};

#include <ptest_main.h>
