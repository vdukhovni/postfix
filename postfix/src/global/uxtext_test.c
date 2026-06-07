 /*
  * Test program to exercise uxtext_quote.c. See PTEST_README for
  * documentation.
  */

 /*
  * System library.
  */
#include <sys_defs.h>
#include <ctype.h>
#include <string.h>

 /*
  * Utility library
  */
#include <msg.h>

 /*
  * Global library.
  */
#include <uxtext.h>

 /*
  * Test library.
  */
#include <ptest.h>

typedef struct PTEST_CASE {
    const char *testname;		/* Human-readable description */
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
    const char *input;
    const char *special;
    const char *want;
} PTEST_CASE;

 /*
  * SLMs.
  */
#define STR	vstring_str

static char *to_hex_string(VSTRING *buf, const char *str)
{
    const unsigned char *cp;

    VSTRING_RESET(buf);
    for (cp = (const unsigned char *) str; *cp; cp++) {
	vstring_sprintf_append(buf, ISPRINT(*cp) ? "%c" : "\\x%02x", *cp);
	if (cp[1])
	    vstring_strcat(buf, " ");
    }
    VSTRING_TERMINATE(buf);
    return (STR(buf));
}

static void test_uxtext_quote(PTEST_CTX *t, const PTEST_CASE *tp)
{
    VSTRING *got_q, *got_u, *got_as_hex, *want_as_hex;

    got_q = vstring_alloc(100);
    got_as_hex = vstring_alloc(100);
    want_as_hex = vstring_alloc(100);
    got_u = vstring_alloc(100);

    if (uxtext_quote(got_q, tp->input, tp->special) == 0)
	ptest_fatal(t, "uxtext_quote failed");
    if (strcmp(STR(got_q), tp->want) != 0)
	ptest_error(t, "uxtext_quote got '%s' want '%s'",
		    to_hex_string(got_as_hex, STR(got_q)),
		    to_hex_string(want_as_hex, tp->want));
    if (uxtext_unquote(got_u, STR(got_q)) == 0)
	ptest_fatal(t, "uxtext_unquote failed");
    if (strcmp(STR(got_u), tp->input) != 0)
	ptest_error(t, "uxtext_unquote got '%s', want '%s'",
		    to_hex_string(got_as_hex, STR(got_u)),
		    to_hex_string(want_as_hex, tp->input));
    vstring_free(got_q);
    vstring_free(got_u);
    vstring_free(got_as_hex);
    vstring_free(want_as_hex);
}

static void test_uxtext_quote_append(PTEST_CTX *t, const PTEST_CASE *tp)
{
    VSTRING *got_q, *got_as_hex, *want_as_hex;
    const char *want = "foobar";

    got_q = vstring_alloc(100);
    got_as_hex = vstring_alloc(100);
    want_as_hex = vstring_alloc(100);

    vstring_strcpy(got_q, "foo");
    uxtext_quote_append(got_q, "bar", "");
    if (strcmp(STR(got_q), want) != 0)
	ptest_error(t, "uxtext_quote got '%s' want '%s'",
		    to_hex_string(got_as_hex, STR(got_q)),
		    to_hex_string(want_as_hex, want));
    vstring_free(got_q);
    vstring_free(got_as_hex);
    vstring_free(want_as_hex);
}

static void test_unitext_quote(PTEST_CTX *t, const PTEST_CASE *tp)
{
    VSTRING *got_q, *got_u, *got_as_hex, *want_as_hex;

    got_q = vstring_alloc(100);
    got_as_hex = vstring_alloc(100);
    want_as_hex = vstring_alloc(100);
    got_u = vstring_alloc(100);

    if (unitext_quote(got_q, tp->input, tp->special) == 0)
	ptest_fatal(t, "unitext_quote failed");
    if (strcmp(STR(got_q), tp->want) != 0)
	ptest_error(t, "unitext_quote got '%s' want '%s'",
		    to_hex_string(got_as_hex, STR(got_q)),
		    to_hex_string(want_as_hex, tp->want));
    if (uxtext_unquote(got_u, STR(got_q)) == 0)
	ptest_error(t, "uxtext_unquote failed");
    if (strcmp(STR(got_u), tp->input) != 0)
	ptest_error(t, "unitext_quote got '%s' want '%s'",
		    to_hex_string(got_as_hex, STR(got_q)),
		    to_hex_string(want_as_hex, tp->input));
    vstring_free(got_q);
    vstring_free(got_u);
    vstring_free(got_as_hex);
    vstring_free(want_as_hex);
}

static void test_unitext_quote_append(PTEST_CTX *t, const PTEST_CASE *tp)
{
    VSTRING *got_q, *got_as_hex, *want_as_hex;
    const char *want = "foobar";

    got_q = vstring_alloc(100);
    got_as_hex = vstring_alloc(100);
    want_as_hex = vstring_alloc(100);

    vstring_strcpy(got_q, "foo");
    uxtext_quote_append(got_q, "bar", "");
    if (strcmp(STR(got_q), want) != 0)
	ptest_error(t, "uxtext_quote got '%s' want '%s'",
		    to_hex_string(got_as_hex, STR(got_q)),
		    to_hex_string(want_as_hex, want));
    vstring_free(got_q);
    vstring_free(got_as_hex);
    vstring_free(want_as_hex);
}

static const PTEST_CASE ptestcases[] = {
    {
	.testname = "uxtext_quote ascii non-control, special=\"\"",
	.action = test_uxtext_quote,
	.input = " !\"#$%&'()*+,-./"
	"0123456789:;<=>?@"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
	"abcdefghijklmnopqrstuvwxyz{|}~",
	.special = "",
	.want = "\\x{20}!\"#$%&'()*+,-./"
	"0123456789:;<=>?@"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\x{5C}]^_`"
	"abcdefghijklmnopqrstuvwxyz{|}~",
    }, {
	.testname = "uxtext_quote ascii non-control, special=\"+=\"",
	.action = test_uxtext_quote,
	.input = " !\"#$%&'()*+,-./"
	"0123456789:;<=>?@"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
	"abcdefghijklmnopqrstuvwxyz{|}~",
	.special = "+=",
	.want = "\\x{20}!\"#$%&'()*\\x{2B},-./"
	"0123456789:;<\\x{3D}>?@"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\x{5C}]^_`"
	"abcdefghijklmnopqrstuvwxyz{|}~",
    }, {
	.testname = "uxtext_quote quotes non-ASCII UTF",
	.action = test_uxtext_quote,
	.input = "\xcf\x80.mydomain",
	.special = "",
	.want = "\\x{3C0}.mydomain",
    }, {
	.testname = "uxtext_quote quotes controls",
	.action = test_uxtext_quote,
	.input = "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a"
	"\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15"
	"\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x7f",
	.special = "",
	.want = "\\x{01}\\x{02}\\x{03}\\x{04}\\x{05}"
	"\\x{06}\\x{07}\\x{08}\\x{09}\\x{0A}"
	"\\x{0B}\\x{0C}\\x{0D}\\x{0E}\\x{0F}\\x{10}"
	"\\x{11}\\x{12}\\x{13}\\x{14}\\x{15}"
	"\\x{16}\\x{17}\\x{18}\\x{19}\\x{1A}\\x{1B}"
	"\\x{1C}\\x{1D}\\x{1E}\\x{1F}\\x{7F}",
    }, {
	.testname = "uxtext_quote_append does append",
	.action = test_uxtext_quote_append,
    }, {
	.testname = "unitext_quote ascii non-control, special=\"\"",
	.action = test_unitext_quote,
	.input = " !\"#$%&'()*+,-./"
	"0123456789:;<=>?@"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
	"abcdefghijklmnopqrstuvwxyz{|}~",
	.special = "",
	.want = "\\x{20}!\"#$%&'()*+,-./"
	"0123456789:;<=>?@"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\x{5C}]^_`"
	"abcdefghijklmnopqrstuvwxyz{|}~",
    }, {
	.testname = "unitext_quote ascii non-control, special=\"+=\"",
	.action = test_unitext_quote,
	.input = " !\"#$%&'()*+,-./"
	"0123456789:;<=>?@"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
	"abcdefghijklmnopqrstuvwxyz{|}~",
	.special = "+=",
	.want = "\\x{20}!\"#$%&'()*\\x{2B},-./"
	"0123456789:;<\\x{3D}>?@"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\x{5C}]^_`"
	"abcdefghijklmnopqrstuvwxyz{|}~",
    }, {
	.testname = "unitext_quote does not quote non-ASCII UTF",
	.action = test_unitext_quote,
	.input = "\xcf\x80.mydomain",
	.special = "",
	.want = "\xcf\x80.mydomain",
    }, {
	.testname = "unitext_quote quotes controls",
	.action = test_unitext_quote,
	.input = "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a"
	"\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15"
	"\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x7f",
	.special = "",
	.want = "\\x{01}\\x{02}\\x{03}\\x{04}\\x{05}"
	"\\x{06}\\x{07}\\x{08}\\x{09}\\x{0A}"
	"\\x{0B}\\x{0C}\\x{0D}\\x{0E}\\x{0F}\\x{10}"
	"\\x{11}\\x{12}\\x{13}\\x{14}\\x{15}"
	"\\x{16}\\x{17}\\x{18}\\x{19}\\x{1A}\\x{1B}"
	"\\x{1C}\\x{1D}\\x{1E}\\x{1F}\\x{7F}",
    }, {
	.testname = "unitext_quote_append does append",
	.action = test_unitext_quote_append,
    },
};

 /*
  * Test library.
  */
#include <ptest_main.h>
