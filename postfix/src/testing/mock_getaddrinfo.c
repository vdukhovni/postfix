/*++
/* NAME
/*	mock_getaddrinfo 3
/* SUMMARY
/*	mock getaddrinfo/getnameinfo for hermetic tests
/* SYNOPSIS
/*	#include <mock_getaddrinfo.h>
/*
/*	int	getaddrinfo(
/*	const char *hostname,
/*	const char *servname,
/*	const struct addrinfo *hints,
/*	struct addrinfo **result,
/*
/*	int	getnameinfo(
/*	const struct sockaddr *sa,
/*	size_t salen,
/*	char	*host,
/*	size_t	hostlen,
/*	char	*serv,
/*	size_t	servlen
/*	int	flags)
/* EXPECTATION SETUP
/*	void	expect_getaddrinfo(
/*	int	calls_expected,
/*	int	retval,
/*	const char *hostname,
/*	const char *servname,
/*	const struct addrinfo *hints,
/*	struct addrinfo *result)
/*
/*	void	expect_getnameinfo(
/*	int	calls_expected,
/*	int	retval,
/*	const struct sockaddr *sa,
/*	size_t salen,
/*	char	*host,
/*	size_t	hostlen,
/*	char	*serv,
/*	size_t	servlen
/*	int	flags)
/* TEST DATA
/*	struct addrinfo *make_addrinfo(
/*	const struct addrinfo *hints,
/*	const char *name,
/*	const char *addr,
/*	int	port)
/*
/*	struct addrinfo *copy_addrinfo(const struct addrinfo *ai)
/*
/*	void	freeaddrinfo(struct addrinfo *ai)
/*
/*	struct sockaddr *make_sockaddr(
/*	int	family,
/*	const char *addr,
/*	int	port)
/*
/*	void	free_sockaddr(struct sockaddr *sa)
/* MATCHERS
/*	int	eq_addrinfo(
/*	PTEST_CTX * t,
/*	const char *what,
/*	struct addrinfo got,
/*	struct addrinfo want)
/*
/*	int	eq_sockaddr
/*	PTEST_CTX * t,
/*	const char *what,
/*	const struct sockaddr *got,
/*	size_t	gotlen,
/*	const struct sockaddr *want,
/*	size_t	wantlen)
/* DESCRIPTION
/*	This module implements mock system library functions that
/*	produce prepared outputs in response to expected inputs.
/*	This supports hermetic tests, i.e. tests that do not depend
/*	on host configuration or on network access.
/*
/*	The "expect_" functions take expected inputs and corresponding
/*	outputs. They make deep copies of their arguments, including
/*	"struct addrinfo *" linked lists. The retval argument
/*	specifies a prepared result value. The calls_expected
/*	argument specifies the expected number of calls (zero means
/*	one or more calls, not: zero calls).
/*
/*	make_addrinfo() creates one addrinfo structure. To create
/*	linked list, manually append make_addrinfo() results.
/*
/*	copy_addrinfo() makes a deep copy of a linked list of
/*	addrinfo structures.
/*
/*	freeaddrinfo() deletes a linked list of addrinfo structures.
/*	This function must be used for addrinfo structures created
/*	with make_addrinfo() and copy_addrinfo().
/*
/*	make_sockaddr() creates a sockaddr structure from the string
/*	representation of an IP address.
/*
/*	free_sockaddr() exists to make program code more explicit.
/*
/*	eq_addrinfo() compares addrinfo linked lists and reports
/*	differences with ptest_error(). The what argument provides
/*	context. Specify a null test context for silent operation.
/*
/*	eq_sockaddr() compares sockaddr instances and reports
/*	differences with ptest_error(). The what argument provides
/*	context. Specify a null test context for silent operation.
/*
/*	append_addrinfo_to_string() appends a textual representation
/*	of the referenced addrinfo to the specified buffer.
/*
/*	addrinfo_hints_to_string() writes a textual representation of
/*	the referenced getaddrinfo() hints object.
/*
/*	sockaddr_to_string() writes a textual representation of the
/*	referenced sockaddr object.
/*
/*	pf_to_string(), af_to_string(), socktype_to_string,
/*	ipprotocol_to_string, ai_flags_to_string(), ni_flags_to_string()
/*	produce a textual representation of addrinfo properties or
/*	getnameinfo() flags.
/* DIAGNOSTICS
/*	If a mock is called unexpectedly (the call arguments do not
/*	match any expectation, or they do match, but more calls are
/*	made than were expected), a warning is logged, and the test
/*	will be flagged as failed. For now the mock returns an error
/*	result to the caller.  TODO: consider aborting the test.
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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <wrap_netdb.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>			/* sprintf/snprintf */

 /*
  * Utility library.
  */
#include <msg.h>
#include <myaddrinfo.h>
#include <mymalloc.h>

 /*
  * Test library.
  */
#include <mock_getaddrinfo.h>
#include <pmock_expect.h>
#include <ptest.h>

#define MYSTRDUP_OR_NULL(x)	((x) ? mystrdup(x) : 0)
#define STR_OR_NULL(s)		((s) ? (s) : "(null)")

#define STR	vstring_str

 /*
  * Manage getaddrinfo() expectations and responses. We use this structure
  * for deep copies of expect_getaddrinfo() expected inputs and prepared
  * responses, and for shallow copies of getaddrinfo() inputs, so that we can
  * reuse the print_getaddrinfo() helper.
  */
struct getaddrinfo_expectation {
    MOCK_EXPECT mock_expect;		/* generic fields */
    int     retval;			/* result value */
    char   *node;			/* inputs */
    char   *service;
    struct addrinfo *hints;
    struct addrinfo *res;		/* outputs */
};

 /*
  * Pointers to getaddrinfo() outputs.
  */
struct getaddrinfo_targets {
    struct addrinfo **res;
    int    *retval;
};

/* match_getaddrinfo - match inputs against expectation */

static int match_getaddrinfo(const MOCK_EXPECT *expect,
			             const MOCK_EXPECT *inputs)
{
    struct getaddrinfo_expectation *pe =
    (struct getaddrinfo_expectation *) expect;
    struct getaddrinfo_expectation *pi =
    (struct getaddrinfo_expectation *) inputs;

    return (strcmp(STR_OR_NULL(pe->node), STR_OR_NULL(pi->node)) == 0
	  && strcmp(STR_OR_NULL(pe->service), STR_OR_NULL(pi->service)) == 0
	 && eq_addrinfo((PTEST_CTX *) 0, (char *) 0, pe->hints, pi->hints));
}

/* assign_getaddrinfo - assign expected output */

static void assign_getaddrinfo(const MOCK_EXPECT *expect, void *targets)
{
    struct getaddrinfo_expectation *pe =
    (struct getaddrinfo_expectation *) expect;
    struct getaddrinfo_targets *pt =
    (struct getaddrinfo_targets *) targets;

    if (pe->retval == 0)
	*(pt->res) = copy_addrinfo(pe->res);
    *pt->retval = pe->retval;
}

/* print_getaddrinfo - print expected inputs */

static char *print_getaddrinfo(const MOCK_EXPECT *expect, VSTRING *buf)
{
    struct getaddrinfo_expectation *pe =
    (struct getaddrinfo_expectation *) expect;
    VSTRING *hints_buf = vstring_alloc(100);

    vstring_sprintf(buf, "\"%s\", \"%s\", %s, (ptr)",
		    STR_OR_NULL(pe->node),
		    STR_OR_NULL(pe->service),
		    addrinfo_hints_to_string(hints_buf, pe->hints));
    vstring_free(hints_buf);
    return (vstring_str(buf));
}

/* free_getaddrinfo - destructor */

static void free_getaddrinfo(MOCK_EXPECT *expect)
{
    struct getaddrinfo_expectation *pe =
    (struct getaddrinfo_expectation *) expect;

    if (pe->node)
	myfree(pe->node);
    if (pe->service)
	myfree(pe->service);
    if (pe->hints)
	myfree(pe->hints);
    if (pe->retval == 0)
	freeaddrinfo(pe->res);
    pmock_expect_free(expect);
}

 /*
  * The mock name and its helper callbacks in one place.
  */
static const MOCK_APPL_SIG getaddrinfo_sig = {
    "getaddrinfo",
    match_getaddrinfo,
    assign_getaddrinfo,
    print_getaddrinfo,
    free_getaddrinfo,
};

/* _expect_getaddrinfo - set up expectation */

void    _expect_getaddrinfo(const char *file, int line,
			            int calls_expected, int retval,
			            const char *node,
			            const char *service,
			            const struct addrinfo *hints,
			            struct addrinfo *res)
{
    struct getaddrinfo_expectation *pe;

    pe = (struct getaddrinfo_expectation *)
	pmock_expect_create(&getaddrinfo_sig,
			    file, line, calls_expected, sizeof(*pe));
    pe->retval = retval;
    pe->node = MYSTRDUP_OR_NULL(node);
    pe->service = MYSTRDUP_OR_NULL(service);
    pe->hints = copy_addrinfo(hints);
    if (pe->retval == 0)
	pe->res = copy_addrinfo(res);
}

/* getaddrinfo - mock getaddrinfo */

int     getaddrinfo(const char *node,
		            const char *service,
		            const struct addrinfo *hints,
		            struct addrinfo **res)
{
    struct getaddrinfo_expectation inputs;
    struct getaddrinfo_targets targets;
    int     retval = EAI_FAIL;

    /*
     * Bundle the arguments to simplify handling.
     */
    inputs.node = (char *) node;
    inputs.service = (char *) service;
    inputs.hints = (struct addrinfo *) hints;

    targets.res = res;
    targets.retval = &retval;

    /*
     * TODO: bail out if there was no match?
     */
    (void) pmock_expect_apply(&getaddrinfo_sig,
			      &inputs.mock_expect, (void *) &targets);
    return (retval);
}

 /*
  * Manage getnameinfo() expectations and responses. We use this structure
  * for deep copies of expect_getnameinfo() expected inputs and prepared
  * responses, and for shallow copies of getnameinfo() inputs, so that we can
  * reuse the print_getnameinfo() helper.
  */
struct getnameinfo_expectation {
    MOCK_EXPECT mock_expect;		/* generic fields */
    int     retval;			/* result value */
    struct sockaddr *sa;		/* inputs */
    size_t  salen;
    char   *host;			/* outputs */
    size_t  hostlen;
    char   *serv;
    size_t  servlen;
    int     flags;			/* other input */
};

 /*
  * Pointers to getnameinfo() outputs.
  */
struct getnameinfo_targets {
    char   *host;
    size_t  hostlen;
    char   *serv;
    size_t  servlen;
    int    *retval;
};

/* match_getnameinfo - match inputs against expectation */

static int match_getnameinfo(const MOCK_EXPECT *expect,
			             const MOCK_EXPECT *inputs)
{
    struct getnameinfo_expectation *pe =
    (struct getnameinfo_expectation *) expect;
    struct getnameinfo_expectation *pi =
    (struct getnameinfo_expectation *) inputs;

    return (eq_sockaddr((PTEST_CTX *) 0, (char *) 0,
			pe->sa, pe->salen, pi->sa, pi->salen)
	    && pe->flags == pi->flags);
}

/* assign_getnameinfo - assign expected output */

static void assign_getnameinfo(const MOCK_EXPECT *expect, void *targets)
{
    struct getnameinfo_expectation *pe =
    (struct getnameinfo_expectation *) expect;
    struct getnameinfo_targets *pt =
    (struct getnameinfo_targets *) targets;

#define MIN_OF(x,y) ((x) < (y) ? (x) : (y))

    if (pe->retval == 0) {
	if (pt->host && pe->host) {
	    strncpy(pt->host, pe->host, MIN_OF(pt->hostlen, pe->hostlen));
	    pt->host[pt->hostlen - 1] = 0;
	}
	if (pt->serv && pe->serv) {
	    strncpy(pt->serv, pe->serv, MIN_OF(pt->servlen, pe->servlen));
	    pt->serv[pt->servlen - 1] = 0;
	}
    }
    *pt->retval = pe->retval;
}

/* print_getnameinfo - print inputs */

static char *print_getnameinfo(const MOCK_EXPECT *expect, VSTRING *buf)
{
    struct getnameinfo_expectation *pe =
    (struct getnameinfo_expectation *) expect;
    VSTRING *sockaddr_buf = vstring_alloc(100);
    VSTRING *flags_buf = vstring_alloc(100);

    vstring_sprintf(buf, "%s, %ld, (ptr), (len), (ptr), (len), %s",
		    sockaddr_to_string(sockaddr_buf, pe->sa, pe->salen),
		    (long) pe->salen,
		    ni_flags_to_string(flags_buf, pe->flags));
    vstring_free(sockaddr_buf);
    vstring_free(flags_buf);
    return (STR(buf));
}

/* free_getnameinfo - destructor */

static void free_getnameinfo(MOCK_EXPECT *expect)
{
    struct getnameinfo_expectation *pe =
    (struct getnameinfo_expectation *) expect;

    if (pe->sa)
	myfree(pe->sa);
    if (pe->host)
	myfree(pe->host);
    if (pe->serv)
	myfree(pe->serv);
    pmock_expect_free(expect);
}

 /*
  * The mock name and its helper callbacks in one place.
  */
static const MOCK_APPL_SIG getnameinfo_sig = {
    "getnameinfo",
    match_getnameinfo,
    assign_getnameinfo,
    print_getnameinfo,
    free_getnameinfo,
};

/* _expect_getnameinfo - set up expectation */

void    _expect_getnameinfo(const char *file, int line,
			            int calls_expected, int retval,
			            const struct sockaddr *sa, size_t salen,
			            const char *host, size_t hostlen,
			            const char *serv, size_t servlen,
			            int flags)
{
    struct getnameinfo_expectation *pe;

    pe = (struct getnameinfo_expectation *)
	pmock_expect_create(&getnameinfo_sig,
			    file, line, calls_expected, sizeof(*pe));
    pe->retval = retval;
    pe->sa = (struct sockaddr *) mymalloc(salen);
    memcpy(pe->sa, sa, salen);
    pe->salen = salen;
    pe->host = MYSTRDUP_OR_NULL(host);
    pe->hostlen = hostlen;
    pe->serv = MYSTRDUP_OR_NULL(serv);
    pe->servlen = servlen;
    pe->flags = flags;
}

/* getnameinfo - mock getnameinfo */

int     getnameinfo(const struct sockaddr *sa, socklen_t salen,
		            char *host, size_t hostlen,
		            char *serv, size_t servlen, int flags)
{
    struct getnameinfo_expectation inputs;
    struct getnameinfo_targets targets;
    int     retval = EAI_FAIL;

    /*
     * Bundle the arguments to simplify handling.
     */
    inputs.sa = (struct sockaddr *) sa;
    inputs.salen = salen;
    inputs.flags = flags;

    targets.host = host;
    targets.hostlen = hostlen;
    targets.serv = serv;
    targets.servlen = servlen;
    targets.retval = &retval;

    /*
     * TODO: bail out if there was no match?
     */
    (void) pmock_expect_apply(&getnameinfo_sig,
			      &inputs.mock_expect, (void *) &targets);
    return (retval);
}
