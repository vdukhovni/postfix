/*++
/* NAME
/*	ptest_run 3h
/* SUMMARY
/*	test runner
/* SYNOPSIS
/*	#include <ptest.h>
/*
/*	void	PTEST_RUN(
/*	PTEST_CTX *t,
/*	const char *test_name,
/*	{ body_in_braces })
/*
/*	NORETURN ptest_skip(PTEST_CTX *t)
/*
/*	NORETURN ptest_return(PTEST_CTX *t)
/*
/*	void	ptest_defer(
/*	PTEST_CTX *t,
/*	void	(*defer_fn)(void *)
/*	void	*defer_ctx)
/* DESCRIPTION
/*	PTEST_RUN() is called from inside a test to run a subtest.
/*
/*	PTEST_RUN() is a macro that runs the { body_in_braces }
/*	with msg(3) logging temporarily redirected to a buffer, and
/*	with panic, fatal, error, and non-error functions that
/*	terminate a test without terminating the process.
/*
/*	To use this as a subtest inside a PTEST_CASE action:
/*
/* .na
/*	static void action(PTEST_CTX *t, const PTEST_CASE *tp)
/*	{
/*	    struct subtest {
/*	        // ...
/*	    };
/*	    static const struct subtest tests[] = {
/*	        // ...subtest data and expectations...
/*	    };
/*	    struct subtest *sp;
/*	    for (sp = tests; sp < tests + sizeof(tests) / sizeof(tests[0]); sp++) {
/*	        PTEST_RUN(t, sp->name, {
/*	            // Test code that uses sp->mumble here.
/*	            // Use ptest_error(), ptest_fatal(), or ptest_return()
/*	            // to report an error or terminate a test, or
/*	            // ptest_skip() to skip a test.
/*	        });
/*	    }
/*	}
/*
/*	ptest_skip() is called from inside a test. It flags a test
/*	as skipped, and terminates the test without terminating the
/*	process.
/*
/*	ptest_return() is called from inside a test. It terminates
/*	the test without terminating the process.
/*
/*	ptest_defer() may be called once from a test, to defer some
/*	processing until after the test completes. This is typically
/*	used to eliminate a resource leak in tests that terminate
/*	the test early (i.e. that return with a long jump).
/*
/*	To "undo" a ptest_defer() call, call the function with a
/*	null defer_fn argument. Then, it may be called again to
/*	set up deferred execution.
/* SEE ALSO
/*	pmock_expect(3), mock for hermetic tests
/*	ptest_error(3), test error support
/*	ptest_log(3), logging receiver support
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

 /*
  * Utility library.
  */
#include <msg.h>
#include <msg_vstream.h>
#include <vstream.h>

 /*
  * Test library.
  */
#include <pmock_expect.h>
#include <ptest.h>

/* ptest_run_prolog - encapsulate PTEST_RUN() dependencies */

void    ptest_run_prolog(PTEST_CTX *t)
{
    ptest_error_setup(t, VSTREAM_ERR);
    ptest_info(t, "RUN  %s", t->name);
    ptest_log_setup(t);
    msg_vstream_enable(0);
}

/* ptest_run_epilog - encapsulate PTEST_RUN() dependencies */

void    ptest_run_epilog(PTEST_CTX *t, PTEST_CTX *parent)
{
    msg_vstream_enable(1);
    ptest_log_wrapup(t);
    pmock_expect_wrapup(t);
    if (ptest_error_wrapup(t) != 0 || t->sub_fail != 0) {
	ptest_info(t, "FAIL %s", t->name);
	parent->sub_fail += 1;
    } else if (t->flags & PTEST_CTX_FLAG_SKIP) {
	ptest_info(t, "SKIP %s", t->name);
	parent->sub_skip += 1;
    } else {
	ptest_info(t, "PASS %s", t->name);
	parent->sub_pass += 1;
    }
    parent->sub_pass += t->sub_pass;
    parent->sub_fail += t->sub_fail;
    parent->sub_skip += t->sub_skip;
    if (t->defer_fn)
	t->defer_fn(t->defer_ctx);
}

/* ptest_skip - skip a test and return from test */

NORETURN ptest_skip(PTEST_CTX *t)
{
    t->flags |= PTEST_CTX_FLAG_SKIP;
    ptest_longjmp(t->jbuf, 1);
}

/* ptest_return - early return from test */

NORETURN ptest_return(PTEST_CTX *t)
{
    ptest_longjmp(t->jbuf, 1);
}

/* ptest_defer - post-test processing */

void    ptest_defer(PTEST_CTX *t, PTEST_DEFER_FN defer_fn,
		            void *defer_ctx)
{
    if (t->defer_fn && defer_fn)
	msg_panic("ptest_defer: multiple calls for this test context");
    t->defer_fn = defer_fn;
    t->defer_ctx = defer_ctx;
}
