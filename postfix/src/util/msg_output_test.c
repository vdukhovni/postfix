 /*
  * Test program to exercise the msg_output module. See PTEST_README for
  * documentation.
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
#include <msg.h>
#include <msg_output.h>
#include <mymalloc.h>
#include <vstring.h>

 /*
  * Test library.
  */
#include <ptest.h>
#include <match_basic.h>

 /*
  * Note: the test framework calls msg_output_pop() after a test returns or
  * makes a long jump, so there is no requirement that each test below pops
  * the handlers that it has pushed.
  */
static ARGV *got_argv;
static VSTRING *buf;

typedef struct PTEST_CASE {
    const char *testname;		/* Human-readable description */
    void    (*action) (PTEST_CTX *t, const struct PTEST_CASE *);
} PTEST_CASE;

/* update_argv - append formatted handler inputs */

static void update_argv(ARGV *argv, int level, const char *text, void *context)
{
    if (buf == 0)
	buf = vstring_alloc(10);
    vstring_sprintf(buf, "%d:%s:%s", level, text, (char *) context);
    argv_add(argv, vstring_str(buf), (char *) 0);
}

/* handler - output handler */

static void handler(int level, const char *text, void *context)
{
    update_argv(got_argv, level, text, context);
}

static void test_msg_output_push_pop_works(PTEST_CTX *t, const PTEST_CASE *unused)
{
    ARGV   *want_argv = argv_alloc(1);
    char   *req_context = "handler";

    /*
     * Install our logging output handler.
     */
    got_argv = argv_alloc(1);
    msg_output_push(handler, req_context);

    /*
     * Expect and generate one logging event.
     */
    update_argv(want_argv, 0, "text", req_context);
    expect_ptest_log_event(t, "text");
    msg_info("text");

    /*
     * Verify that the event was sent to our logging handler.
     */
    if (got_argv->argc != 1)
	ptest_error(t, "handler: got %ld results, want 1",
		    (long) got_argv->argc);
    else
	(void) eq_argv(t, "handler events", got_argv, want_argv);

    /*
     * Pop our logging output handler and verify that it no longer receives
     * logging
     */
    msg_output_pop(handler, req_context);
    expect_ptest_log_event(t, "more text");
    msg_info("more text");
    if (got_argv->argc > 1)
	ptest_error(t, "handler: got result after it was popped");

    /*
     * Clean up.
     */
    argv_free(got_argv);
    argv_free(want_argv);
}

static void test_msg_output_push_dedups(PTEST_CTX *t, const PTEST_CASE *unused)
{
    ARGV   *want_argv = argv_alloc(1);
    char   *req_context = "handler";

    /*
     * Push the same logging handler twice.
     */
    got_argv = argv_alloc(1);
    msg_output_push(handler, req_context);
    msg_output_push(handler, req_context);

    /*
     * Expect and generate a logging event.
     */
    update_argv(want_argv, 0, "text", req_context);
    expect_ptest_log_event(t, "text");
    msg_info("text");

    /*
     * Verify the handler is called only once.
     */
    if (got_argv->argc != 1)
	ptest_error(t, "handler: got %ld results, want 1",
		    (long) got_argv->argc);
    else
	(void) eq_argv(t, "handler events", got_argv, want_argv);

    /*
     * Clean up.
     */
    msg_output_pop(handler, req_context);
    argv_free(got_argv);
    argv_free(want_argv);
}

static PTEST_CASE ptestcases[] = {
    {"test msg_output_push_pop works", test_msg_output_push_pop_works},
    {"test msg_output_push dedups", test_msg_output_push_dedups},
};

#include <ptest_main.h>
