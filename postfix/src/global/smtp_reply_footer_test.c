 /*
  * Test program to exercise smtp_reply_footer.c. See ptest_main.h for a
  * documented example.
  */

 /*
  * System library.
  */
#include <sys_defs.h>
#include <string.h>

 /*
  * Global library.
  */
#include <dsn_util.h>
#include <smtp_reply_footer.h>

 /*
  * Test library.
  */
#include <ptest.h>

 /*
  * SLMs.
  */
#define STR	vstring_str

typedef struct PTEST_CASE {
    const char *testname;
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
    const char *orig_reply;
    const char *template;
    const char *filter;
    int     want_status;
    const char *new_reply;
    const char *ignore_warning;
} PTEST_CASE;

#define NO_FILTER	((char *) 0)
#define NO_TEMPLATE	"NO_TEMPLATE"
#define NO_ERROR	(0)
#define BAD_SMTP	(-1)
#define BAD_MACRO	(-2)

static const char *lookup(const char *testname, int unused_mode, void *context)
{
    return "DUMMY";
}

static void test_footer(PTEST_CTX *t, const PTEST_CASE *tp)
{
    VSTRING *buf = vstring_alloc(10);
    int     got_status;
    void   *context = 0;

    if (tp->ignore_warning)
	expect_ptest_log_event(t, tp->ignore_warning);
    vstring_strcpy(buf, tp->orig_reply);
    got_status = smtp_reply_footer(buf, 0, tp->template, tp->filter,
				   lookup, context);
    if (got_status != tp->want_status) {
	ptest_error(t, "smtp_reply_footer status: got %d, want %d",
		    got_status, tp->want_status);
    } else if (got_status < 0 && strcmp(STR(buf), tp->orig_reply) != 0) {
	ptest_error(t, "smtp_reply_footer result: got \"%s\", want \"%s\"",
		    STR(buf), tp->orig_reply);
    } else if (got_status == 0 && strcmp(STR(buf), tp->new_reply) != 0) {
	ptest_error(t, "smtp_reply_footer result: got \"%s\", want \"%s\"",
		    STR(buf), tp->new_reply);
    }
    vstring_free(buf);
}

const PTEST_CASE ptestcases[] = {
    {"missing reply", test_footer, "", NO_TEMPLATE, NO_FILTER, BAD_SMTP, 0},
    {"long smtp_code", test_footer, "1234 foo", NO_TEMPLATE, NO_FILTER, BAD_SMTP, 0},
    {"short smtp_code", test_footer, "12 foo", NO_TEMPLATE, NO_FILTER, BAD_SMTP, 0},
    {"good+bad smtp_code", test_footer, "321 foo\r\n1234 foo", NO_TEMPLATE, NO_FILTER, BAD_SMTP, 0},
    {"1-line no dsn", test_footer, "550 Foo", "\\c footer", NO_FILTER, NO_ERROR, "550 Foo footer"},
    {"1-line no dsn", test_footer, "550 Foo", "Bar", NO_FILTER, NO_ERROR, "550-Foo\r\n550 Bar"},
    {"2-line no dsn", test_footer, "550-Foo\r\n550 Bar", "Baz", NO_FILTER, NO_ERROR, "550-Foo\r\n550-Bar\r\n550 Baz"},
    {"1-line with dsn", test_footer, "550 5.1.1 Foo", "Bar", NO_FILTER, NO_ERROR, "550-5.1.1 Foo\r\n550 5.1.1 Bar"},
    {"2-line with dsn", test_footer, "550-5.1.1 Foo\r\n450 4.1.1 Bar", "Baz", NO_FILTER, NO_ERROR, "550-5.1.1 Foo\r\n450-4.1.1 Bar\r\n450 4.1.1 Baz"},
    {"bad macro", test_footer, "220 myhostname", "\\c ${whatever", NO_FILTER, BAD_MACRO, 0, "truncated macro reference"},
    {"bad macroCRLF", test_footer, "220 myhostname\r\n", "\\c ${whatever", NO_FILTER, BAD_MACRO, 0, "truncated macro reference"},
    {"good macro", test_footer, "220 myhostname", "\\c $whatever", NO_FILTER, NO_ERROR, "220 myhostname DUMMY"},
    {"good macroCRLF", test_footer, "220 myhostname\r\n", "\\c $whatever", NO_FILTER, NO_ERROR, "220 myhostname DUMMY\r\n"},
};

#include <ptest_main.h>
