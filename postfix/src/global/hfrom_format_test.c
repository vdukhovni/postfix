 /*
  * Test program to exercise hfrom_format.c. See ptest_main.h for a
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
#include <hfrom_format.h>
#include <mail_params.h>

 /*
  * Test library.
  */
#include <ptest.h>

 /*
  * Test adapter.
  */
typedef struct PTEST_CASE {
    const char *testname;
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
} PTEST_CASE;

struct name_test_case {
    const char *label;			/* identifies test case */
    const char *config;			/* configuration under test */
    const char *want_warning;		/* expected warning or empty */
    const int want_code;		/* expected code */
};

static struct name_test_case name_test_cases[] = {
    {"hfrom_format_parse good-standard",
	 /* config */ HFROM_FORMAT_NAME_STD,
	 /* warning */ "",
	 /* want_code */ HFROM_FORMAT_CODE_STD
    },
    {"hfrom_format_parse good-obsolete",
	 /* config */ HFROM_FORMAT_NAME_OBS,
	 /* warning */ "",
	 /* want_code */ HFROM_FORMAT_CODE_OBS
    },
    {"hfrom_format_parse bad",
	 /* config */ "does-not-exist,",
	 /* warning */ "invalid setting: \"hfrom_format_parse bad = does-not-exist,\"",
	 /* code */ 0,
    },
    {"hfrom_format_parse empty",
	 /* config */ "",
	 /* warning */ "invalid setting: \"hfrom_format_parse empty = \"",
	 /* code */ 0,
    },
    0,
};

static void test_hfrom_format_parse(PTEST_CTX *t, const PTEST_CASE *tp)
{
    struct name_test_case *np;

    for (np = name_test_cases; np->label != 0; np++) {
	PTEST_RUN(t, np->label, {
	    int     got_code;

	    if (*np->want_warning)
		expect_ptest_error(t, np->want_warning);
	    got_code = hfrom_format_parse(np->label, np->config);
	    if (*np->want_warning == 0) {
		if (got_code != np->want_code) {
		    ptest_error(t, "got code \"%d\", want \"%d\"(%s)",
			       got_code, np->want_code,
			       str_hfrom_format_code(np->want_code));
		}
	    }
	});
    }
}

struct code_test_case {
    const char *label;			/* identifies test case */
    int     code;			/* code under test */
    const char *want_warning;		/* expected warning or empty */
    const char *want_name;		/* expected namme */
};

static struct code_test_case code_test_cases[] = {
    {"str_hfrom_format_code good-standard",
	 /* code */ HFROM_FORMAT_CODE_STD,
	 /* warning */ "",
	 /* want_name */ HFROM_FORMAT_NAME_STD
    },
    {"str_hfrom_format_code good-obsolete",
	 /* code */ HFROM_FORMAT_CODE_OBS,
	 /* warning */ "",
	 /* want_name */ HFROM_FORMAT_NAME_OBS
    },
    {"str_hfrom_format_code bad",
	 /* config */ 12345,
	 /* warning */ "invalid header format code: 12345",
	 /* want_name */ 0
    },
    0,
};

static void test_str_hfrom_format_code(PTEST_CTX *t, const PTEST_CASE *tp)
{
    struct code_test_case *cp;

    for (cp = code_test_cases; cp->label != 0; cp++) {
	PTEST_RUN(t, cp->label, {
	    const char *got_name;

	    if (*cp->want_warning)
		expect_ptest_error(t, cp->want_warning);
	    got_name = str_hfrom_format_code(cp->code);
	    if (*cp->want_warning == 0) {
		if (strcmp(got_name, cp->want_name) != 0) {
		    ptest_error(t, "got name: \"%s\", want: \"%s\"",
			       got_name, cp->want_name);
		}
	    }
	});
    }
}

 /*
  * Test adapter.
  */
static const PTEST_CASE ptestcases[] = {
    {"test hfrom_format_parse", test_hfrom_format_parse,},
    {"test str_hfrom_format_code", test_str_hfrom_format_code,},
};

#include <ptest_main.h>
