 /*
  * Test program to exercise dict_stream.c. See PTEST_README for
  * documentation.
  */

 /*
  * System library.
  */
#include <sys_defs.h>
#include <string.h>

 /*
  * Utility library.
  */
#include <vstream.h>
#include <vstring.h>
#include <dict.h>

 /*
  * Test library.
  */
#include <ptest.h>

typedef struct PTEST_CASE {
    const char *testname;
    void    (*action) (PTEST_CTX *t, const struct PTEST_CASE *);
    const char *mapname;		/* starts with brace */
    const char *want_err;		/* null or message */
    const char *want_cont;		/* null or content */
} PTEST_CASE;

#define DICT_TYPE_TEST	"test"
#define LEN(x)	VSTRING_LEN(x)
#define STR(x)	vstring_str(x)

static void test_dict_stream(PTEST_CTX *t, const PTEST_CASE *tp)
{
    VSTRING *got_err = 0;
    VSTRING *got_cont = vstring_alloc(100);
    VSTREAM *fp;
    struct stat st;
    ssize_t want_len;
    ssize_t got_len;

    fp = dict_stream_open(DICT_TYPE_TEST, tp->mapname, O_RDONLY,
			  0, &st, &got_err);
    if (fp) {
	if (tp->want_err) {
	    ptest_error(t, "got stream, want error '%s'", tp->want_err);
	} else if (!tp->want_err && got_err && LEN(got_err) > 0) {
	    ptest_error(t, "got error '%s', want noerror", STR(got_err));
	} else if (!tp->want_cont) {
	    ptest_error(t, "got stream, expected nostream");
	} else {
	    want_len = strlen(tp->want_cont);
	    if ((got_len = vstream_fread_buf(fp, got_cont, 2 * want_len)) < 0) {
		ptest_error(t, "content read error");
	    } else {
		VSTRING_TERMINATE(got_cont);
		if (strcmp(tp->want_cont, STR(got_cont)) != 0) {
		    ptest_error(t, "got content '%s', want '%s'",
				STR(got_cont), tp->want_cont);
		}
	    }
	}
    } else {
	if (!tp->want_err) {
	    ptest_error(t, "got nostream, want noerror");
	} else if (tp->want_cont) {
	    ptest_error(t, "got nostream, want stream");
	} else if (strcmp(STR(got_err), tp->want_err) != 0) {
	    ptest_error(t, "got error '%s', want '%s'",
			STR(got_err), tp->want_err);
	}
    }
    if (fp)
	vstream_fclose(fp);
    if (got_err)
	vstring_free(got_err);
    vstring_free(got_cont);
}


#define WANT_NOERR	0
#define WANT_NOCONT	0

const char rule_spec_error[] = DICT_TYPE_TEST " map: "
"syntax error after '}' in \"{blah blah}x\"";

const char inline_config_error[] = DICT_TYPE_TEST " map: "
"syntax error after '}' in \"{{foo bar}, {blah blah}}x\"";

static PTEST_CASE ptestcases[] = {
    {"normal", test_dict_stream,
	"{{foo bar}, {blah blah}}", WANT_NOERR, "foo bar\nblah blah\n"
    },
    {"trims leading/trailing wsp around rule-text", test_dict_stream,
	"{{ foo bar }, { blah blah }}", WANT_NOERR, "foo bar\nblah blah\n"
    },
    {"trims leading/trailing comma-wsp around rule-spec", test_dict_stream,
	"{, ,{foo bar}, {blah blah}, ,}", WANT_NOERR, "foo bar\nblah blah\n"
    },
    {"empty inline-file", test_dict_stream,
	"{, }", WANT_NOERR, ""
    },
    {"propagates extpar error for inline-file", test_dict_stream,
	"{{foo bar}, {blah blah}}x", inline_config_error, WANT_NOCONT
    },
    {"propagates extpar error for rule-spec", test_dict_stream,
	"{{foo bar}, {blah blah}x}", rule_spec_error, WANT_NOCONT
    },
};

#include <ptest_main.h>
