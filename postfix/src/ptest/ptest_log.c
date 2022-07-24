/*++
/* NAME
/*	ptest_log 3
/* SUMMARY
/*	log event receiver support
/* SYNOPSIS
/*	#include <ptest.h>
/*
/*	void	expect_ptest_log_event(
/*	PTEST_CTX *t,
/*	const char *text)
/* INFRASTRUCTURE SUPPORT
/*	void	ptest_log_setup(
/*	PTEST_CTX *t)
/*
/*	void	ptest_log_wrapup(
/*	PTEST_CTX *t)
/* DESCRIPTION
/*	This module inspects msg(3) logging.
/*
/*	expect_ptest_log_event() is called from a test. It requires
/*	that an msg(3) call will be made whose formatted text
/*	contains a substring that matches the text argument. For
/*	robustness, do not include file name or line number
/*	information. If a match fails, then the log event receiver
/*	will call ptest_error() to report the unexpected msg(3)
/*	call. If the expected msg(3) call is not made, then
/*	ptest_log_wrapup() will call ptest_error() to report the
/*	missing call.
/*
/*	ptest_log_setup() is called by testing infrastructure before
/*	a test is started. It updates the PTEST_CTX structure, and
/*	installs an msg(3) log event receiver.
/*
/*	ptest_log_wrapup() is called by test infrastructure after
/*	a test terminates. It calls ptest_error() to report any
/*	unmatched expect_ptest_log_event() expectations, and destroys
/*	buffers that were created by ptest_log_setup().
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

 /*
  * Utility library.
  */
#include <msg.h>
#include <msg_output.h>
#include <vstring.h>

 /*
  * Test library.
  */
#include <ptest.h>

 /*
  * SLMs.
  */
#define STR(x)	vstring_str(x)

/* ptest_log_event - receive log event */

static void ptest_log_event(int level, const char *text, void *context)
{
    static const char *level_text[] = {
	"info", "warning", "error", "fatal", "panic",
    };
    PTEST_CTX *t = (PTEST_CTX *) context;
    char  **cpp;

    /*
     * Silence events for parent handlers.
     */
    if (t != ptest_ctx_current())
	return;

    /*
     * Sanity checks.
     */
    if (level < 0 || level >= (int) (sizeof(level_text) / sizeof(level_text[0])))
	msg_panic("ptest_log_event: invalid severity level: %d", level);

    /*
     * Format the text.
     */
    if (level == MSG_INFO) {
	vstring_sprintf(t->log_buf, "%s", text);
    } else {
	vstring_sprintf(t->log_buf, "%s: %s", level_text[level], text);
    }

    /*
     * Handle expected versus unexpected text.
     */
    for (cpp = t->allow_logs->argv; *cpp; cpp++) {
	if (strstr(STR(t->log_buf), *cpp) != 0) {
	    argv_delete(t->allow_logs, cpp - t->allow_logs->argv, 1);
	    return;
	}
    }
    ptest_error(t, "Unexpected log event: got '%s'", STR(t->log_buf));
}

/* ptest_log_setup - install logging receiver */

void    ptest_log_setup(PTEST_CTX *t)
{
    if (t != ptest_ctx_current())
	msg_panic("ptest_log_setup: not current context");
    t->log_buf = vstring_alloc(100);
    t->allow_logs = argv_alloc(1);
    msg_output_push(ptest_log_event, (void *) t);
}

/* expect_ptest_log_event - add log event expectation */

void    expect_ptest_log_event(PTEST_CTX *t, const char *text)
{
    if (t != ptest_ctx_current())
	msg_panic("expect_ptest_log_event: not current context");
    argv_add(t->allow_logs, text, (char *) 0);
}

/* ptest_log_wrapup - enforce logging expectations */

void    ptest_log_wrapup(PTEST_CTX *t)
{
    char  **cpp;

    msg_output_pop(ptest_log_event, (void *) t);
    for (cpp = t->allow_logs->argv; *cpp; cpp++)
	ptest_error(t, "Missing log event: want '%s'", *cpp);
    vstring_free(t->log_buf);
    t->log_buf = 0;
    argv_free(t->allow_logs);
    t->allow_logs = 0;
}
