/*++
/* NAME
/*	ptest_ctx 3
/* SUMMARY
/*	test context support
/* SYNOPSIS
/*	#include <ptest.h>
/*
/*	PTEST_CTX ptest_ctx_create(
/*	const char *name,
/*	TEST_JMP_BUF *jbuf)
/*
/*	PTEST_CTX *ptest_ctx_current(void)
/*
/*	int	ptest_ctx_free(PTEST_CTX *t)
/* DESCRIPTION
/*	This module manages a stack of contexts that are used by
/*	tests.
/*
/*	ptest_ctx_create() is called by test infrastructure before
/*	a test is run. It returns an initialized PTEST_CTX object
/*	after making it the current test context. The jbuf argument
/*	references jump buffer that will be used by ptest_fatal(),
/*	msg_fatal() and msg_panic().
/*
/*	ptest_ctx_current() returns the current test context.  This
/*	function exists because mocked functions must be called
/*	without an argument that specifies a test context.
/*
/*	ptest_ctx_free() is called by test infrastructure after a
/*	test terminates and all error reporting has completed.
/*	It destroys the PTEST_CTX object.
/* DIAGNOSTICS
/*	ptest_ctx_current() will panic if the test context stack is
/*	empty.
/*
/*	ptest_ctx_free() will panic if the argument does not specify
/*	the current test context.
/* SEE ALSO
/*	pmock_expect(3), mock test support
/*	ptest_error(3), test error reporter
/*	ptest_log(3), log receiver
/*	ptest_main(3), test driver
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

 /*
  * Utility library.
  */
#include <msg.h>
#include <mymalloc.h>
#include <stringops.h>
#include <vstream.h>

 /*
  * Test library.
  */
#include <ptest.h>

static PTEST_CTX *ptest_ctx_head;

/* ptest_ctx_create - create initialized PTEST_CTX object */

PTEST_CTX *ptest_ctx_create(const char *name, TEST_JMP_BUF *jbuf)
{
    PTEST_CTX *parent = ptest_ctx_head;
    PTEST_CTX *t;

    t = mymalloc(sizeof(*t));
    if (name == 0)				/* main-level context */
	t->name = 0;
    else if (parent->name == 0)			/* top-level test context */
	t->name = mystrdup(name);
    else					/* sub test */
	t->name = concatenate(parent->name, "/", name, (char *) 0);
    t->jbuf = jbuf;
    t->parent = parent;
    t->flags = 0;
    /* ptest_run.c specific */
    t->sub_pass = t->sub_fail = t->sub_skip = 0;
    /* ptest_error.c specific */
    t->err_stream = 0;
    t->err_buf = 0;
    t->allow_errors = 0;
    /* ptest_log.c specific */
    t->log_buf = 0;
    t->allow_logs = 0;
    /* ptest_defer.c specific */
    t->defer_fn = 0;
    t->defer_ctx = 0;

    ptest_ctx_head = t;

    return (t);
}

/* ptest_ctx_current - return current context or die */

PTEST_CTX *ptest_ctx_current()
{
    if (ptest_ctx_head == 0)
	msg_panic("ptest_ctx_current: no test context");
    return (ptest_ctx_head);
}

/* ptest_ctx_free - destroy PTEST_CTX or die */

void    ptest_ctx_free(PTEST_CTX *t)
{
    if (t != ptest_ctx_head)
	msg_panic("ptest_ctx_free: wrong test context - "
		  "should you use ptest_return()?");
    ptest_ctx_head = t->parent;
    if (t->name)
	myfree(t->name);
    myfree((void *) t);
}
