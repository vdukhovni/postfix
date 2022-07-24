/*++
/* NAME
/*	mock_myaddrinfo 3
/* SUMMARY
/*	myaddrinfo mock for hermetic tests
/* SYNOPSIS
/*	#include <mock_myaddrinfo.h>
/*
/*	int	hostname_to_sockaddr_pf(
/*	const char *hostname,
/*	int	pf,
/*	const char *service,
/*	int	socktype,
/*	struct addrinfo **result,
/*
/*	int	hostaddr_to_sockaddr(
/*	const char *hostaddr,
/*	const char *service,
/*	int	socktype,
/*	struct addrinfo **result)
/*
/*	int	sockaddr_to_hostaddr(
/*	const struct sockaddr *sa,
/*	SOCKADDR_SIZE salen,
/*	MAI_HOSTADDR_STR *hostaddr,
/*	MAI_SERVPORT_STR *portnum,
/*	int	socktype)
/*
/*	int	sockaddr_to_hostname(
/*	const struct sockaddr *sa,
/*	SOCKADDR_SIZE salen,
/*	MAI_HOSTNAME_STR *hostname,
/*	MAI_SERVNAME_STR *service,
/*	int	socktype)
/* EXPECTATION SETUP
/*	void	expect_hostname_to_sockaddr_pf(
/*	int	calls_expected,
/*	int	retval,
/*	const char *hostname,
/*	int	pf,
/*	const char *service,
/*	int	socktype,
/*	struct addrinfo *result)
/*
/*	void	expect_hostaddr_to_sockaddr(
/*	int	calls_expected,
/*	int	retval,
/*	const char *hostaddr,
/*	const char *service,
/*	int	socktype,
/*	struct addrinfo *result)
/*
/*	void	expect_sockaddr_to_hostaddr(
/*	int	calls_expected,
/*	int	retval,
/*	const struct sockaddr *sa,
/*	SOCKADDR_SIZE salen,
/*	MAI_HOSTADDR_STR *hostaddr,
/*	MAI_SERVPORT_STR *portnum,
/*	int	socktype)
/*
/*	void	expect_sockaddr_to_hostname(
/*	int	calls_expected,
/*	int	retval,
/*	const struct sockaddr *sa,
/*	SOCKADDR_SIZE salen,
/*	MAI_HOSTNAME_STR *hostname,
/*	MAI_SERVNAME_STR *service,
/*	int	socktype)
/* TEST DATA
/*	struct addrinfo *make_addrinfo(
/*	const struct addrinfo *hints,
/*	const char *name,
/*	const char *addr)
/*
/*	struct addrinfo *copy_addrinfo(const struct addrinfo *ai)
/*
/*	void	freeaddrinfo(struct addrinfo *ai)
/*
/*	struct sockaddr *make_sockaddr(
/*	const char *addr,
/*	int	port)
/*
/*	void	free_sockaddr(struct sockaddr *sa)
/* MATCHERS
/*	int	eq_addrinfo(
/*	PTEST_CTX *t,
/*	const char *what,
/*	struct addrinfo *got,
/*	struct addrinfo *want)
/* DESCRIPTION
/*	This module implements mock myaddrinfo() lookup and conversion
/*	functions that produce prepared outputs in response to
/*	expected inputs. This supports hermetic tests, i.e. tests
/*	that do not depend on host configuration or on network
/*	access.
/*
/*	This module also provides a mock freeaddrinfo() function.
/*	This is needed because the mock_myaddrinfo library and the
/*	system library may use different memory allocation strategies.
/*
/*	The "expect_" functions take expected inputs and corresponding
/*	outputs. They make deep copies of their arguments, including
/*	the "struct addrinfo *" linked lists. The retval argument
/*	specifies a prepared result value. The calls_expected argument
/*	specifies the expected number of calls (zero means one or
/*	more calls, not: zero calls).
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
/*	differences with ptest_error().  The what argument provides
/*	context. Specify a null test context for silent operation.
/* DIAGNOSTICS
/*	If a mock is called unexpectedly (the call arguments do not
/*	match the expectation, or more calls are made than expected),
/*	a warning is logged, and the test will be flagged as failed.
/*	For now the mock returns an error result to the caller.
/*	TODO: consider aborting the test.
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
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>			/* sprintf */

 /*
  * Utility library.
  */
#include <mymalloc.h>
#include <msg.h>
#include <myaddrinfo.h>

 /*
  * Test library.
  */
#include <mock_myaddrinfo.h>
#include <pmock_expect.h>
#include <make_addr.h>
#include <ptest.h>

#define MYSTRDUP_OR_NULL(x)		((x) ? mystrdup(x) : 0)
#define STR_OR_NULL(s)			((s) ? (s) : "(null)")

 /*
  * Manage hostname_to_sockaddr_pf() expectations and responses. We use this
  * structure for deep copies pf expect_hostname_to_sockaddr_pf() expected
  * inputs and prepared responses, and for shallow copies of
  * hostname_to_sockaddr_pf() inputs, so that we can reuse the
  * print_hostname_to_sockaddr_pf() helper.
  */
struct hostname_to_sockaddr_pf_expectation {
    MOCK_EXPECT mock_expect;		/* generic fields */
    int     retval;			/* result value */
    char   *hostname;			/* inputs */
    int     pf;
    char   *service;
    int     socktype;
    struct addrinfo *res;		/* other outputs */
};

 /*
  * Pointers to hostname_to_sockaddr_pf() outputs.
  */
struct hostname_to_sockaddr_pf_targets {
    struct addrinfo **res;
    int    *retval;
};

/* match_hostname_to_sockaddr_pf - match inputs against expectation */

static int match_hostname_to_sockaddr_pf(const MOCK_EXPECT *expect,
					         const MOCK_EXPECT *inputs)
{
    struct hostname_to_sockaddr_pf_expectation *pe =
    (struct hostname_to_sockaddr_pf_expectation *) expect;
    struct hostname_to_sockaddr_pf_expectation *pi =
    (struct hostname_to_sockaddr_pf_expectation *) inputs;

    return (strcmp(STR_OR_NULL(pe->hostname),
		   STR_OR_NULL(pi->hostname)) == 0
	    && pe->pf == pi->pf
	    && strcmp(STR_OR_NULL(pe->service),
		      STR_OR_NULL(pi->service)) == 0
	    && pe->socktype == pi->socktype);
}

/* assign_hostname_to_sockaddr_pf - assign expected output */

static void assign_hostname_to_sockaddr_pf(const MOCK_EXPECT *expect,
					           void *targets)
{
    struct hostname_to_sockaddr_pf_expectation *pe =
    (struct hostname_to_sockaddr_pf_expectation *) expect;
    struct hostname_to_sockaddr_pf_targets *pt =
    (struct hostname_to_sockaddr_pf_targets *) targets;

    if (pe->retval == 0)
	*(pt->res) = copy_addrinfo(pe->res);
    *pt->retval = pe->retval;
}

/* print_hostname_to_sockaddr_pf - print expected inputs */

static char *print_hostname_to_sockaddr_pf(const MOCK_EXPECT *expect,
					           VSTRING *buf)
{
    struct hostname_to_sockaddr_pf_expectation *pe =
    (struct hostname_to_sockaddr_pf_expectation *) expect;

    vstring_sprintf(buf, "\"%s\", %d, \"%s\", %d, (ptr)",
		    STR_OR_NULL(pe->hostname), pe->pf,
		    STR_OR_NULL(pe->service), pe->socktype);
    return (vstring_str(buf));
}

/* free_hostname_to_sockaddr_pf - destructor */

static void free_hostname_to_sockaddr_pf(MOCK_EXPECT *expect)
{
    struct hostname_to_sockaddr_pf_expectation *pe =
    (struct hostname_to_sockaddr_pf_expectation *) expect;

    if (pe->hostname)
	myfree(pe->hostname);
    if (pe->service)
	myfree(pe->service);
    if (pe->retval == 0)
	freeaddrinfo(pe->res);
    pmock_expect_free(expect);
}

 /*
  * The mock name and its helper callbacks in one place.
  */
static const MOCK_APPL_SIG hostname_to_sockaddr_pf_sig = {
    "hostname_to_sockaddr_pf",
    match_hostname_to_sockaddr_pf,
    assign_hostname_to_sockaddr_pf,
    print_hostname_to_sockaddr_pf,
    free_hostname_to_sockaddr_pf,
};

/* _expect_hostname_to_sockaddr_pf - set up expectation */

void    _expect_hostname_to_sockaddr_pf(const char *file, int line,
				             int calls_expected, int retval,
					        const char *hostname, int pf,
					        const char *service,
					        int socktype,
					        struct addrinfo *res)
{
    struct hostname_to_sockaddr_pf_expectation *pe;

    pe = (struct hostname_to_sockaddr_pf_expectation *)
	pmock_expect_create(&hostname_to_sockaddr_pf_sig,
			    file, line, calls_expected, sizeof(*pe));
    pe->retval = retval;
    pe->hostname = MYSTRDUP_OR_NULL(hostname);
    pe->pf = pf;
    pe->service = MYSTRDUP_OR_NULL(service);
    pe->socktype = socktype;
    if (pe->retval == 0)
	pe->res = copy_addrinfo(res);
}

/* hostname_to_sockaddr_pf - mock hostname_to_sockaddr_pf */

int     hostname_to_sockaddr_pf(const char *hostname, int pf,
				        const char *service, int socktype,
				        struct addrinfo **res)
{
    struct hostname_to_sockaddr_pf_expectation inputs;
    struct hostname_to_sockaddr_pf_targets targets;
    int     retval = EAI_FAIL;

    /*
     * Bundle the arguments to simplify handling.
     */
    inputs.hostname = (char *) hostname;
    inputs.pf = pf;
    inputs.service = (char *) service;
    inputs.socktype = socktype;

    targets.res = res;
    targets.retval = &retval;

    /*
     * TODO: bail out if there was no match?
     */
    (void) pmock_expect_apply(&hostname_to_sockaddr_pf_sig,
			      &inputs.mock_expect, (void *) &targets);
    return (retval);
}

 /*
  * Manage hostaddr_to_sockaddr() expectations and responses. We use this
  * structure for deep copies of expect_hostaddr_to_sockaddr() expected
  * inputs and prepared responses, and for shallow copies
  * hostaddr_to_sockaddr() inputs, so that we can reuse the
  * print_hostaddr_to_sockaddr() helper.
  */
struct hostaddr_to_sockaddr_expectation {
    MOCK_EXPECT mock_expect;		/* generic fields */
    int     retval;			/* result value */
    char   *hostaddr;			/* inputs */
    char   *service;
    int     socktype;
    struct addrinfo *res;		/* other outputs */
};

 /*
  * Pointers to hostaddr_to_sockaddr() outputs.
  */
struct hostaddr_to_sockaddr_targets {
    struct addrinfo **res;
    int    *retval;
};

/* match_hostaddr_to_sockaddr - match inputs against expectation */

static int match_hostaddr_to_sockaddr(const MOCK_EXPECT *expect,
				              const MOCK_EXPECT *inputs)
{
    struct hostaddr_to_sockaddr_expectation *pe =
    (struct hostaddr_to_sockaddr_expectation *) expect;
    struct hostaddr_to_sockaddr_expectation *pi =
    (struct hostaddr_to_sockaddr_expectation *) inputs;

    return (strcmp(STR_OR_NULL(pe->hostaddr),
		   STR_OR_NULL(pi->hostaddr)) == 0
	    && strcmp(STR_OR_NULL(pe->service),
		      STR_OR_NULL(pi->service)) == 0
	    && pe->socktype == pi->socktype);
}

/* assign_hostaddr_to_sockaddr - assign expected output */

static void assign_hostaddr_to_sockaddr(const MOCK_EXPECT *expect,
					        void *targets)
{
    struct hostaddr_to_sockaddr_expectation *pe =
    (struct hostaddr_to_sockaddr_expectation *) expect;
    struct hostaddr_to_sockaddr_targets *pt =
    (struct hostaddr_to_sockaddr_targets *) targets;

    if (pe->retval == 0)
	*(pt->res) = copy_addrinfo(pe->res);
    *pt->retval = pe->retval;
}

/* print_hostaddr_to_sockaddr - print expected inputs */

static char *print_hostaddr_to_sockaddr(const MOCK_EXPECT *expect,
					        VSTRING *buf)
{
    struct hostaddr_to_sockaddr_expectation *pe =
    (struct hostaddr_to_sockaddr_expectation *) expect;

    vstring_sprintf(buf, "\"%s\", \"%s\", %d, (ptr)",
		    STR_OR_NULL(pe->hostaddr),
		    STR_OR_NULL(pe->service), pe->socktype);
    return (vstring_str(buf));
}

/* free_hostname_to_sockaddr_pf - destructor */

static void free_hostaddr_to_sockaddr(MOCK_EXPECT *expect)
{
    struct hostaddr_to_sockaddr_expectation *pe =
    (struct hostaddr_to_sockaddr_expectation *) expect;

    if (pe->hostaddr)
	myfree(pe->hostaddr);
    if (pe->service)
	myfree(pe->service);
    if (pe->retval == 0)
	freeaddrinfo(pe->res);
    pmock_expect_free(expect);
}

 /*
  * The mock name and its helper callbacks in one place.
  */
static const MOCK_APPL_SIG hostaddr_to_sockaddr_sig = {
    "hostaddr_to_sockaddr",
    match_hostaddr_to_sockaddr,
    assign_hostaddr_to_sockaddr,
    print_hostaddr_to_sockaddr,
    free_hostaddr_to_sockaddr,
};

/* _expect_hostaddr_to_sockaddr - set up expectation */

void    _expect_hostaddr_to_sockaddr(const char *file, int line,
				             int calls_expected, int retval,
				             const char *hostaddr,
				          const char *service, int socktype,
				             struct addrinfo *res)
{
    struct hostaddr_to_sockaddr_expectation *pe;

    pe = (struct hostaddr_to_sockaddr_expectation *)
	pmock_expect_create(&hostaddr_to_sockaddr_sig,
			    file, line, calls_expected, sizeof(*pe));
    pe->retval = retval;
    pe->hostaddr = MYSTRDUP_OR_NULL(hostaddr);
    pe->service = MYSTRDUP_OR_NULL(service);
    pe->socktype = socktype;
    if (pe->retval == 0)
	pe->res = copy_addrinfo(res);
}

/* hostaddr_to_sockaddr - mock hostaddr_to_sockaddr */

int     hostaddr_to_sockaddr(const char *hostaddr, const char *service,
			             int socktype, struct addrinfo **res)
{
    struct hostaddr_to_sockaddr_expectation inputs;
    struct hostaddr_to_sockaddr_targets targets;
    int     retval = EAI_FAIL;

    /*
     * Bundle the arguments to simplify handling.
     */
    inputs.hostaddr = (char *) hostaddr;
    inputs.service = (char *) service;
    inputs.socktype = socktype;

    targets.res = res;
    targets.retval = &retval;

    /*
     * TODO: bail out if there was no match?
     */
    (void) pmock_expect_apply(&hostaddr_to_sockaddr_sig,
			      &inputs.mock_expect, (void *) &targets);
    return (retval);
}

 /*
  * Manage sockaddr_to_hostaddr() expectations and responses. We use this
  * structure for deep copies of expect_sockaddr_to_hostaddr() expected
  * inputs and prepared responses, and for shallow copies of
  * sockaddr_to_hostaddr() inputs, so that we can reuse the
  * print_sockaddr_to_hostaddr() helper.
  */
struct sockaddr_to_hostaddr_expectation {
    MOCK_EXPECT mock_expect;		/* generic fields */
    int     retval;			/* result value */
    struct sockaddr_storage sa;		/* inputs */
    SOCKADDR_SIZE salen;
    int     socktype;
    MAI_HOSTADDR_STR *hostaddr;		/* other outputs */
    MAI_SERVPORT_STR *portnum;
    MAI_HOSTADDR_STR hostaddr_storage;
    MAI_SERVPORT_STR portnum_storage;
};

 /*
  * Pointers to sockaddr_to_hostaddr() outputs.
  */
struct sockaddr_to_hostaddr_targets {
    MAI_HOSTADDR_STR *hostaddr;
    MAI_SERVPORT_STR *portnum;
    int    *retval;
};

/* match_sockaddr_to_hostaddr - match inputs against expectation */

static int match_sockaddr_to_hostaddr(const MOCK_EXPECT *expect,
				              const MOCK_EXPECT *inputs)
{
    struct sockaddr_to_hostaddr_expectation *pe =
    (struct sockaddr_to_hostaddr_expectation *) expect;
    struct sockaddr_to_hostaddr_expectation *pi =
    (struct sockaddr_to_hostaddr_expectation *) inputs;

    return (pe->salen == pi->salen
	    && memcmp(&pe->sa, &pi->sa, pe->salen) == 0
	    && pe->socktype == pi->socktype);
}

/* assign_sockaddr_to_hostaddr - assign expected output */

static void assign_sockaddr_to_hostaddr(const MOCK_EXPECT *expect,
					        void *targets)
{
    struct sockaddr_to_hostaddr_expectation *pe =
    (struct sockaddr_to_hostaddr_expectation *) expect;
    struct sockaddr_to_hostaddr_targets *pt =
    (struct sockaddr_to_hostaddr_targets *) targets;

    if (pe->retval == 0) {
	if (pe->hostaddr && pt->hostaddr)
	    *pt->hostaddr = *pe->hostaddr;
	if (pe->portnum && pt->portnum)
	    *pt->portnum = *pe->portnum;
    }
    *pt->retval = pe->retval;
}

/* print_sockaddr_to_hostaddr - print expected inputs */

static char *print_sockaddr_to_hostaddr(const MOCK_EXPECT *expect,
					        VSTRING *buf)
{
    struct sockaddr_to_hostaddr_expectation *pe =
    (struct sockaddr_to_hostaddr_expectation *) expect;
    VSTRING *sockaddr_buf = vstring_alloc(100);

    vstring_sprintf(buf, "%s, %ld, (ptr), (ptr)",
		    sockaddr_to_string(sockaddr_buf,
				       (struct sockaddr *) &pe->sa,
				       pe->salen),
		    (long) pe->salen);
    vstring_free(sockaddr_buf);
    return (vstring_str(buf));
}

 /*
  * The mock name and its helper callbacks in one place.
  */
static const MOCK_APPL_SIG sockaddr_to_hostaddr_sig = {
    "sockaddr_to_hostaddr",
    match_sockaddr_to_hostaddr,
    assign_sockaddr_to_hostaddr,
    print_sockaddr_to_hostaddr,
    pmock_expect_free,
};

/* _expect_sockaddr_to_hostaddr - binary address to printable address form */

void    _expect_sockaddr_to_hostaddr(const char *file, int line,
				             int calls_expected, int retval,
				             const struct sockaddr *sa,
				             SOCKADDR_SIZE salen,
				             MAI_HOSTADDR_STR *hostaddr,
				             MAI_SERVPORT_STR *portnum,
				             int socktype)
{
    struct sockaddr_to_hostaddr_expectation *pe;

    pe = (struct sockaddr_to_hostaddr_expectation *)
	pmock_expect_create(&sockaddr_to_hostaddr_sig,
			    file, line, calls_expected, sizeof(*pe));
    pe->retval = retval;
    memcpy((void *) &pe->sa, (void *) sa, salen);
    pe->salen = salen;
    if (pe->retval == 0 && hostaddr) {
	*(pe->hostaddr = &pe->hostaddr_storage) = *hostaddr;
    } else {
	pe->hostaddr = 0;
    }
    if (pe->retval == 0 && portnum) {
	*(pe->portnum = &pe->portnum_storage) = *portnum;
    } else {
	pe->portnum = 0;
    }
    pe->socktype = socktype;
}

/* sockaddr_to_hostaddr - mock sockaddr_to_hostaddr */

int     sockaddr_to_hostaddr(const struct sockaddr *sa,
			             SOCKADDR_SIZE salen,
			             MAI_HOSTADDR_STR *hostaddr,
			             MAI_SERVPORT_STR *portnum,
			             int socktype)
{
    struct sockaddr_to_hostaddr_expectation inputs;
    struct sockaddr_to_hostaddr_targets targets;
    int     retval = EAI_FAIL;

    /*
     * Bundle the arguments to simplify handling.
     */
    memcpy((void *) &inputs.sa, (void *) sa, salen);
    inputs.salen = salen;
    inputs.socktype = socktype;

    targets.hostaddr = hostaddr;
    targets.portnum = portnum;
    targets.retval = &retval;

    /*
     * TODO: bail out if there was no match?
     */
    (void) pmock_expect_apply(&sockaddr_to_hostaddr_sig,
			      &inputs.mock_expect, (void *) &targets);
    return (retval);
}

 /*
  * Manage sockaddr_to_hostname() expectations and responses. We use this
  * structure for deep copies of expect_sockaddr_to_hostname() expected
  * inputs and prepared responses, and for shallow copies of
  * sockaddr_to_hostname() inputs, so that we can reuse the
  * print_sockaddr_to_hostname() helper.
  */
struct sockaddr_to_hostname_expectation {
    MOCK_EXPECT mock_expect;		/* generic fields */
    int     retval;			/* result value */
    struct sockaddr_storage sa;		/* inputs */
    SOCKADDR_SIZE salen;
    int     socktype;
    MAI_HOSTNAME_STR *hostname;		/* other outputs */
    MAI_SERVNAME_STR *service;
    MAI_HOSTNAME_STR hostname_storage;
    MAI_SERVNAME_STR service_storage;
};

 /*
  * Pointers to sockaddr_to_hostname() outputs.
  */
struct sockaddr_to_hostname_targets {
    MAI_HOSTNAME_STR *hostname;
    MAI_SERVNAME_STR *service;
    int    *retval;
};

/* match_sockaddr_to_hostname - match inputs against expectation */

static int match_sockaddr_to_hostname(const MOCK_EXPECT *expect,
				              const MOCK_EXPECT *inputs)
{
    struct sockaddr_to_hostname_expectation *pe =
    (struct sockaddr_to_hostname_expectation *) expect;
    struct sockaddr_to_hostname_expectation *pi =
    (struct sockaddr_to_hostname_expectation *) inputs;

    return (pe->salen == pi->salen
	    && memcmp(&pe->sa, &pi->sa, pe->salen) == 0
	    && pe->socktype == pi->socktype);
}

/* assign_sockaddr_to_hostname - assign expected output */

static void assign_sockaddr_to_hostname(const MOCK_EXPECT *expect,
					        void *targets)
{
    struct sockaddr_to_hostname_expectation *pe =
    (struct sockaddr_to_hostname_expectation *) expect;
    struct sockaddr_to_hostname_targets *pt =
    (struct sockaddr_to_hostname_targets *) targets;

    if (pe->retval == 0) {
	if (pe->hostname && pt->hostname)
	    *pt->hostname = *pe->hostname;
	if (pe->service && pt->service)
	    *pt->service = *pe->service;
    }
    *pt->retval = pe->retval;
}

/* print_sockaddr_to_hostname - print expected inputs */

static char *print_sockaddr_to_hostname(const MOCK_EXPECT *expect,
					        VSTRING *buf)
{
    struct sockaddr_to_hostname_expectation *pe =
    (struct sockaddr_to_hostname_expectation *) expect;
    VSTRING *sockaddr_buf = vstring_alloc(100);

    vstring_sprintf(buf, "%s, %ld, (ptr), (ptr)",
		    sockaddr_to_string(sockaddr_buf,
				       (struct sockaddr *) &pe->sa,
				       pe->salen),
		    (long) pe->salen);
    vstring_free(sockaddr_buf);
    return (vstring_str(buf));
}

 /*
  * The mock name and its helper callbacks in one place.
  */
static const MOCK_APPL_SIG sockaddr_to_hostname_sig = {
    "sockaddr_to_hostname",
    match_sockaddr_to_hostname,
    assign_sockaddr_to_hostname,
    print_sockaddr_to_hostname,
    pmock_expect_free,
};

/* _expect_sockaddr_to_hostname - set up expectations */

void    _expect_sockaddr_to_hostname(const char *file, int line,
				             int calls_expected, int retval,
				             const struct sockaddr *sa,
				             SOCKADDR_SIZE salen,
				             MAI_HOSTNAME_STR *hostname,
				             MAI_SERVNAME_STR *service,
				             int socktype)
{
    struct sockaddr_to_hostname_expectation *pe;

    pe = (struct sockaddr_to_hostname_expectation *)
	pmock_expect_create(&sockaddr_to_hostname_sig,
			    file, line, calls_expected, sizeof(*pe));
    pe->retval = retval;
    memcpy((void *) &pe->sa, (void *) sa, salen);
    pe->salen = salen;
    if (retval == 0 && hostname) {
	*(pe->hostname = &pe->hostname_storage) = *hostname;
    } else {
	pe->hostname = 0;
    }
    if (retval == 0 && service) {
	*(pe->service = &pe->service_storage) = *service;
    } else {
	pe->service = 0;
    }
    pe->socktype = socktype;
}

/* sockaddr_to_hostname - mock sockaddr_to_hostname */

int     sockaddr_to_hostname(const struct sockaddr *sa,
			             SOCKADDR_SIZE salen,
			             MAI_HOSTNAME_STR *hostname,
			             MAI_SERVNAME_STR *service,
			             int socktype)
{
    struct sockaddr_to_hostname_expectation inputs;
    struct sockaddr_to_hostname_targets targets;
    int     retval = EAI_FAIL;

    /*
     * Bundle the arguments to simplify handling.
     */
    memcpy((void *) &inputs.sa, (void *) sa, salen);
    inputs.salen = salen;
    inputs.socktype = socktype;

    targets.hostname = hostname;
    targets.service = service;
    targets.retval = &retval;

    /*
     * TODO: bail out if there was no match?
     */
    (void) pmock_expect_apply(&sockaddr_to_hostname_sig,
			      &inputs.mock_expect, (void *) &targets);
    return (retval);
}
