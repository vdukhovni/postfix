 /*
  * Test program to exercise unescape.c. See ptest_main.h for a documented
  * example.
  */

 /*
  * System library.
  */
#include <sys_defs.h>
#include <string.h>

 /*
  * Utility library
  */
#include <stringops.h>
#include <msg.h>

 /*
  * Test library.
  */
#include <ptest.h>

typedef struct PTEST_CASE {
    const char *testname;		/* Human-readable description */
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
    const char *input;
    const char *want;
} PTEST_CASE;

 /*
  * SLMs.
  */
#define STR	vstring_str

static char *to_octal_string(VSTRING *buf, const char *str)
{
    const unsigned char *cp;

    VSTRING_RESET(buf);
    for (cp = (const unsigned char *) str; *cp; cp++) {
	vstring_sprintf_append(buf, "%03o", *cp);
	if (cp[1])
	    vstring_strcat(buf, " ");
    }
    VSTRING_TERMINATE(buf);
    return (STR(buf));
}

static void test_unescape(PTEST_CTX *t, const PTEST_CASE *tp)
{
    VSTRING *got, *got_octal, *want_octal;;

    got = vstring_alloc(100);
    got_octal = vstring_alloc(100);
    want_octal = vstring_alloc(100);

    unescape(got, tp->input);
    if (strcmp(STR(got), tp->want) != 0)
	ptest_error(t, "unescape got '%s' want '%s'",
		    to_octal_string(got_octal, STR(got)),
		    to_octal_string(want_octal, tp->want));
    vstring_free(got);
    vstring_free(got_octal);
    vstring_free(want_octal);
}

static const PTEST_CASE ptestcases[] = {
    {
	 /* name */ "escape lowecase a-z",
	 /* action */ test_unescape,
	 /* input */ "\\a\\b\\c\\d\\e\\f\\g\\h\\i\\j\\k\\l\\m\\n\\o\\p\\q\\r\\s\\t\\u\\v\\w\\x\\y\\z",
	 /* want */ "\a\bcde\fghijklm\nopq\rs\tu\vwxyz",
    }, {
	 /* name */ "escape digits 0-9",
	 /* action */ test_unescape,
	 /* input */ "\\1\\2\\3\\4\\5\\6\\7\\8\\9",
	 /* want */ "\001\002\003\004\005\006\00789",
    }, {
	 /* name */ "\\nnn plus digit",
	 /* action */ test_unescape,
	 /* input */ "\\1234\\2345\\3456\\04567",
	 /* want */ "\1234\2345\3456\04567",
    }, {
	 /* name */ "non-ascii email",
	 /* action */ test_unescape,
	 /* input */ "rcpt to:<wietse@\\317\\200.porcupine.org>",
	 /* want */ "rcpt to:<wietse@\317\200.porcupine.org>",
    },
};

 /*
  * Test library.
  */
#include <ptest_main.h>
