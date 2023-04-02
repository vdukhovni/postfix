/*++
/* NAME
/*	argv_splitq 3
/* SUMMARY
/*	string array utilities
/* SYNOPSIS
/*	#include <argv.h>
/*
/*	ARGV	*argv_splitq(string, delim, parens)
/*	const char *string;
/*	const char *delim;
/*	const char *parens;
/*
/*	ARGV	*argv_splitq_cw(string, delim, parens, blame)
/*	const char *string;
/*	const char *delim;
/*	const char *parens;
/*	const char *blame;
/*
/*	ARGV	*argv_splitq_append(argv, string, delim, parens)
/*	ARGV	*argv;
/*	const char *string;
/*	const char *delim;
/*	const char *parens;
/* DESCRIPTION
/*	argv_splitq() breaks up \fIstring\fR into tokens according
/*	to the delimiters specified in \fIdelim\fR, while avoiding
/*	splitting text between matching parentheses. The result is
/*	a null-terminated string array.
/*
/*	argv_splitq_cw() is like argv_splitq() but stops splitting
/*	input and logs a warning when it encounters text that looks
/*	like a trailing comment. The \fBblame\fR argument specifies
/*	context that is used in warning messages. Specify a null
/*	pointer to disable the trailing comment check.
/*
/*	argv_splitq_append() performs the same operation as argv_splitq(),
/*	but appends the result to an existing string array.
/* SEE ALSO
/*	mystrtokq(), safe string splitter.
/* DIAGNOSTICS
/*	Fatal errors: memory allocation problem.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*
/*	Wietse Venema
/*--*/

/* System libraries. */

#include <sys_defs.h>
#include <string.h>

/* Application-specific. */

#include "mymalloc.h"
#include "stringops.h"
#include "argv.h"
#include "msg.h"

/* argv_splitq - ABI compatibility wrapper */

#undef argv_splitq

ARGV   *argv_splitq(const char *string, const char *delim, const char *parens)
{
    return (argv_splitq_cw(string, delim, parens, (char *) 0));
}

/* argv_splitq_cw - split string into token array */

ARGV   *argv_splitq_cw(const char *string, const char *delim,
		               const char *parens, const char *blame)
{
    ARGV   *argvp = argv_alloc(1);
    char   *saved_string = mystrdup(string);
    char   *bp = saved_string;
    char   *arg;

    while ((arg = mystrtokq_cw(&bp, delim, parens, blame)) != 0)
	argv_add(argvp, arg, (char *) 0);
    argv_terminate(argvp);
    myfree(saved_string);
    return (argvp);
}

/* argv_splitq_append - split string into token array, append to array */

ARGV   *argv_splitq_append(ARGV *argvp, const char *string, const char *delim,
			           const char *parens)
{
    char   *saved_string = mystrdup(string);
    char   *bp = saved_string;
    char   *arg;

    while ((arg = mystrtokq(&bp, delim, parens)) != 0)
	argv_add(argvp, arg, (char *) 0);
    argv_terminate(argvp);
    myfree(saved_string);
    return (argvp);
}

#ifdef TEST

 /*
  * System library.
  */
#include <setjmp.h>
#include <stdlib.h>

 /*
  * Utility library.
  */
#include <msg_vstream.h>
#include <stringops.h>

#define ARRAY_LEN       (10)

typedef struct TEST_CASE {
    const char *label;			/* identifies test case */
    const char *in_array[ARRAY_LEN];	/* input array */
    const char *in_string;		/* input string */
    const char *delim;			/* field delimiters */
    const char *parens;			/* grouping */
    const char *blame;			/* blame or null */
    ARGV   *(*populate_fn) (const struct TEST_CASE *, ARGV *);
    int     exp_argc;			/* expected array length */
    const char *exp_argv[ARRAY_LEN];	/* expected array content */
} TEST_CASE;

#define TERMINATE_ARRAY (1)

#define PASS    (0)
#define FAIL    (1)

/* test_argv_populate - populate result from in_array*/

static ARGV *test_argv_populate(const TEST_CASE *tp, ARGV *argvp)
{
    const char *const * cpp;

    for (cpp = tp->in_array; *cpp; cpp++)
	argv_add(argvp, *cpp, (char *) 0);
    return (argvp);
}

/* test_argv_splitq - basic tests */

static ARGV *test_argv_splitq(const TEST_CASE *tp, ARGV *argvp)
{
    if (argvp)
	argv_free(argvp);
    return (tp->blame ?
	    argv_splitq_cw(tp->in_string, tp->delim, tp->parens, tp->blame) :
	    argv_splitq(tp->in_string, tp->delim, tp->parens));
}

/* test_argv_splitq_append - basic tests */

static ARGV *test_argv_splitq_append(const TEST_CASE *tp, ARGV *argvp)
{
    test_argv_populate(tp, argvp);
    return (argv_splitq_append(argvp, tp->in_string, tp->delim, tp->parens));
}

 /*
  * The test cases. TODO: argv_addn with good and bad string length.
  */
static const TEST_CASE test_cases[] = {
    {"argv_splitq, mixed non/comment",
	{}, " f#o  bar #baz ", CHARS_COMMA_SP, CHARS_BRACE, /* blame */ 0,
	test_argv_splitq,
	3, {"f#o", "bar", "#baz", 0}
    },
    {"argv_splitq_cw, non-comment",
	{}, " f#o { #bar } baz", CHARS_COMMA_SP, CHARS_BRACE, "blame-me",
	test_argv_splitq,
	3, {"f#o", "{ #bar }", "baz", 0}
    },
    {"argv_splitq_cw, mixed non/comment 1",
	{}, "f#o #{ #bar } baz", CHARS_COMMA_SP, CHARS_BRACE, "blame-me",
	test_argv_splitq,
	1, {"f#o", 0}
    },
    {"argv_splitq_cw, mixed non/comment 2",
	{}, " f#o  { #bar }  #baz ", CHARS_COMMA_SP, CHARS_BRACE, "blame-me",
	test_argv_splitq,
	2, {"f#o", "{ #bar }", 0}
    },
    {"argv_splitq_append, mixed non/comment",
	{"##"}, " f#o  { #bar } #baz ", CHARS_COMMA_SP, CHARS_BRACE, /* blame */ 0,
	test_argv_splitq_append,
	4, {"##", "f#o", "{ #bar }", "#baz", 0}
    },
#if 0
    {"argv_splitq_append_cw, non-comment",
	{"##"}, " f#o { #bar } baz", CHARS_COMMA_SP, CHARS_BRACE, "blame-me",
	test_argv_splitq_append,
	4, {"##", "f#o", "{ #bar }", "baz", 0}
    },
    {"argv_splitq_append_cw, mixed non/comment 1",
	{"##"}, "f#o #{ #bar } baz", CHARS_COMMA_SP, CHARS_BRACE, "blame-me",
	test_argv_splitq_append,
	2, {"##", "f#o", 0}
    },
    {"argv_splitq_append_cw, mixed non/comment 2",
	{"##"}, " f#o  { #bar }  #baz ", CHARS_COMMA_SP, CHARS_BRACE, "blame-me",
	test_argv_splitq_append,
	3, {"##", "f#o", "{ #bar }", 0}
    },
#endif
    0,
};

/* test_argv_verify - verify result */

static int test_argv_verify(const TEST_CASE *tp, ARGV *argvp)
{
    int     idx;

    if (argvp->argc != tp->exp_argc) {
	msg_warn("test case '%s': got argc: %ld, want: %d",
		 tp->label, (long) argvp->argc, tp->exp_argc);
	return (FAIL);
    }
    for (idx = 0; idx < argvp->argc; idx++) {
	if (strcmp(argvp->argv[idx], tp->exp_argv[idx]) != 0) {
	    msg_warn("test case '%s': index %d: got '%s', want: '%s'",
		     tp->label, idx, argvp->argv[idx], tp->exp_argv[idx]);
	    return (FAIL);
	}
    }
    return (PASS);
}

int     main(int argc, char **argv)
{
    const TEST_CASE *tp;
    int     pass = 0;
    int     fail = 0;

    msg_vstream_init(sane_basename((VSTRING *) 0, argv[0]), VSTREAM_ERR);

    for (tp = test_cases; tp->label != 0; tp++) {
	int     test_failed;
	ARGV   *argvp;

	msg_info("RUN  %s", tp->label);
	argvp = argv_alloc(1);
	tp->populate_fn(tp, argvp);
	test_failed = test_argv_verify(tp, argvp);
	if (test_failed) {
	    msg_info("FAIL %s", tp->label);
	    fail++;
	} else {
	    msg_info("PASS %s", tp->label);
	    pass++;
	}
	argv_free(argvp);
    }
    msg_info("PASS=%d FAIL=%d", pass, fail);
    exit(fail != 0);
}

#endif
