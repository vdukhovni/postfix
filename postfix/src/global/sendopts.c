/*++
/* NAME
/*	sendopts 3
/* SUMMARY
/*	Support for SMTPUTF8, REQUIRETLS, etc.
/* SYNOPSIS
/*	#include <sendopts.h>
/*
/*	const char *sendopts_strflags(code)
/*	int	code;
/* DESCRIPTION
/*	Postfix queue files and IPC messages contain a sendopts field
/*	with flags that control SMTPUTF8, REQUIRETLS, etc. support. The
/*	flags are documented in sendopts(3h), and are based on information
/*	received with ESMTP requests or with message content.
/*
/*	The SMTPUTF8 flags life cycle is documented in smtputf8(3h).
/*
/*	sendopts_strflags() maps a sendopts flag value to printable
/*	string. The result is overwritten upon each call.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

/* System library. */

#include <sys_defs.h>

 /*
  * Utility library.
  */
#include <msg.h>
#include <vstring.h>
#include <name_mask.h>

 /*
  * Global library.
  */
#include <sendopts.h>

 /*
  * Mapping from flags code to printable string.
  */
static NAME_MASK sendopts_flag_map[] = {
    "smtputf8_requested", SOPT_SMTPUTF8_REQUESTED,
    "smtputf8_header", SOPT_SMTPUTF8_HEADER,
    "smtputf8_sender", SOPT_SMTPUTF8_SENDER,
    "smtputf8_recipient", SOPT_SMTPUTF8_RECIPIENT,
    "requiretls_header", SOPT_REQUIRETLS_HEADER,
    "requiretls_esmtp", SOPT_REQUIRETLS_ESMTP,
    0,
};

/* sendopts_strflags - map flags code to printable string */

const char *sendopts_strflags(unsigned flags)
{
    static VSTRING *result;

    if (flags == 0)
	return ("none");

    if (result == 0)
	result = vstring_alloc(20);
    else
	VSTRING_RESET(result);

    return (str_name_mask_opt(result, "sendopts_strflags", sendopts_flag_map,
			      flags, NAME_MASK_FATAL));
}

#ifdef TEST
#include <stdlib.h>
#include <string.h>
#include <stringops.h>
#include <msg_vstream.h>

 /*
  * Tests and test cases.
  */
typedef struct TEST_CASE {
    const char *label;			/* identifies test case */
    int     mask;
    const char *want;
} TEST_CASE;

static const TEST_CASE test_cases[] = {
    {"SOPT_SMTPUTF8_ALL",
	SOPT_SMTPUTF8_ALL,
	"smtputf8_requested smtputf8_header smtputf8_sender smtputf8_recipient"
    },
    {"SOPT_SMTPUTF8_DERIVED",
	SOPT_SMTPUTF8_DERIVED,
	"smtputf8_header smtputf8_sender smtputf8_recipient"
    },
    {"SOPT_SMTPUTF8_REQUESTED",
	SOPT_SMTPUTF8_REQUESTED,
	"smtputf8_requested"
    },
    {"SOPT_SMTPUTF8_HEADER",
	SOPT_SMTPUTF8_HEADER,
	"smtputf8_header"
    },
    {"SOPT_SMTPUTF8_SENDER",
	SOPT_SMTPUTF8_SENDER,
	"smtputf8_sender"
    },
    {"SOPT_SMTPUTF8_RECIPIENT",
	SOPT_SMTPUTF8_RECIPIENT,
	"smtputf8_recipient"
    },
    {"SOPT_REQUIRETLS_ALL",
	SOPT_REQUIRETLS_ALL,
	"requiretls_header requiretls_esmtp"
    },
    {"SOPT_REQUIRETLS_DERIVED",
	SOPT_REQUIRETLS_DERIVED,
	"requiretls_header"
    },
    {"SOPT_REQUIRETLS_HEADER",
	SOPT_REQUIRETLS_HEADER,
	"requiretls_header"
    },
    {"SOPT_REQUIRETLS_ESMTP",
	SOPT_REQUIRETLS_ESMTP,
	"requiretls_esmtp"
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
	got = sendopts_strflags(tp->mask);
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

#endif
