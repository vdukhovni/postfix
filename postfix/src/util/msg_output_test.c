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
  * Note: PTEST_RUN() calls msg_output_push() and msg_output_pop() before and
  * after running a test, so that the tests below don't need to pop the
  * handlers that they have pushed.
  */

typedef struct PTEST_CASE {
    const char *testname;		/* Human-readable description */
    void    (*action) (PTEST_CTX *t, const struct PTEST_CASE *);
} PTEST_CASE;

 /*
  * Handler storage.
  */
static ARGV *got_argv;
static VSTRING *buf;

/* add_handler_event - append formatted handler inputs */

static void add_handler_event(ARGV *argv, int level, const char *text,
			              char *context)
{
    if (buf == 0)
	buf = vstring_alloc(10);
    vstring_sprintf(buf, "%d:%s:%s", level, text, context);
    argv_add(argv, vstring_str(buf), (char *) 0);
}

/* handler - output handler */

static void handler(int level, const char *text, void *context)
{
    add_handler_event(got_argv, level, text, context);
}

static void test_msg_output_push_pop_works(PTEST_CTX *t,
					           const PTEST_CASE *unused)
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
    add_handler_event(want_argv, 0, "text", req_context);
    expect_ptest_log_event(t, "text");
    msg_info("text");

    /*
     * Verify that the event was sent to our logging handler.
     */
    (void) eq_argv(t, "handler events", got_argv, want_argv);

    /*
     * Pop our logging output handler and verify that it no longer receives
     * logging events.
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
    add_handler_event(want_argv, 0, "text", req_context);
    expect_ptest_log_event(t, "text");
    msg_info("text");

    /*
     * Verify the handler is called only once.
     */
    (void) eq_argv(t, "handler events", got_argv, want_argv);

    /*
     * Clean up.
     */
    msg_output_pop(handler, req_context);
    argv_free(got_argv);
    argv_free(want_argv);
}

static void test_msg_output_pop_rejects_unregistered(PTEST_CTX *t,
					           const PTEST_CASE *unused)
{
    char   *req_context = "handler";

    /*
     * Install our logging output handler.
     */
    got_argv = argv_alloc(1);
    msg_output_push(handler, req_context);

    /*
     * Clean up.
     */
    msg_output_pop(handler, req_context);
    argv_free(got_argv);

    /*
     * Force an msg_output_pop() panic.
     */
    expect_ptest_log_event(t, "panic: msg_output_pop: handler");
    msg_output_pop(handler, req_context);
}

static PTEST_CASE ptestcases[] = {
    {"test msg_output_push_pop works", test_msg_output_push_pop_works},
    {"test msg_output_push dedups", test_msg_output_push_dedups},
    {"test msg_output_pop rejects unregistered",
    test_msg_output_pop_rejects_unregistered},
};

#include <ptest_main.h>
