/*++
/* NAME
/*	pmock_expect 3h
/* SUMMARY
/*	mock support for hermetic tests
/* SYNOPSIS
/*	#include <pmock_expect.h>
/*
/*	MOCK_EXPECT *pmock_expect_create(
/*	const MOCK_APPL_SIG *sig,
/*	const char *file,
/*	int	line,
/*	int	calls_expected,
/*	ssize_t	size)
/*
/*	int	pmock_expect_apply(
/*	const MOCK_APPL_SIG *sig,
/*	const MOCK_EXPECT *inputs,
/*	void *targets)
/*
/*	void	pmock_expect_free(MOCK_EXPECT *me)
/*
/*	void	pmock_expect_wrapup(PTEST_CTX *t)
/* DESCRIPTION
/*	This module provides support to implement mock functions
/*	that emulate real functions with the same name, but that
/*	respond to calls with prepared outputs. This requires that
/*	the real function has the "MOCKABLE" annotation.
/*
/*	For a simple example, see the pmock_expect_test.c file.
/*
/*	pmock_expect_create() creates an expectation for calls into
/*	a mock function (whose details are given with the MOCK_APPL_SIG
/*	argument). pmock_expect_create() initializes the generic
/*	expectation fields (file name, line number, and number of
/*	calls), and appends the resulting object to a dedicated
/*	list for the user-defined mock function. The pmock_expect_create()
/*	caller must save deep copies of the expected inputs and
/*	prepared outputs.
/*
/*	pmock_expect_apply() takes an inputs argument with mock call
/*	inputs, and looks up a matching expectation. If a match is
/*	found, and if its call count isn't already saturated,
/*	pmock_expect_apply() uses the targets argument to update the
/*	mock call outputs.
/*
/*	pmock_expect_wrapup() reports unused expectations, and
/*	destroys all expectations. Subsequent calls of this function
/*	do nothing.
/* DIAGNOSTICS
/*	pmock_expect_apply() returns 'true' when a match is found
/*	and the match is not saturated. Otherwise, it returns 'false'
/*	after generating a "too many calls" or "unexpected call"
/*	error. If that error is expected (with expect_ptest_error()),
/*	then it is ignored (not reported) and it will not count as
/*	a test failure.
/*
/*	pmock_expect_wrapup() logs a warning when some expectation
/*	has not been used.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	Google, Inc.
/*	111 8th Avenue
/*	New York, NY 10011, USA
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>

 /*
  * Utility library.
  */
#include <mymalloc.h>
#include <htable.h>

 /*
  * Testing library.
  */
#include <pmock_expect.h>
#include <ptest.h>

 /*
  * Private structure with all expectations for a single mock application.
  */
typedef struct MOCK_APPL {
    const MOCK_APPL_SIG *sig;		/* application-specific */
    MOCK_EXPECT *head;			/* first expectation */
    MOCK_EXPECT *tail;			/* last expectation */
} MOCK_APPL;

 /*
  * Collection of MOCK_APPL instances indexed by application name.
  */
static HTABLE *mock_appl_list;

/* mock_appl_create - create empty list for same-type expectations */

static MOCK_APPL *mock_appl_create(const MOCK_APPL_SIG *sig)
{
    MOCK_APPL *ma;

    /*
     * Initialize self.
     */
    ma = (MOCK_APPL *) mymalloc(sizeof(*ma));
    ma->sig = sig;
    ma->head = ma->tail = 0;
    return (ma);
}

/* mock_appl_list_free - destroy application node */

static void mock_appl_list_free(MOCK_APPL *ma)
{
    myfree(ma);
}

/* pmock_expect_create - create one mock expectation */

MOCK_EXPECT *pmock_expect_create(const MOCK_APPL_SIG *sig, const char *file,
				         int line, int calls_expected,
				         ssize_t size)
{
    MOCK_APPL *ma;
    MOCK_EXPECT *me;

    /*
     * Look up or instantiate the expectation for this mock application.
     */
    if (mock_appl_list == 0)
	mock_appl_list = htable_create(13);
    if ((ma = (MOCK_APPL *) htable_find(mock_appl_list, sig->name)) == 0) {
	ma = mock_appl_create(sig);
	(void) htable_enter(mock_appl_list, sig->name, (void *) ma);
    }

    /*
     * Initialize the generic expectation fields.
     */
    me = (MOCK_EXPECT *) mymalloc(size);
    me->file = mystrdup(file);
    me->line = line;
    me->calls_expected = calls_expected;
    me->calls_made = 0;
    me->next = 0;

    /*
     * Append the new expectation to this mock application list.
     */
    if (ma->head == 0)
	ma->head = me;
    else
	ma->tail->next = me;
    ma->tail = me;

    /*
     * Let the caller fill in their application-specific fields.
     */
    return (me);
}

/* pmock_expect_free - destroy one expectation node */

void    pmock_expect_free(MOCK_EXPECT *me)
{
    myfree(me->file);
    myfree(me);
}

/* pmock_expect_apply - match inputs and apply outputs */

int     pmock_expect_apply(const MOCK_APPL_SIG *sig,
			           const MOCK_EXPECT *inputs,
			           void *targets)
{
    MOCK_APPL *ma;
    MOCK_EXPECT *me;
    MOCK_EXPECT *saturated = 0;		/* saturated expectation */
    VSTRING *buf;
    PTEST_CTX *t;

    /*
     * Look up the mock application list.
     */
    if (mock_appl_list != 0 && (ma = (MOCK_APPL *)
			     htable_find(mock_appl_list, sig->name)) != 0) {

	/*
	 * Look for an expectation match that is not saturated. Remember the
	 * last saturated match.
	 */
	for (me = ma->head; me != 0; me = me->next) {
	    const MOCK_APPL_SIG *sig = ma->sig;

	    if (sig->match_expect == 0 || sig->match_expect(me, inputs)) {
		if (me->calls_expected == 0
		    || me->calls_made < me->calls_expected) {
		    if (sig->assign_expect)
			sig->assign_expect(me, targets);
		    me->calls_made += 1;
		    return (1);
		} else {
		    saturated = me;
		}
	    }
	}
    }

    /*
     * Report a saturated or unmatched expectation.
     */
    buf = vstring_alloc(100);
    t = ptest_ctx_current();
    if (saturated != 0) {
	ptest_error(t, "%s:%d too many calls: %s(%s)",
		    saturated->file, saturated->line, sig->name,
		    sig->print_expect(saturated, buf));
    } else {
	ptest_error(t, "unexpected call: %s(%s)", sig->name,
		    sig->print_expect(inputs, buf));
    }
    vstring_free(buf);
    return (0);
}

/* pmock_expect_wrapup - report unused expectations and clean up */

void    pmock_expect_wrapup(PTEST_CTX *t)
{
    HTABLE_INFO **info, **ht;
    MOCK_APPL *ma;
    MOCK_EXPECT *me, *next_me;
    VSTRING *buf = 0;
    const char *plural[] = {"", "s"};

    /*
     * Iterate over each mock application.
     * 
     * NOTE: do not call ptest_fatal(). This code runs after the test has
     * completed.
     */
    if (mock_appl_list != 0) {
	info = htable_list(mock_appl_list);
	for (ht = info; *ht; ht++) {
	    ma = (MOCK_APPL *) ht[0]->value;

	    /*
	     * Iterate over each expectation.
	     */
	    for (me = ma->head; me != 0; me = next_me) {
		next_me = me->next;
		if (me->calls_expected > 0
		    && me->calls_expected > me->calls_made) {
		    ma->sig->print_expect(me, buf ? buf :
					  (buf = vstring_alloc(100)));
		    ptest_error(t, "%s:%d got %d call%s for %s(%s), want %d",
				me->file, me->line, me->calls_made,
				plural[me->calls_made != 1],
				ma->sig->name, vstring_str(buf),
				me->calls_expected);
		} else if (me->calls_made == 0) {
		    ma->sig->print_expect(me, buf ? buf :
					  (buf = vstring_alloc(100)));
		    ptest_error(t, "%s:%d got 0 calls for %s(%s), want 1 or more",
				me->file, me->line, ma->sig->name,
				vstring_str(buf));
		}
		ma->sig->free_expect(me);
	    }
	    htable_delete(mock_appl_list, ma->sig->name, (void (*) (void *)) 0);
	    mock_appl_list_free(ma);
	}
	if (buf)
	    vstring_free(buf);
	myfree(info);
    }
    if (mock_appl_list != 0 && mock_appl_list->used != 0)
	ptest_error(t, "pmock_expect_wrapup: mock_appl_list->used is %ld",
		    (long) mock_appl_list->used);
}
