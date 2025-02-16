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

 /*
  * Tests and test cases.
  */
typedef struct TEST_CASE {
    const char *label;			/* identifies test case */
    int     mask;
    const char *want;
} TEST_CASE;

 /*
  * Verify that each verb has its unique bit mask.
  */
static const TEST_CASE test_cases[] = {
    {"EHLO_MASK_8BITMIME",
	EHLO_MASK_8BITMIME,
	"8BITMIME"
    },
    {"EHLO_MASK_PIPELINING",
	EHLO_MASK_PIPELINING,
	"PIPELINING"
    },
    {"EHLO_MASK_SIZE",
	EHLO_MASK_SIZE,
	"SIZE"
    },
    {"EHLO_MASK_VRFY",
	EHLO_MASK_VRFY,
	"VRFY"
    },
    {"EHLO_MASK_ETRN",
	EHLO_MASK_ETRN,
	"ETRN"
    },
    {"EHLO_MASK_AUTH",
	EHLO_MASK_AUTH,
	"AUTH"
    },
    {"EHLO_MASK_VERP",
	EHLO_MASK_VERP,
	"VERP"
    },
    {"EHLO_MASK_STARTTLS",
	EHLO_MASK_STARTTLS,
	"STARTTLS"
    },
    {"EHLO_MASK_XCLIENT",
	EHLO_MASK_XCLIENT,
	"XCLIENT"
    },
    {"EHLO_MASK_ENHANCEDSTATUSCODES",
	EHLO_MASK_ENHANCEDSTATUSCODES,
	"ENHANCEDSTATUSCODES"
    },
    {"EHLO_MASK_DSN",
	EHLO_MASK_DSN,
	"DSN"
    },
    {"EHLO_MASK_SMTPUTF8",
	EHLO_MASK_SMTPUTF8,
	"SMTPUTF8"
    },
    {"EHLO_MASK_CHUNKING",
	EHLO_MASK_CHUNKING,
	"CHUNKING"
    },
    {"EHLO_MASK_REQUIRETLS",
	EHLO_MASK_REQUIRETLS,
	"REQUIRETLS"
    },
    {"EHLO_MASK_SILENT",
	EHLO_MASK_SILENT,
	"SILENT-DISCARD"
    },
    {0},
};

int     main(int argc, char **argv)
{
    const TEST_CASE *tp;
    int     pass = 0;
    int     fail = 0;
    const char *got;

    msg_vstream_init(sane_basename((VSTRING *) 0, argv[0]), VSTREAM_ERR);

    for (tp = test_cases; tp->label != 0; tp++) {
	msg_info("RUN  %s", tp->label);
	got = str_ehlo_mask(tp->mask);
	if (strcmp(got, tp->want) != 0) {
	    msg_warn("got result '%s', want: '%s'", got, tp->want);
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
