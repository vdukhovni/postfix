 /*
  * System library.
  */
#include <sys_defs.h>
#include <stdlib.h>
#include <string.h>
#include <stringops.h>

 /*
  * Utility library.
  */
#include <msg.h>
#include <msg_vstream.h>
#include <vstream.h>
#include <vstring.h>

 /*
  * Global library.
  */
#include <ehlo_mask.h>
#include <sendopts.h>
#include <x_esmtp_verb_mask.h>

 /*
  * Tests and test cases.
  */
typedef struct TEST_CASE {
    const char *label;			/* identifies test case */
    const char *text;
    int     want_mask;
    const char *want_text;
} TEST_CASE;

 /*
  * Verify that each verb has its unique bit mask and vice versa.
  */
static const TEST_CASE test_cases[] = {
    {"REQUIRETLS",
	"REQUIRETLS",
	SOPT_REQUIRETLS_ESMTP,
	"REQUIRETLS",
    },
    {"requiretls",
	"requiretls",
	SOPT_REQUIRETLS_ESMTP,
	"REQUIRETLS",
    },
    {"SMTPUTF8",
	"SMTPUTF8",
	SOPT_SMTPUTF8_REQUESTED,
	"SMTPUTF8",
    },
    {"smtputf8",
	"smtputf8",
	SOPT_SMTPUTF8_REQUESTED,
	"SMTPUTF8",
    },
    {"SMTPUTF8 ,",
	"SMTPUTF8 ,",
	SOPT_SMTPUTF8_REQUESTED,
	"SMTPUTF8",
    },
    {"SMTPUTF8, REQUIRETLS, foobar",
	"SMTPUTF8, REQUIRETLS, foobar",
	SOPT_SMTPUTF8_REQUESTED | SOPT_REQUIRETLS_ESMTP,
	"SMTPUTF8, REQUIRETLS",
    },
    {0},
};

int     main(int argc, char **argv)
{
    const TEST_CASE *tp;
    int     pass = 0;
    int     fail = 0;
    int     got_mask;
    const char *got_text;

    msg_vstream_init(sane_basename((VSTRING *) 0, argv[0]), VSTREAM_ERR);

    for (tp = test_cases; tp->label != 0; tp++) {
	msg_info("RUN  %s", tp->label);
	got_mask = x_esmtp_verb_mask(tp->text);
	if (got_mask != tp->want_mask) {
	    msg_warn("got mask '%s'", str_x_esmtp_verb_mask(got_mask));
	    msg_warn("want: '%s'", str_x_esmtp_verb_mask(tp->want_mask));
	    fail++;
	    msg_info("FAIL %s", tp->label);
	} else if ((got_text = str_x_esmtp_verb_mask(got_mask)),
		   strcmp(got_text, tp->want_text) != 0) {
	    msg_warn("got text '%s', want: '%s'", got_text, tp->want_text);
	    fail++;
	    msg_info("FAIL %s", tp->label);

	} else {
	    msg_info("PASS %s", tp->label);
	    pass++;
	}
    }
    msg_info("PASS=%d FAIL=%d", pass, fail);
    exit(fail != 0);
}
