/*++
/* NAME
/*	mock_servent 3
/* SUMMARY
/*	getservbyname mock for hermetic tests
/* SYNOPSIS
/*	#include <mock_servent.h>
/*
/*	struct servent *getservbyname(
/*	const char *name,
/*	const char *proto)
/*
/*	void	setservent(
/*	int	stayopen)
/*
/*	void	endservent(void)
/* EXPECTATION SETUP
/*	void	expect_getservbyname(
/*	int	calls_expected,
/*	int	retval,
/*	const char *name,
/*	const char *proto)
/*
/*	void	expect_setservent(
/*	int	calls_expected,
/*	int	stayopen)
/*
/*	void	expect_endservent(
/*	int	calls_expected)
/*
/*	struct servent *make_servent(
/*	const char *name,
/*	int	port,
/*	const char *proto)
/*
/*	void	free_servent(
/*	struct servent *ent)
/* MATCHERS
/*	int	eq_servent(
/*	PTEST_CTX *t,
/*	const char *what,
/*	struct servent *got,
/*	struct servent *want)
/* DESCRIPTION
/*	This module implements a partial mock getservent(3) module
/*	that produces prepared outputs in response to expected
/*	inputs. This supports hermetic tests, i.e. tests that do
/*	not depend on host configuration or on network access.
/*
/*	expect_getservbyname() makes deep copies of its input
/*	arguments. The retval argument specifies a prepared result
/*	value. The calls_expected argument specifies the expected
/*	number of calls (zero means one or more calls, not: zero
/*	calls).
/*
/*	Note: getservbyname() maintains ownership of the struct
/*	servent result that is returned by the mock getservbyname()
/*	function. This is for consistency with the real getservbyname()
/*	which also maintains ownership of the result.
/*
/*	expect_setservent() copies its stayopen argument. The
/*	calls_expected argument specifies the expected number of
/*	calls (zero means one or more calls, not: zero calls).
/*
/*	expect_endservent() has no expected inputs. The calls_expected
/*	argument specifies the expected number of calls (zero means
/*	one or more calls, not: zero calls).
/*
/*	make_servent() returns a pointer to a minimal struct servent
/*	instance. Use free_servent() to destroy it.
/*
/*	eq_servent() is a predicate that compares its arguments for
/*	equality. The what argument is used in logging when the
/*	inputs differ. Specify a null test context for silent
/*	operation.
/* DIAGNOSTICS
/*	If a mock is called unexpectedly (the call arguments do not
/*	match an expectation, or more calls are made than expected),
/*	a warning is logged, and the test will be flagged as failed.
/*	For now the mock returns an error result to the caller.
/*	TODO: consider aborting the test.
/* SEE ALSO
/*	dns_lookup(3), domain name service lookup
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
#include <wrap_netdb.h>
#include <string.h>

 /*
  * Utility library.
  */
#include <mymalloc.h>
#include <msg.h>

 /*
  * Test library.
  */
#include <mock_servent.h>
#include <pmock_expect.h>

 /*
  * Helpers.
  */
#define MYSTRDUP_OR_NULL(x)             ((x) ? mystrdup(x) : 0)
#define STR_OR_NULL(s)                  ((s) ? (s) : "(null)")

#define STR(x)				vstring_str(x)

/* copy_servent - deep copy */

static struct servent *copy_servent(struct servent * src)
{
    struct servent *dst;

    dst = (struct servent *) mymalloc(sizeof(*dst));
    dst->s_name = MYSTRDUP_OR_NULL(src->s_name);
    dst->s_aliases = mymalloc(sizeof(*dst->s_aliases));
    dst->s_aliases[0] = 0;
    dst->s_port = src->s_port;
    dst->s_proto = MYSTRDUP_OR_NULL(src->s_proto);
    return (dst);
}

/* make_servent - create mock servent instance */

struct servent *make_servent(const char *name, int port, const char *proto)
{
    struct servent *dst;

    dst = (struct servent *) mymalloc(sizeof(*dst));
    dst->s_name = MYSTRDUP_OR_NULL(name);
    dst->s_aliases = mymalloc(sizeof(*dst->s_aliases));
    dst->s_aliases[0] = 0;
    dst->s_port = htons(port);
    dst->s_proto = MYSTRDUP_OR_NULL(proto);
    return (dst);
}

/* free_servent - destructor */

void    free_servent(struct servent * ent)
{
    if (ent->s_name)
	myfree(ent->s_name);
    if (ent->s_aliases)
	myfree((char *) ent->s_aliases);
    if (ent->s_proto)
	myfree(ent->s_proto);
    myfree(ent);
}

/* eq_aliases - equality predicate */

static int eq_aliases(PTEST_CTX *t, const char *file, int line, const char *what,
		              char **got, char **want)
{
    if (got[0] == 0 && want[0] == 0)
	return (1);
    if (got[0] == 0 || want[0] == 0) {
	if (t)
	    ptest_error(t, "%s: got alias %s, want %s",
			what, got[0] ? got[0] : "(null)",
			want[0] ? want[0] : "(null)");
	return (0);
    }
    if (strcmp(got[0], want[0]) != 0) {
	if (t)
	    ptest_error(t, "%s: got alias '%s', want '%s'",
			what, got[0], want[0]);
	return (0);
    }
    return (1);
}

/* _eq_servent - equality predicate */

int     _eq_servent(PTEST_CTX *t, const char *file, int line,
		            const char *what,
		            struct servent * got, struct servent * want)
{
    if (got == 0 && want == 0)
	return (1);
    if (got == 0 || want == 0) {
	if (t)
	    ptest_error(t, "%s: got %s, want %s",
			what, got ? "(struct servent *)" : "(null)",
			want ? "(struct servent *)" : "(null)");
	return (0);
    }
    if (strcmp(got->s_name, want->s_name) != 0) {
	if (t)
	    ptest_error(t, "%s: got name '%s', want '%s'",
			what, got->s_name, want->s_name);
	return (0);
    }
    if (!eq_aliases(t, file, line, what, got->s_aliases, want->s_aliases))
	return (0);
    if (got->s_port != want->s_port) {
	if (t)
	    ptest_error(t, "%s: got port %d, want %d",
			what, ntohs(got->s_port), ntohs(want->s_port));
	return (0);
    }
    if (strcmp(got->s_proto, want->s_proto) != 0) {
	if (t)
	    ptest_error(t, "%s: got proto '%s', want '%s'",
			what, got->s_proto, want->s_proto);
	return (0);
    }
    return (1);
}

 /*
  * Manage getservbyname() expectations and responses. We use this structure
  * for deep copies of expect_getservbyname() expected inputs and prepared
  * responses, and for shallow copies of getservbyname() inputs.
  */
struct getservbyname_expectation {
    MOCK_EXPECT mock_expect;		/* generic fields */
    char   *name;			/* inputs */
    char   *proto;
    struct servent *retval;		/* outputs */
};

 /*
  * Pointers to getservbyname() outputs.
  */
struct getservbyname_targets {
    struct servent **retval;
};

/* match_getservbyname - match inputs against expectation */

static int match_getservbyname(const MOCK_EXPECT *expect,
			               const MOCK_EXPECT *inputs)
{
    struct getservbyname_expectation *pe =
    (struct getservbyname_expectation *) expect;
    struct getservbyname_expectation *pi =
    (struct getservbyname_expectation *) inputs;

    return (strcmp(STR_OR_NULL(pe->name),
		   STR_OR_NULL(pi->name)) == 0
	    && strcmp(STR_OR_NULL(pe->proto),
		      STR_OR_NULL(pi->proto)) == 0);
}

/* assign_getservbyname - assign expected output */

static void assign_getservbyname(const MOCK_EXPECT *expect,
				         void *targets)
{
    struct getservbyname_expectation *pe =
    (struct getservbyname_expectation *) expect;
    struct getservbyname_targets *pt =
    (struct getservbyname_targets *) targets;

    /* Not a deep copy. */
    *pt->retval = pe->retval;
}

/* print_getservbyname - print expected inputs */

static char *print_getservbyname(const MOCK_EXPECT *expect,
				         VSTRING *buf)
{
    struct getservbyname_expectation *pe =
    (struct getservbyname_expectation *) expect;

    vstring_sprintf(buf, "\"%s\", \"%s\"",
		    STR_OR_NULL(pe->name), STR_OR_NULL(pe->proto));
    return (STR(buf));
}

/* free_getservbyname - destructor */

static void free_getservbyname(MOCK_EXPECT *expect)
{
    struct getservbyname_expectation *pe =
    (struct getservbyname_expectation *) expect;

    if (pe->name)
	myfree(pe->name);
    if (pe->proto)
	myfree(pe->proto);
    if (pe->retval)
	free_servent(pe->retval);
    pmock_expect_free(expect);
}

 /*
  * The mock name and its helper callbacks in one place.
  */
static const MOCK_APPL_SIG getservbyname_sig = {
    "getservbyname",
    match_getservbyname,
    assign_getservbyname,
    print_getservbyname,
    free_getservbyname,
};

/* _expect_getservbyname - set up expectation */

void    _expect_getservbyname(const char *file, int line,
			        int calls_expected, struct servent * retval,
			              const char *name, const char *proto)
{
    struct getservbyname_expectation *pe;

    pe = (struct getservbyname_expectation *)
	pmock_expect_create(&getservbyname_sig,
			    file, line, calls_expected, sizeof(*pe));
    /* Inputs. */
    pe->name = MYSTRDUP_OR_NULL(name);
    pe->proto = MYSTRDUP_OR_NULL(proto);
    /* Outputs. */
    pe->retval = retval ? copy_servent(retval) : retval;
}

/* getservbyname - answer the call with prepared responses */

struct servent *getservbyname(const char *name, const char *proto)
{
    struct getservbyname_expectation inputs;
    struct getservbyname_targets targets;
    struct servent *retval = 0;

    /*
     * Bundle the arguments to simplify handling.
     */
    inputs.name = (char *) name;
    inputs.proto = (char *) proto;

    targets.retval = &retval;

    /*
     * TODO: bail out if there was no match?
     */
    (void) pmock_expect_apply(&getservbyname_sig,
			      &inputs.mock_expect, (void *) &targets);
    return (retval);
}

 /*
  * Manage setservent() expectations. This function has no outputs.
  */
struct setservent_expectation {
    MOCK_EXPECT mock_expect;		/* generic fields */
    int     stayopen;			/* input */
};

static int match_setservent(const MOCK_EXPECT *expect,
			            const MOCK_EXPECT *inputs)
{
    struct setservent_expectation *pe =
    (struct setservent_expectation *) expect;
    struct setservent_expectation *pi =
    (struct setservent_expectation *) inputs;

    return (pe->stayopen == pi->stayopen);
}

/* print_setservent - print expected inputs */

static char *print_setservent(const MOCK_EXPECT *expect,
			              VSTRING *buf)
{
    struct setservent_expectation *pe =
    (struct setservent_expectation *) expect;

    vstring_sprintf(buf, "%d", pe->stayopen);
    return (STR(buf));
}

 /*
  * The mock name and its helper callbacks in one place.
  */
static const MOCK_APPL_SIG setservent_sig = {
    "setservent",
    match_setservent,
    0,					/* no outputs to assign */
    print_setservent,
    pmock_expect_free,
};

/* _expect_setservent - set up expectation */

void    _expect_setservent(const char *file, int line,
			           int calls_expected, int stayopen)
{
    struct setservent_expectation *pe;

    pe = (struct setservent_expectation *)
	pmock_expect_create(&setservent_sig,
			    file, line, calls_expected, sizeof(*pe));
    /* Inputs. */
    pe->stayopen = stayopen;
}

/* setservent - answer the call */

void    setservent(int stayopen)
{
    struct setservent_expectation inputs;

    /*
     * Bundle the arguments to simplify handling.
     */
    inputs.stayopen = stayopen;

    /*
     * TODO: bail out if there was no match?
     */
    (void) pmock_expect_apply(&setservent_sig,
			      &inputs.mock_expect, (void *) 0);
}

 /*
  * Manage endservent() expectations. This function has no arguments (all
  * calls will match), and no outputs.
  */

/* print_endservent - print expected inputs */

static char *print_endservent(const MOCK_EXPECT *expect,
			              VSTRING *buf)
{
    VSTRING_RESET(buf);
    VSTRING_TERMINATE(buf);
    return (STR(buf));
}

 /*
  * The mock name and its helper callbacks in one place.
  */
static const MOCK_APPL_SIG endservent_sig = {
    "endservent",
    0,					/* no inputs to match */
    0,					/* no outputs to assign */
    print_endservent,
    pmock_expect_free,
};

/* _expect_endservent - set up expectation */

void    _expect_endservent(const char *file, int line, int calls_expected)
{
    (void) pmock_expect_create(&endservent_sig,
			   file, line, calls_expected, sizeof(MOCK_EXPECT));
}

/* endservent - return no answer */

void    endservent(void)
{

    /*
     * TODO: bail out if there was no match?
     */
    (void) pmock_expect_apply(&endservent_sig, (MOCK_EXPECT *) 0, (void *) 0);
}
