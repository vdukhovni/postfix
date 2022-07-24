/*++
/* NAME
/*	ptest_error 3
/* SUMMARY
/*	test error and non-error support
/* SYNOPSIS
/*	#include <ptest.h>
/*
/*	void	expect_ptest_error(
/*	PTEST_CTX *t,
/*	const char *text)
/*
/*	void	PRINTFLIKE(2, 3) ptest_info(
/*	PTEST_CTX *t,
/*	const char *, ...)
/*
/*	void	PRINTFLIKE(2, 3) ptest_error(
/*	PTEST_CTX *t,
/*	const char *, ...)
/*
/*	NORETURN PRINTFLIKE(2, 3) ptest_fatal(
/*	PTEST_CTX *t,
/*	const char *, ...)
/* TEST INFRASTRUCTURE SUPPORT
/*	PTEST_CTX ptest_error_setup(VSTREAM *err_stream)
/*
/*	int	ptest_error_wrapup(PTEST_CTX *t)
/* DESCRIPTION
/*	This module manages errors and non-errors that are reported
/*	by tests.
/*
/*	ptest_info() is called from inside a test, to report a
/*	non-error condition, for example, to report progress.
/*
/*	ptest_error() is called from inside a test, to report a
/*	non-fatal test error (after the call is finished, the test
/*	will continue). If the error text matches a pattern given
/*	to an earlier expect_ptest_error() call (see below), then
/*	this ptest_error() call will be ignored once, and treated
/*	as a non-error.  Otherwise, ptest_error() logs the error and
/*	increments an error count.
/*
/*	expect_ptest_error() is called from inside a test. It requires
/*	that a ptest_error() call will be made whose formatted text
/*	contains a substring that matches the text argument. For
/*	robustness, do not include file line number information in
/*	the expected text. If the expected ptest_error() call is
/*	made, then that call will be ignored once, and treated as
/*	a non-error (call expect_ptest_error() multiple times to
/*	ignore an error multiple times). If the expected ptest_error()
/*	call is not made, then ptest_error_wrapup() will report an
/*	error and the test will fail.
/*
/*	ptest_fatal() is called from inside a test. It reports a
/*	fatal test error and increments an error count. A ptest_fatal()
/*	call does not return, instead it terminates the test.
/*	ptest_fatal() calls cannot be expected and ignored with
/*	expect_ptest_error().
/*
/*	ptest_error_setup() is called by test infrastructure before
/*	a test is run. It updates a PTEST_CTX object. The err_stream
/*	argument specifies the output stream for error reporting.
/*
/*	ptest_error_wrapup() is called by test infrastructure after
/*	a test terminates. It calls ptest_error() to report any
/*	missing ptest_error() calls, destroys the PTEST_CTX information
/*	that was allocated with ptest_error_setup(), and returns the
/*	final error count.
/* DIAGNOSTICS
/*	The above functions write to the VSTREAM specified in the
/*	ptest_error_setup() call.
/* SEE ALSO
/*	pmock_expect(3), mock test support
/*	ptest_main(3h), test driver
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
#include <setjmp.h>
#include <string.h>

 /*
  * Utility library.
  */
#include <argv.h>
#include <msg.h>
#include <mymalloc.h>
#include <vstream.h>
#include <vstring.h>

 /*
  * Test library.
  */
#include <ptest.h>

 /*
  * SLMs.
  */
#define STR(x)	vstring_str(x)

/* ptest_error_setup - populate PTEST_CTX object */

void    ptest_error_setup(PTEST_CTX *t, VSTREAM *err_stream)
{
    t->flags &= ~PTEST_CTX_FLAG_FAIL;
    t->err_stream = err_stream;
    t->err_buf = vstring_alloc(100);
    t->allow_errors = argv_alloc(1);
}

/* expect_ptest_error - require and bless a non-fatal error */

void    expect_ptest_error(PTEST_CTX *t, const char *text)
{
    argv_add(t->allow_errors, text, (char *) 0);
}

/* ptest_info - report non-error condition */

void    ptest_info(PTEST_CTX *t, const char *fmt,...)
{
    va_list ap;

    /*
     * Format the message.
     */
    va_start(ap, fmt);
    vstream_vfprintf(t->err_stream, fmt, ap);
    vstream_fprintf(t->err_stream, "\n");
    va_end(ap);
    vstream_fflush(t->err_stream);
}

/* ptest_error - report non-fatal error */

void    ptest_error(PTEST_CTX *t, const char *fmt,...)
{
    va_list ap;
    char  **cpp;

    /*
     * Format the message.
     */
    va_start(ap, fmt);
    vstring_vsprintf(t->err_buf, fmt, ap);
    va_end(ap);

    /*
     * Skip this error if it was expected.
     */
    for (cpp = t->allow_errors->argv; *cpp; cpp++) {
	if (strstr(STR(t->err_buf), *cpp) != 0) {
	    argv_delete(t->allow_errors, cpp - t->allow_errors->argv, 1);
	    return;
	}
    }

    /*
     * Report the message.
     */
    vstream_fprintf(t->err_stream, "error: %s\n", STR(t->err_buf));
    vstream_fflush(t->err_stream);
    t->flags |= PTEST_CTX_FLAG_FAIL;
}

/* ptest_fatal - report fatal error */

NORETURN ptest_fatal(PTEST_CTX *t, const char *fmt,...)
{
    va_list ap;

    /*
     * This has no code in common with ptest_error().
     */
    vstream_fprintf(t->err_stream, "fatal: ");
    va_start(ap, fmt);
    vstream_vfprintf(t->err_stream, fmt, ap);
    va_end(ap);
    vstream_fprintf(t->err_stream, "\n");
    vstream_fflush(t->err_stream);
    t->flags |= PTEST_CTX_FLAG_FAIL;
    ptest_longjmp(t->jbuf, 1);
}

/* ptest_error_wrapup - enforce error expectations and clean up */

extern int ptest_error_wrapup(PTEST_CTX *t)
{
    char  **cpp;
    int     fail_flag;

    /*
     * Report a new error if an expected error did not happen.
     */
    for (cpp = t->allow_errors->argv; *cpp; cpp++) {
	vstream_fprintf(t->err_stream, "Missing error: want '%s'\n", *cpp);
	t->flags |= PTEST_CTX_FLAG_FAIL;
	vstream_fflush(t->err_stream);
    }
    fail_flag = (t->flags & PTEST_CTX_FLAG_FAIL);

    /*
     * Clean up the PTEST_CTX fields that we created.
     */
    vstring_free(t->err_buf);
    t->err_buf = 0;
    argv_free(t->allow_errors);
    t->flags &= ~PTEST_CTX_FLAG_FAIL;
    return (fail_flag);
}
