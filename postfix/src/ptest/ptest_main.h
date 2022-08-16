/*++
/* NAME
/*	ptest_main 3h
/* SUMMARY
/*	test driver
/* DESCRIPTION
/*	This file should be included at the end of a *_test.c file.
/*	It contains a main program and test driver, and supports
/*	programs whether or not they use mocks as defined in
/*	<pmock_expect.h>.
/*
/*	Before including this file, a *_test.c file should define
/*	the structure and content of its test cases, and the functions
/*	that implement those tests:
/*
/* .nf
/*	/* Begin example. */
/*
/*	#include <ptest.h>
/*	#include <pmock_expect.h>
/*
/*	 /*
/*	  * Test case structure. If multiple test functions cannot
/*	  * have their test data in a shared PTEST_CASE structure, then
/*	  * each test function can define its own test data, and run
/*	  * multiple tests with PTEST_RUN(). See documentation in
/*	  * ptest_run.c.
/*	  */
/*	typedef struct PTEST_CASE {
/*	    const char *testname;       /* Human-readable description */
/*	    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
/*	    /* Optionally, your test data fields... */
/*	} PTEST_CASE;
/*
/*	 /*
/*	  * Test functions. These should not use msg_xxx() functions.
/*	  *
/*	  * To report a test error use ptest_error(t, ...) and to abort a
/*	  * test use ptest_fatal(t, ...). Tests with errors will not PASS.
/*	  *
/*	  * To "expect" a non-fatal error (and not count it as a failure)
/*	  * use expect_ptest_error(t, text) where the text is a substring
/*	  * of the expected error message.
/*	  */
/*
/*	static void test_abc(PTEST_CTX *t, const PTEST_CASE *tp)
/*	{
/*	    int  ret;
/*
/*	    want_abc = 42;
/*	    got_abc = abc(1, 2, 3);
/*	    if (got_abc != want_abc)
/*	        ptest_error(t, "abc: got %d, want %d", got_abc, want_abc);
/*	}
/*
/*	/* More test functions... */
/*
/*	/* Test cases. Do not terminate with null. */
/*
/*	const PTEST_CASE ptestcases[] = {
/*	    { "test abc", test_abc, ...},
/*	    /* More test cases... */
/*	};
/*
/*	#include <ptest_main.h>
/*
/*	/* End example. */
/* .fi
/*
/*	The <ptest_main.h> test driver iterates over each test case
/*	and invokes the test case's action function with a pointer
/*	to its test case. The test driver captures all logging that
/*	is generated while the test case runs, including logging
/*	from test functions, library functions and from the mock
/*	infrastructure.
/*
/*	The action function should call ptest_error() or ptest_fatal()
/*	when a test fails. ptest_fatal() terminates a test but does
/*	not terminate the process.
/*
/*	If the tests use mocks, the mock infrastructure will log
/*	unexpected mock calls, and unused mock call expectations.
/*
/*	The ptest_log module will log a warning if the captured
/*	logging differs from the expected logging.
/* SEE ALSO
/*	msg(3) diagnostics
/*	ptest_error(3) test error and non-error handling
/*	ptest_log(3) log event receiver support
/* BUGS
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	Google, Inc.
/*	111 8th Avenue
/*	New York, NY 10011, USA
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>
#include <string.h>
#include <stdlib.h>

 /*
  * Utility library.
  */
#include <msg.h>
#include <msg_output.h>
#include <msg_vstream.h>
#include <stringops.h>
#include <vstream.h>

 /*
  * Test library.
  */
#include <pmock_expect.h>
#include <ptest.h>

/* main - test driver */

int     main(int argc, char **argv)
{
    PTEST_CTX *t;
    const PTEST_CASE *tp;
    int     fail;

    /*
     * This must be set BEFORE the first hash table call.
     */
#ifndef DORANDOMIZE
    if (putenv("NORANDOMIZE=") != 0)
	msg_fatal("putenv() failed: %m");
#endif

    /*
     * Send msg(3) logging to stderr by default.
     */
    msg_vstream_init(basename(argv[0]), VSTREAM_ERR);

    /*
     * The main-level PTEST_CTX context has no name and no long jump context.
     * It's sole purpose is to run tests and to aggregate pass/skip/fail counts.
     */
    t = ptest_ctx_create((char *) 0, (MSG_JMP_BUF *) 0);

    /*
     * Run each test in its own PTEST_CTX context with its own log
     * interceptor and long jump context. Each test can invoke PTEST_RUN() to
     * run one or more of subtests in their own context with their own test
     * data, instead of having to store all test data in a PTEST_CASE
     * structure.
     */
    for (tp = ptestcases; tp < ptestcases + PTEST_NROF(ptestcases); tp++) {
	if (tp->testname == 0)
	    msg_fatal("Null testname in ptestcases array!");
	PTEST_RUN(t, tp->testname, {
	    tp->action(t, tp);
	});
    }
    msg_info("PASS: %d, SKIP: %d, FAIL: %d", t->sub_pass, t->sub_skip, fail = t->sub_fail);
    ptest_ctx_free(t);
    exit(fail > 0);
}
