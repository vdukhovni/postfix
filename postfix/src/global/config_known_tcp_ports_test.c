 /*
  * Test program to exercise config_known_tcp_ports.c. See ptest_main.h for a
  * documented example.
  */

 /*
  * System library.
  */
#include <sys_defs.h>
#include <string.h>

 /*
  * Utility library.
  */
#include <known_tcp_ports.h>
#include <vstring.h>

 /*
  * Global library.
  */
#include <config_known_tcp_ports.h>

 /*
  * Test library.
  */
#include <ptest.h>

typedef struct PTEST_CASE {
    const char *testname;		/* identifies test case */
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
    const char *config;			/* configuration under test */
    const char *want_warning;		/* expected warning or null */
    const char *want_export;		/* expected export or null */
} PTEST_CASE;

static void test_config_known_tcp_ports(PTEST_CTX *t, const PTEST_CASE *tp)
{
    VSTRING *export_buf;
    const char *got_export;

    export_buf = vstring_alloc(100);
    if (*tp->want_warning)
	expect_ptest_error(t, tp->want_warning);
    config_known_tcp_ports(tp->testname, tp->config);
    got_export = export_known_tcp_ports(export_buf);
    if (strcmp(got_export, tp->want_export) != 0)
	ptest_error(t, "got export \"%s\", want \"%s\"",
		    got_export, tp->want_export);
    clear_known_tcp_ports();
    vstring_free(export_buf);
}

static const PTEST_CASE ptestcases[] = {
    {"good", test_config_known_tcp_ports,
	 /* config */ "smtp = 25, smtps = submissions = 465, lmtp = 24",
	 /* warning */ "",
	 /* export */ "lmtp=24 smtp=25 smtps=465 submissions=465"
    },
    {"equal-equal", test_config_known_tcp_ports,
	 /* config */ "smtp = 25, smtps == submissions = 465, lmtp = 24",
	 /* warning */ "equal-equal: in \" smtps == submissions = 465\": "
	"missing service name before \"=\"",
	 /* export */ "lmtp=24 smtp=25 smtps=465 submissions=465"
    },
    {"port test 1", test_config_known_tcp_ports,
	 /* config */ "smtps = submission =",
	 /* warning */ "port test 1: in \"smtps = submission =\": "
	"missing port value after \"=\"",
	 /* export */ ""
    },
    {"port test 2", test_config_known_tcp_ports,
	 /* config */ "smtps = submission = 4 65",
	 /* warning */ "port test 2: in \"smtps = submission = 4 65\": "
	"whitespace in port number",
	 /* export */ ""
    },
    {"port test 3", test_config_known_tcp_ports,
	 /* config */ "lmtp = 24, smtps = submission = foo",
	 /* warning */ "port test 3: in \" smtps = submission = foo\": "
	"non-numerical service port",
	 /* export */ "lmtp=24"
    },
    {"service name test 1", test_config_known_tcp_ports,
	 /* config */ "smtps = sub mission = 465",
	 /* warning */ "service name test 1: in \"smtps = sub mission = 465\": "
	"whitespace in service name",
	 /* export */ "smtps=465"
    },
    {"service name test 2", test_config_known_tcp_ports,
	 /* config */ "lmtp = 24, smtps = 1234 = submissions = 465",
	 /* warning */ "service name test 2: in \" smtps = 1234 = submissions "
	"= 465\": numerical service name",
	 /* export */ "lmtp=24 smtps=465 submissions=465"
    },
};

#include <ptest_main.h>
