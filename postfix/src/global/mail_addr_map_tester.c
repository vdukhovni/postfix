/*++
/* NAME
/*	mail_addr_map_tester 1
/* SUMMARY
/*	mail_addr_map test program
/* SYNOPSIS
/*	mail_addr_map pass_tests | fail_tests
/* DESCRIPTION
/*	mail_addr_map performs the specified set of built-in
/*	unit tests. With 'pass_tests', all tests must pass, and
/*	with 'fail_tests' all tests must fail.
/* DIAGNOSTICS
/*	When a unit test fails, the program prints details of the
/*	failed test.
/*
/*	The program terminates with a non-zero exit status when at
/*	least one test does not pass with 'pass_tests', or when at
/*	least one test does not fail with 'fail_tests'.
/* SEE ALSO
/*	mail_addr_map(3), generic address mapping
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

/* System library. */

#include <sys_defs.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Utility library. */

#include <argv.h>
#include <msg.h>
#include <mymalloc.h>
#include <vstring.h>

/* Global library. */

#include <canon_addr.h>
#include <mail_addr_map.h>
#include <mail_conf.h>			/* XXX eliminate main.cf dependency */
#include <mail_params.h>

/* Application-specific. */

#define STR	vstring_str

typedef struct {
    const char *testname;
    const char *database;
    int     propagate;
    const char *delimiter;
    int     in_form;
    int     out_form;
    const char *address;
    const char *expect_argv[2];
    int     expect_argc;
} MAIL_ADDR_MAP_TEST;

#define DONT_PROPAGATE_UNMATCHED_EXTENSION	0
#define DO_PROPAGATE_UNMATCHED_EXTENSION	1
#define NO_RECIPIENT_DELIMITER			""
#define PLUS_RECIPIENT_DELIMITER		"+"

 /*
  * All these tests must pass, so that we know that mail_addr_map_opt() works
  * as intended.
  */
static MAIL_ADDR_MAP_TEST pass_tests[] = {
    {
	"1 external to external, no extension",
	"inline:{ aa@example.com=bb@example.com }",
	DO_PROPAGATE_UNMATCHED_EXTENSION, PLUS_RECIPIENT_DELIMITER,
	MAIL_ADDR_FORM_EXTERNAL, MAIL_ADDR_FORM_EXTERNAL,
	"aa@example.com",
	{"bb@example.com"}, 1,
    },
    {
	"2 external to external, extension, propagation",
	"inline:{ aa@example.com=bb@example.com }",
	DO_PROPAGATE_UNMATCHED_EXTENSION, PLUS_RECIPIENT_DELIMITER,
	MAIL_ADDR_FORM_EXTERNAL, MAIL_ADDR_FORM_EXTERNAL,
	"aa+ext@example.com",
	{"bb+ext@example.com"}, 1,
    },
    {
	"3 external to external, extension, no propagation, no match",
	"inline:{ aa@example.com=bb@example.com }",
	DONT_PROPAGATE_UNMATCHED_EXTENSION, NO_RECIPIENT_DELIMITER,
	MAIL_ADDR_FORM_EXTERNAL, MAIL_ADDR_FORM_EXTERNAL,
	"aa+ext@example.com",
	{0}, 0,
    },
    {
	"4 external to external, extension, full match",
	"inline:{{cc+ext@example.com=dd@example.com,ee@example.com}}",
	DO_PROPAGATE_UNMATCHED_EXTENSION, PLUS_RECIPIENT_DELIMITER,
	MAIL_ADDR_FORM_EXTERNAL, MAIL_ADDR_FORM_EXTERNAL,
	"cc+ext@example.com",
	{"dd@example.com", "ee@example.com"}, 2,
    },
    {
	"5 external to external, no extension, quoted",
	"inline:{ {\"a a\"@example.com=\"b b\"@example.com} }",
	DO_PROPAGATE_UNMATCHED_EXTENSION, PLUS_RECIPIENT_DELIMITER,
	MAIL_ADDR_FORM_EXTERNAL, MAIL_ADDR_FORM_EXTERNAL,
	"\"a a\"@example.com",
	{"\"b b\"@example.com"}, 1,
    },
    {
	"6 external to external, extension, propagation, quoted",
	"inline:{ {\"a a\"@example.com=\"b b\"@example.com} }",
	DO_PROPAGATE_UNMATCHED_EXTENSION, PLUS_RECIPIENT_DELIMITER,
	MAIL_ADDR_FORM_EXTERNAL, MAIL_ADDR_FORM_EXTERNAL,
	"\"a a+ext\"@example.com",
	{"\"b b+ext\"@example.com"}, 1,
    },
    {
	"7 internal to internal, no extension, propagation, embedded space",
	"inline:{ {\"a a\"@example.com=\"b b\"@example.com} }",
	DO_PROPAGATE_UNMATCHED_EXTENSION, PLUS_RECIPIENT_DELIMITER,
	MAIL_ADDR_FORM_INTERNAL, MAIL_ADDR_FORM_INTERNAL,
	"a a@example.com",
	{"b b@example.com"}, 1,
    },
    {
	"8 internal to internal, extension, propagation, embedded space",
	"inline:{ {\"a a\"@example.com=\"b b\"@example.com} }",
	DO_PROPAGATE_UNMATCHED_EXTENSION, PLUS_RECIPIENT_DELIMITER,
	MAIL_ADDR_FORM_INTERNAL, MAIL_ADDR_FORM_INTERNAL,
	"a a+ext@example.com",
	{"b b+ext@example.com"}, 1,
    },
    {
	"9 noconv to noconv, no extension, propagation, embedded space",
	"inline:{ {a_a@example.com=\"b b\"@example.com} }",
	DO_PROPAGATE_UNMATCHED_EXTENSION, PLUS_RECIPIENT_DELIMITER,
	MAIL_ADDR_FORM_INTERNAL, MAIL_ADDR_FORM_INTERNAL,
	"a_a@example.com",
	{"b b@example.com"}, 1,
    },
    {
	"10 noconv to noconv, extension, propagation, embedded space",
	"inline:{ {a_a@example.com=\"b b\"@example.com} }",
	DO_PROPAGATE_UNMATCHED_EXTENSION, PLUS_RECIPIENT_DELIMITER,
	MAIL_ADDR_FORM_INTERNAL, MAIL_ADDR_FORM_INTERNAL,
	"a_a+ext@example.com",
	{"b b+ext@example.com"}, 1,
    },
    0,
};

 /*
  * All these tests must fail, so that we know that the tests work.
  */
static MAIL_ADDR_MAP_TEST fail_tests[] = {
    {
	"selftest 1 external to external, no extension, quoted",
	"inline:{ {\"a a\"@example.com=\"b b\"@example.com} }",
	DO_PROPAGATE_UNMATCHED_EXTENSION, PLUS_RECIPIENT_DELIMITER,
	MAIL_ADDR_FORM_EXTERNAL, MAIL_ADDR_FORM_EXTERNAL,
	"\"a a\"@example.com",
	{"\"bXb\"@example.com"}, 1,
    },
    {
	"selftest 2 external to external, no extension, quoted",
	"inline:{ {\"a a\"@example.com=\"b b\"@example.com} }",
	DO_PROPAGATE_UNMATCHED_EXTENSION, PLUS_RECIPIENT_DELIMITER,
	MAIL_ADDR_FORM_EXTERNAL, MAIL_ADDR_FORM_EXTERNAL,
	"\"aXa\"@example.com",
	{"\"b b\"@example.com"}, 1,
    },
    {
	"selftest 3 external to external, no extension, quoted",
	"inline:{ {\"a a\"@example.com=\"b b\"@example.com} }",
	DO_PROPAGATE_UNMATCHED_EXTENSION, PLUS_RECIPIENT_DELIMITER,
	MAIL_ADDR_FORM_EXTERNAL, MAIL_ADDR_FORM_EXTERNAL,
	"\"a a\"@example.com",
	{0}, 0,
    },
    0,
};

/* canon_addr_external - surrogate to avoid trivial-rewrite dependency */

VSTRING *canon_addr_external(VSTRING *result, const char *addr)
{
    return (vstring_strcpy(result, addr));
}

static int compare(const char *testname,
		           const char **expect_argv, int expect_argc,
		           char **result_argv, int result_argc)
{
    int     n;
    int     err = 0;

    if (expect_argc != 0 && result_argc != 0) {
	for (n = 0; n < expect_argc && n < result_argc; n++) {
	    if (strcmp(expect_argv[n], result_argv[n]) != 0) {
		msg_warn("fail test %s: expect[%d]='%s', result[%d]='%s'",
			 testname, n, expect_argv[n], n, result_argv[n]);
		err = 1;
	    }
	}
    }
    if (expect_argc != result_argc) {
	msg_warn("fail test %s: expects %d results but there were %d",
		 testname, expect_argc, result_argc);
	for (n = expect_argc; n < result_argc; n++)
	    msg_info("no expect to match result[%d]='%s'", n, result_argv[n]);
	for (n = result_argc; n < expect_argc; n++)
	    msg_info("no result to match expect[%d]='%s'", n, expect_argv[n]);
	err = 1;
    }
    return (err);
}

static char *progname;

static NORETURN usage(void)
{
    msg_fatal("usage: %s pass_test | fail_test", progname);
}

int     main(int argc, char **argv)
{
    MAIL_ADDR_MAP_TEST *test;
    MAIL_ADDR_MAP_TEST *tests;
    int     errs = 0;

#define UPDATE(dst, src) { if (dst) myfree(dst); dst = mystrdup(src); }

    /*
     * Parse JCL.
     */
    progname = argv[0];
    if (argc != 2) {
	usage();
    } else if (strcmp(argv[1], "pass_tests") == 0) {
	tests = pass_tests;
    } else if (strcmp(argv[1], "fail_tests") == 0) {
	tests = fail_tests;
    } else {
	usage();
    }

    /*
     * Initialize.
     */
    mail_conf_read();				/* XXX eliminate */

    /*
     * A read-eval-print loop, because specifying C strings with quotes and
     * backslashes is painful.
     */
    for (test = tests; test->testname; test++) {
	ARGV   *result;
	int     fail = 0;

	if (mail_addr_form_to_string(test->in_form) == 0) {
	    msg_warn("test %s: bad in_form field: %d",
		     test->testname, test->in_form);
	    fail = 1;
	    continue;
	}
	if (mail_addr_form_to_string(test->out_form) == 0) {
	    msg_warn("test %s: bad out_form field: %d",
		     test->testname, test->out_form);
	    fail = 1;
	    continue;
	}
	MAPS   *maps = maps_create("test", test->database, DICT_FLAG_LOCK
			     | DICT_FLAG_FOLD_FIX | DICT_FLAG_UTF8_REQUEST);

	UPDATE(var_rcpt_delim, test->delimiter);
	result = mail_addr_map_opt(maps, test->address, test->propagate,
				   test->in_form, test->out_form);
	if (compare(test->testname, test->expect_argv, test->expect_argc,
	       result ? result->argv : 0, result ? result->argc : 0) != 0) {
	    msg_info("database = %s", test->database);
	    msg_info("propagate = %d", test->propagate);
	    msg_info("delimiter = '%s'", var_rcpt_delim);
	    msg_info("in_form = %s", mail_addr_form_to_string(test->in_form));
	    msg_info("out_form = %s", mail_addr_form_to_string(test->out_form));
	    msg_info("address = %s", test->address);
	    fail = 1;
	}
	maps_free(maps);
	if (result)
	    argv_free(result);

	/*
	 * It is an error if a test does not pass or fail as intended.
	 */
	errs += (tests == pass_tests ? fail : !fail);
    }
    return (errs != 0);
}
