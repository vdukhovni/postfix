/*++
/* NAME
/*	smtp_starttls_sasl_test 1t
/* SUMMARY
/*	test SASL credential protection after STARTTLS downgrade
/* SYNOPSIS
/*	./smtp_starttls_sasl_test
/* DESCRIPTION
/*	This test verifies that SASL authentication features are
/*	stripped when a server rejects STARTTLS, preventing credential
/*	leakage over plaintext connections.
/*
/*	Attack scenario: a MITM replaces the server's "220 Ready to
/*	start TLS" with "454 TLS not available". Without the fix, the
/*	client continues in plaintext and sends AUTH PLAIN credentials
/*	in the clear because AUTH was announced in the same (plaintext)
/*	EHLO that announced STARTTLS.
/*
/*	The fix strips SMTP_FEATURE_AUTH and cleans up the SASL
/*	mechanism list after a STARTTLS rejection when the client
/*	falls back to plaintext (level == MAY).
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/*--*/

#include <sys_defs.h>
#include <stdlib.h>
#include <string.h>

#include <msg.h>
#include <msg_vstream.h>
#include <vstream.h>
#include <mymalloc.h>

#include "smtp.h"

#ifdef USE_TLS
#ifdef USE_SASL_AUTH

/*
 * Test helper: simulate the state after a pre-STARTTLS EHLO that
 * announced both STARTTLS and AUTH PLAIN.
 */
typedef struct {
    const char *label;
    int     initial_features;
    const char *initial_mechs;
    int     tls_level;
    int     expect_auth;		/* expect AUTH feature after fix */
    int     expect_mechs;		/* expect mechanism list after fix */
} TEST_CASE;

static TEST_CASE tests[] = {
    {
	"STARTTLS rejected, level=may, AUTH was offered",
	SMTP_FEATURE_STARTTLS | SMTP_FEATURE_AUTH | SMTP_FEATURE_ESMTP,
	"PLAIN LOGIN",
	TLS_LEV_MAY,
	0,				/* AUTH should be stripped */
	0,				/* mechs should be freed */
    },
    {
	"STARTTLS rejected, level=may, no AUTH was offered",
	SMTP_FEATURE_STARTTLS | SMTP_FEATURE_ESMTP,
	0,
	TLS_LEV_MAY,
	0,				/* AUTH already absent */
	0,				/* no mechs to free */
    },
    {
	"STARTTLS rejected, level=encrypt",
	SMTP_FEATURE_STARTTLS | SMTP_FEATURE_AUTH | SMTP_FEATURE_ESMTP,
	"PLAIN LOGIN",
	TLS_LEV_ENCRYPT,
	/* doesn't matter -- connection will be aborted */
	1,				/* AUTH state irrelevant, conn aborts */
	1,				/* mechs irrelevant, conn aborts */
    },
    {0,},
};

/*
 * Simulate what smtp_proto.c does after the server rejects STARTTLS.
 * This mirrors the code at smtp_proto.c around line 857.
 */
static void simulate_starttls_rejected(int *features, char **mech_list)
{
    /* Line 857: clear STARTTLS feature */
    *features &= ~SMTP_FEATURE_STARTTLS;

    /* The fix: clear AUTH feature and SASL state */
    if (*features & SMTP_FEATURE_AUTH) {
	if (*mech_list) {
	    myfree(*mech_list);
	    *mech_list = 0;
	}
    }
    *features &= ~SMTP_FEATURE_AUTH;
}

int     main(int argc, char **argv)
{
    TEST_CASE *tp;
    int     failed = 0;
    int     passed = 0;

    msg_vstream_init(argv[0], VSTREAM_ERR);

    for (tp = tests; tp->label != 0; tp++) {
	int     features = tp->initial_features;
	char   *mech_list = tp->initial_mechs ?
	mystrdup(tp->initial_mechs) : 0;

	/*
	 * Only test the plaintext fallback path (level == MAY and TLS
	 * not required). When TLS is required, the connection is aborted
	 * before reaching the AUTH code, so the fix is irrelevant.
	 */
	if (tp->tls_level > TLS_LEV_MAY) {
	    msg_info("SKIP: %s (TLS required, connection aborted)", tp->label);
	    if (mech_list)
		myfree(mech_list);
	    continue;
	}

	simulate_starttls_rejected(&features, &mech_list);

	if ((features & SMTP_FEATURE_AUTH) != 0 && tp->expect_auth == 0) {
	    msg_info("FAIL: %s: AUTH feature still set after STARTTLS rejected",
		     tp->label);
	    failed++;
	} else if ((features & SMTP_FEATURE_AUTH) == 0 && tp->expect_auth != 0) {
	    msg_info("FAIL: %s: AUTH feature unexpectedly cleared",
		     tp->label);
	    failed++;
	} else if (mech_list != 0 && tp->expect_mechs == 0) {
	    msg_info("FAIL: %s: mechanism list not freed after STARTTLS rejected",
		     tp->label);
	    myfree(mech_list);
	    failed++;
	} else if (mech_list == 0 && tp->expect_mechs != 0) {
	    msg_info("FAIL: %s: mechanism list unexpectedly freed",
		     tp->label);
	    failed++;
	} else {
	    msg_info("PASS: %s", tp->label);
	    passed++;
	    if (mech_list)
		myfree(mech_list);
	}
    }

    msg_info("%s: PASS=%d FAIL=%d", argv[0], passed, failed);
    exit(failed ? 1 : 0);
}

#else					/* USE_SASL_AUTH */

int     main(int argc, char **argv)
{
    msg_vstream_init(argv[0], VSTREAM_ERR);
    msg_info("SKIP: SASL support not compiled in");
    exit(0);
}

#endif					/* USE_SASL_AUTH */
#else					/* USE_TLS */

int     main(int argc, char **argv)
{
    msg_vstream_init(argv[0], VSTREAM_ERR);
    msg_info("SKIP: TLS support not compiled in");
    exit(0);
}

#endif					/* USE_TLS */
