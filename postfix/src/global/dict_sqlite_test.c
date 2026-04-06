/*++
/* NAME
/*	dict_sqlite_test 1t
/* SUMMARY
/*	dict_sqlite unit test
/* SYNOPSIS
/*	./dict_sqlite_test
/* DESCRIPTION
/*	dict_sqlite_test runs and logs each configured test, reports if
/*	a test is a PASS or FAIL, and returns an exit status of zero if
/*	all tests are a PASS.
/* HERMETICITY
/* .ad
/* .fi
/*	Each test creates a temporary test database and a corresponding
/*	Postfix sqlite client configuration file, both having unique
/*	names. Otherwise, each test is hermetic.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>
#include <stdlib.h>
#include <string.h>

 /*
  * Utility library.
  */
#include <msg.h>
#include <msg_vstream.h>
#include <stringops.h>

 /*
  * Global library.
  */
#include <dict_sqlite.h>

 /*
  * Test libraries.
  */
#include <ptest.h>

#ifdef HAS_SQLITE

 /*
  * Override the printable.c module because it may break some tests.
  * 
  * TODO(wietse) move this to a fake_printable.c module that can override all
  * printable.c global symbols.
  */
int     util_utf8_enable;

char   *printable_except(char *string, int replacement, const char *except)
{
    return (string);
}

 /*
  * Scaffolding for dict_sqlite(3) tests.
  */

/* create_and_populate_db - create an empty database and optionally populate */

static void create_and_populate_db(PTEST_CTX *t, char *dbpath,
				           const char *commands)
{
    int     fd;

    /*
     * Create an empty database file with a unique name. Assume that an
     * adversary cannot rename or remove the file.
     */
    if ((fd = mkstemp(dbpath)) < 0)
	ptest_fatal(t, "mkstemp(\"%s\"): %m", dbpath);
    if (close(fd) < 0)
	ptest_fatal(t, "close %s: %m", dbpath);

    /*
     * TODO(wietse) Open the database file, prepare and execute commands to
     * populate the database, and close the database.
     */
    if (commands) {
	ptest_fatal(t, "commands are not yet supported");
    }
}

/* create_and_populate_cf - create sqlite_table(5) configuration file */

static void create_and_populate_cf(PTEST_CTX *t, char *cfpath,
				           const char *dbpath,
				           const char *cftext)
{
    int     fd;
    VSTREAM *fp;

    /*
     * Create an empty sqlite_table(5) configuration file with a unique name.
     * Assume that an adversary cannot rename or remove the file.
     */
    if ((fd = mkstemp(cfpath)) < 0)
	ptest_fatal(t, "mkstemp(\"%s\"): %m", cfpath);
    if ((fp = vstream_fdopen(fd, O_WRONLY)) == 0)
	ptest_fatal(t, "vstream_fdopen: %m");
    (void) vstream_fprintf(fp, "%s\ndbpath = %s\n", cftext, dbpath);
    if (vstream_fclose(fp) != 0)
	ptest_fatal(t, "vstream_fdclose: %m");
}

#endif					/* HAS_SQLITE */

 /*
  * Test structure. Some tests may come their own.
  */
typedef struct PTEST_CASE {
    const char *testname;
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
    const char *commands;		/* commands or null */
    const char *settings;		/* sqlite_table(5) */
    const char *want_log;		/* substring match or null */
} PTEST_CASE;

#define PASS    (0)
#define FAIL    (1)

#define PATH_TEMPLATE	"/tmp/test-XXXXXXX"

/* test_flag_non_recommended_query - flag non-recommended query payloads */

static void test_flag_non_recommended_query(PTEST_CTX *t, const PTEST_CASE *tp)
{
#ifdef HAS_SQLITE
    const char template[] = PATH_TEMPLATE;
    char    dbpath[sizeof(template)];
    char    cfpath[sizeof(template)];
    DICT   *dict;

    /* Prepare scaffolding database and configuration file. */
    memcpy(dbpath, template, sizeof(dbpath));
    create_and_populate_db(t, dbpath, tp->commands);
    memcpy(cfpath, template, sizeof(cfpath));
    create_and_populate_cf(t, cfpath, dbpath, tp->settings);

    if (tp->want_log)
	expect_ptest_log_event(t, tp->want_log);
    dict = dict_sqlite_open(cfpath, O_RDONLY, DICT_FLAG_UTF8_REQUEST);
    dict_close(dict);

    /* Cleanup scaffolding database and configuration files. */
    if (unlink(dbpath) < 0)
	ptest_error(t, "unlink %s: %m", dbpath);
    if (unlink(cfpath) < 0)
	ptest_error(t, "unlink %s: %m", cfpath);
#else
    ptest_skip(t);
#endif
}

 /*
  * The list of test cases.
  */
static const PTEST_CASE ptestcases[] = {

    /*
     * Tests to flag non-recommended query forms. These create an empty test
     * database, and open it with the dict_sqlite client without querying it.
     */
    {.testname = "no_dynamic_payload",
	.action = test_flag_non_recommended_query,
	.settings = "query = select a from b where c = 5",
    },
    {.testname = "dynamic_payload_inside_recommended_quotes",
	.action = test_flag_non_recommended_query,
	.settings = "query = select a from b where c = 'xx%syy'",
    },
    {.testname = "dynamic_payload_without_quotes",
	.action = test_flag_non_recommended_query,
	.settings = "query = select s from b where c = xx%syy",
	.want_log = "contains >%s< without the recommended '' quotes",
    },
    {.testname = "payload_inside_double_quotes",
	.action = test_flag_non_recommended_query,
	.settings = "query = select s from b where c = \"xx%syy\"",
	.want_log = "contains >%s< without the recommended '' quotes",
    },

    /*
     * TODO: Tests that actually populate a test database, and that query it
     * with the dict_sqlite client.
     */
};

#include <ptest_main.h>
