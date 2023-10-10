/*++
/* NAME
/*	valid_utf8_string 3
/* SUMMARY
/*	predicate if string is valid UTF-8
/* SYNOPSIS
/*	#include <stringops.h>
/*
/*	int	valid_utf8_string(str, len)
/*	const char *str;
/*	ssize_t	len;
/* DESCRIPTION
/*	valid_utf8_string() determines if all bytes in a string
/*	satisfy parse_utf8_char(3h) checks. See there for any
/*	implementation limitations.
/*
/*	A zero-length string is considered valid.
/* DIAGNOSTICS
/*	The result value is zero when the caller specifies a negative
/*	length, or a string that does not pass parse_utf8_char(3h) checks.
/* SEE ALSO
/*	parse_utf8_char(3h), parse one UTF-8 multibyte character
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
/*	porcupine.org
/*	Amawalk, NY 10501, USA
/*--*/

/* System library. */

#include <sys_defs.h>

/* Utility library. */

#include <stringops.h>
#include <parse_utf8_char.h>

/* valid_utf8_string - validate string according to RFC 3629 */

int     valid_utf8_string(const char *str, ssize_t len)
{
    const char *ep = str + len;
    const char *cp;
    const char *last;

    if (len < 0)
	return (0);
    if (len == 0)
	return (1);

    /*
     * Ideally, the compiler will inline parse_utf8_char().
     */
    for (cp = str; cp < ep; cp++) {
	if ((last = parse_utf8_char(cp, ep)) != 0)
	    cp = last;
	else
	    return (0);
    }
    return (1);
}

 /*
  * Stand-alone test program. Each string is a line without line terminator.
  */
#ifdef TEST
#include <stdlib.h>
#include <string.h>
#include <msg.h>
#include <vstream.h>
#include <msg_vstream.h>

 /*
  * Test cases for 1-, 2-, and 3-byte encodings. See printable() tests for
  * provenance.
  * 
  * XXX Need a test for 4-byte encodings, preferably with strings that can be
  * displayed.
  */
struct testcase {
    const char *name;
    const char *input;
    int     expected;
};

static const struct testcase testcases[] = {
    {"Printable ASCII",
	"printable", 1,
    },
    {"Latin accented text, no error",
	"na\303\257ve", 1,
    },
    {"Latin text, with error",
	"na\303ve", 0,
    },
    {"Viktor, Cyrillic, no error",
	"\320\262\320\270\320\272\321\202\320\276\321\200", 1,
    },
    {"Viktor, Cyrillic, two errors",
	"\320\262\320\320\272\272\321\202\320\276\321\200", 0,
    },
    {"Viktor, Hebrew, no error",
	"\327\225\327\231\327\247\327\230\327\225\326\274\327\250", 1,
    },
    {"Viktor, Hebrew, with error",
	"\327\225\231\327\247\327\230\327\225\326\274\327\250", 0,
    },
    {"Chinese (Simplified), no error",
	"\344\270\255\345\233\275\344\272\222\350\201\224\347\275\221\347"
	"\273\234\345\217\221\345\261\225\347\212\266\345\206\265\347\273"
	"\237\350\256\241\346\212\245\345\221\212", 1,
    },
    {"Chinese (Simplified), with errors",
	"\344\270\255\345\344\272\222\350\224\347\275\221\347"
	"\273\234\345\217\221\345\261\225\347\212\266\345\206\265\347\273"
	"\237\350\256\241\346\212\245\345", 0,
    },
};

int     main(int argc, char **argv)
{
    const struct testcase *tp;
    int     pass;
    int     fail;

#define NUM_TESTS       sizeof(testcases)/sizeof(testcases[0])

    msg_vstream_init(basename(argv[0]), VSTREAM_ERR);
    util_utf8_enable = 1;

    for (pass = fail = 0, tp = testcases; tp < testcases + NUM_TESTS; tp++) {
	int     actual;

	/*
	 * Notes:
	 * 
	 * - The msg(3) functions use printable() which interferes when logging
	 * inputs and outputs. Use vstream_fprintf() instead.
	 */
	vstream_fprintf(VSTREAM_ERR, "RUN  %s\n", tp->name);
	actual = valid_utf8_string(tp->input, strlen(tp->input));

	if (actual == tp->expected) {
	    vstream_fprintf(VSTREAM_ERR, "input: >%s<, want and got: >%s<\n",
			    tp->input, actual ? "valid" : "not valid");
	    vstream_fprintf(VSTREAM_ERR, "PASS %s\n", tp->name);
	    pass++;
	} else {
	    vstream_fprintf(VSTREAM_ERR, "input: >%s<, want: >%s<, got: >%s<\n",
			    tp->input, tp->expected ? "valid" : "not valid",
			    actual ? "valid" : "not valid");
	    vstream_fprintf(VSTREAM_ERR, "FAIL %s\n", tp->name);
	    fail++;
	}
    }
    msg_info("PASS=%d FAIL=%d", pass, fail);
    return (fail > 0);
}

#endif
