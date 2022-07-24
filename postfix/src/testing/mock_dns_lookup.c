/*++
/* NAME
/*	mock_dns_lookup 3
/* SUMMARY
/*	dns_lookup mock for hermetic tests
/* SYNOPSIS
/*	#include <mock_dns_lookup.h>
/*
/*	int	dns_lookup_x(
/*	const char *name,
/*	unsigned type,
/*	unsigned rflags,
/*	DNS_RR	**list,
/*	VSTRING	*fqdn,
/*	VSTRING	*why,
/*	int	*rcode,
/*	unsigned lflags)
/*
/*	int	 dns_get_h_errno(void)
/*
/*	void	 dns_set_h_errno(int herrval)
/* EXPECTATION SETUP
/*	void	expect_dns_lookup_x(
/*	int	calls_expected,
/*	int	herrval,
/*	int	retval,
/*	const char *name,
/*	unsigned type,
/*	unsigned flags,
/*	DNS_RR	*rrlist,
/*	VSTRING	*fqdn,
/*	VSTRING	*why,
/*	int	rcode,
/*	unsigned lflags)
/*
/*	DNS_RR	*make_dns_rr(
/*	const char *qname,
/*	const char *rname,
/*	unsigned type,
/*	unsigned class,
/*	unsigned ttl,
/*	unsigned dnssec_valid,
/*	unsigned pref,
/*	void	*data,
/*	size_t	data_len)
/* MATCHERS
/*	int	eq_dns_rr(
/*	PTEST_CTX *t,
/*	const char *what,
/*	DNS_RR	*got,
/*	DNS_RR	*want)
/* OTHER HELPERS
/*	const char *dns_status_to_string(int status)
/* DESCRIPTION
/*	This module implements a mock dns_lookup_x() lookup function
/*	that produces prepared outputs in response to expected
/*	inputs. This supports hermetic tests, i.e. tests that do
/*	not depend on host configuration or on network access.
/*
/*	expect_dns_lookup_x() makes deep copies of its input
/*	arguments, and of the arguments that specify prepared
/*	outputs. The herrval argument specifies a prepared
/*	dns_get_h_errno() result value, and the retval argument
/*	specifies a prepared dns_lookup_x() result value. The
/*	calls_expected argument specifies the expected number of
/*	dns_lookup_x() calls (zero means one or more calls, not:
/*	zero calls).
/*
/*	dns_get_h_errno() returns an error value that is configured
/*	with expect_dns_lookup_x(), and that is assigned when
/*	dns_lookup_x() or dns_set_h_errno() are called.
/*
/*	dns_set_h_errno() assigns the dns_get_h_errno() result
/*	value.
/*
/*	make_dns_rr() is a wrapper around dns_rr_create() that also
/*	controls the dnssec_valid flag.
/*
/*	eq_dns_rr() is a predicate that compares its arguments
/*	(linked lists) for equality. If t is not null, the what
/*	argument is used in logging when the inputs differ.
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

 /*
  * Utility library.
  */
#include <mymalloc.h>
#include <msg.h>
#include <vstring.h>
#include <stringops.h>
#include <hex_code.h>
#include <name_code.h>

 /*
  * DNS library.
  */
#include <dns.h>

 /*
  * Test library.
  */
#include <mock_dns.h>
#include <pmock_expect.h>
#include <ptest.h>

 /*
  * Helpers.
  */
#define MYSTRDUP_OR_NULL(x)		((x) ? mystrdup(x) : 0)
#define VSTRDUP_OR_NULL(x) \
    ((x) ? vstring_strcpy(vstring_alloc(VSTRING_LEN(x)), vstring_str(x)) : 0)

#define STR(x)				vstring_str(x)

#define STR_OR_NULL(s)			((s) ? (s) : "(null)")
#define VSTR_STR_OR_NULL(s)		((s) ? STR(s) : "(null)")

 /*
  * Local state for the mock functions dns_get_herrno() and dns_set_herrno(),
  * also updated when the mock function dns_lookup_x() is called.
  * 
  * XXX This could leak information when tests are run successively in the same
  * process. Should the test infrastructure fork() the process before each
  * test? Leakage will not happen when each test calls a function that resets
  * global_herrval.
  */
static int global_herrval = ~0;

/* dns_status_to_string - convert status code to string */

const char *dns_status_to_string(int status)
{
    static const NAME_CODE status_string[] = {
	"DNS_OK", DNS_OK,
	"DNS_POLICY", DNS_POLICY,
	"DNS_RETRY", DNS_RETRY,
	"DNS_INVAL", DNS_INVAL,
	"DNS_FAIL", DNS_FAIL,
	"DNS_NULLMX", DNS_NULLMX,
	"DNS_NOTFOUND", DNS_NOTFOUND,
	0,
    };

    return (str_name_code(status_string, status));
}

/* copy_dns_rrlist - deep copy */

static DNS_RR *copy_dns_rrlist(DNS_RR *list)
{
    DNS_RR *rr;

    if (list == 0)
	return (0);
    rr = dns_rr_copy(list);
    rr->next = copy_dns_rrlist(list->next);
    return (rr);
}

/* make_dns_rr - dns_rr_create() wrapper */

DNS_RR *make_dns_rr(const char *qname, const char *rname, unsigned type,
		            unsigned class, unsigned ttl,
		            unsigned dnssec_valid, unsigned pref,
		            void *data, size_t data_len)
{
    DNS_RR *rr;

    rr = dns_rr_create(qname, rname, type, class, ttl, pref, data, data_len);
    rr->dnssec_valid = dnssec_valid;
    return (rr);
}

/* _eq_dns_rr - equality predicate */

int     _eq_dns_rr(PTEST_CTX *t, const char *file, int line,
		           const char *what,
		           DNS_RR *got, DNS_RR *want)
{
    if (got == 0 && want == 0) {
	return (1);
    }
    if (got == 0 || want == 0) {
	if (t)
	    ptest_error(t, "%s:%d %s: got %s, want %s",
			file, line, what, got ? "(DNS_RR *)" : "(null)",
			want ? "(DNS_RR *)" : "(null)");
	return (0);
    }
    if (strcmp(got->qname, want->qname) != 0) {
	if (t)
	    ptest_error(t, "%s:%d %s: got qname '%s', want '%s'",
			file, line, what, got->qname, want->qname);
	return (0);
    }
    if (strcmp(got->rname, want->rname) != 0) {
	if (t)
	    ptest_error(t, "%s:%d %s: got rname '%s', want '%s'",
			file, line, what, got->rname, want->rname);
	return (0);
    }
    if (got->type != want->type) {
	if (t)
	    ptest_error(t, "%s:%d %s: got type %d, want %d",
			file, line, what, got->type, want->type);
	return (0);
    }
    if (got->class != want->class) {
	if (t)
	    ptest_error(t, "%s:%d %s: got class %d, want %d",
			file, line, what, got->class, want->class);
	return (0);
    }
    if (got->ttl != want->ttl) {
	if (t)
	    ptest_error(t, "%s:%d %s: got ttl %d, want %d",
			file, line, what, got->ttl, want->ttl);
	return (0);
    }
    if (got->dnssec_valid != want->dnssec_valid) {
	if (t)
	    ptest_error(t, "%s:%d %s: got dnssec_valid %d, want %d",
		   file, line, what, got->dnssec_valid, want->dnssec_valid);
	return (0);
    }
    if (got->pref != want->pref) {
	if (t)
	    ptest_error(t, "%s:%d %s: got pref %d, want %d",
			file, line, what, got->pref, want->pref);
	return (0);
    }
    if (got->data_len != want->data_len) {
	if (t)
	    ptest_error(t, "%s:%d %s: got data_len %d, want %d",
	       file, line, what, (int) got->data_len, (int) want->data_len);
	return (0);
    }
    if (memcmp(got->data, want->data, got->data_len) != 0) {
	VSTRING *got_data_hex = vstring_alloc(100);
	VSTRING *want_data_hex = vstring_alloc(100);

	if (t)
	    ptest_error(t, "%s:%d %s: got data %s, want %s",
	       file, line, what, STR(hex_encode_opt(got_data_hex, got->data,
				 got->data_len, HEX_ENCODE_FLAG_USE_COLON)),
			STR(hex_encode_opt(want_data_hex, want->data,
			       want->data_len, HEX_ENCODE_FLAG_USE_COLON)));
	vstring_free(got_data_hex);
	vstring_free(want_data_hex);
	return (0);
    }
    return (_eq_dns_rr(t, file, line, what, got->next, want->next));
}

 /*
  * Manage dns_lookup_x() expectations and responses. We use this structure
  * for deep copies of expect_dns_lookup_x() expected inputs and prepared
  * responses, and for shallow copies of dns_lookup_x() inputs.
  */
struct dns_lookup_x_expectation {
    MOCK_EXPECT mock_expect;		/* generic fields */
    int     herrval;			/* h_errno value */
    int     retval;			/* result value */
    char   *name;			/* inputs */
    unsigned type;
    unsigned flags;
    unsigned lflags;
    DNS_RR *rrlist;			/* outputs */
    VSTRING *fqdn;
    VSTRING *why;
    int     rcode;
};

 /*
  * Pointers to dns_lookup_x() outputs.
  */
struct dns_lookup_x_targets {
    int    *herrval;
    int    *retval;
    DNS_RR **rrlist;
    VSTRING *fqdn;
    VSTRING *why;
    int    *rcode;
};

/* match_dns_lookup_x - match inputs against expectation */

static int match_dns_lookup_x(const MOCK_EXPECT *expect,
			              const MOCK_EXPECT *inputs)
{
    struct dns_lookup_x_expectation *pe =
    (struct dns_lookup_x_expectation *) expect;
    struct dns_lookup_x_expectation *pi =
    (struct dns_lookup_x_expectation *) inputs;

    return (strcmp(STR_OR_NULL(pe->name),
		   STR_OR_NULL(pi->name)) == 0
	    && pe->type == pi->type
	    && pe->flags == pi->flags
	    && pe->lflags == pi->lflags);
}

/* assign_dns_lookup_x - assign expected output */

static void assign_dns_lookup_x(const MOCK_EXPECT *expect,
				        void *targets)
{
    struct dns_lookup_x_expectation *pe =
    (struct dns_lookup_x_expectation *) expect;
    struct dns_lookup_x_targets *pt =
    (struct dns_lookup_x_targets *) targets;

    if (pe->retval == DNS_OK) {
	if (pt->rrlist)
	    *(pt->rrlist) = copy_dns_rrlist(pe->rrlist);
	if (pt->fqdn && pe->fqdn)
	    vstring_strcpy(pt->fqdn, STR(pe->fqdn));
    } else {
	if (pt->why && pe->why)
	    vstring_strcpy(pt->why, STR(pe->why));
    }
    if (pt->rcode)
	*pt->rcode = pe->rcode;
    *pt->retval = pe->retval;
    *pt->herrval = pe->herrval;
}

/* print_dns_lookup_x - print expected inputs */

static char *print_dns_lookup_x(const MOCK_EXPECT *expect,
				        VSTRING *buf)
{
    struct dns_lookup_x_expectation *pe =
    (struct dns_lookup_x_expectation *) expect;

    vstring_sprintf(buf, "\"%s\", %s, %d, (ptr), (ptr), (ptr), (ptr), %d",
		    STR_OR_NULL(pe->name), dns_strtype(pe->type),
		    pe->flags, pe->lflags);
    return (STR(buf));
}

/* free_dns_lookup_x - destructor */

static void free_dns_lookup_x(MOCK_EXPECT *expect)
{
    struct dns_lookup_x_expectation *pe =
    (struct dns_lookup_x_expectation *) expect;

    myfree(pe->name);
    if (pe->retval == DNS_OK) {
	if (pe->rrlist)
	    dns_rr_free(pe->rrlist);
	if (pe->fqdn)
	    vstring_free(pe->fqdn);
    } else {
	if (pe->why)
	    vstring_free(pe->why);
    }
    pmock_expect_free(expect);
}

 /*
  * The mock name and its helper callbacks in one place.
  */
static const MOCK_APPL_SIG dns_lookup_x_sig = {
    "dns_lookup_x",
    match_dns_lookup_x,
    assign_dns_lookup_x,
    print_dns_lookup_x,
    free_dns_lookup_x,
};

/* _expect_dns_lookup_x - set up expectation */

void    _expect_dns_lookup_x(const char *file, int line, int calls_expected,
			             int herrval, int retval,
		            const char *name, unsigned type, unsigned flags,
			        DNS_RR *rrlist, VSTRING *fqdn, VSTRING *why,
			             int rcode, unsigned lflags)
{
    struct dns_lookup_x_expectation *pe;

    pe = (struct dns_lookup_x_expectation *)
	pmock_expect_create(&dns_lookup_x_sig,
			    file, line, calls_expected, sizeof(*pe));

    /*
     * Inputs.
     */
    pe->name = MYSTRDUP_OR_NULL(name);
    pe->type = type;
    pe->flags = flags;
    pe->lflags = lflags;

    /*
     * Outputs.
     */
    pe->herrval = herrval;
    pe->retval = retval;
    if (pe->retval == DNS_OK) {
	pe->rrlist = copy_dns_rrlist(rrlist);
	pe->fqdn = VSTRDUP_OR_NULL(fqdn);
    } else {
	pe->why = VSTRDUP_OR_NULL(why);
    }
    pe->rcode = rcode;
}

/* dns_lookup_x - answer the call with prepared responses */

int     dns_lookup_x(const char *name, unsigned type, unsigned flags,
		             DNS_RR **rrlist, VSTRING *fqdn, VSTRING *why,
		             int *rcode, unsigned lflags)
{
    struct dns_lookup_x_expectation inputs;
    struct dns_lookup_x_targets targets;
    int     retval = DNS_FAIL;

    /*
     * Bundle the arguments to simplify handling.
     */
    inputs.name = (char *) name;
    inputs.type = type;
    inputs.flags = flags;
    inputs.lflags = lflags;

    targets.herrval = &global_herrval;
    targets.retval = &retval;
    targets.rrlist = rrlist;
    targets.fqdn = fqdn;
    targets.why = why;
    targets.rcode = rcode;

    /*
     * Aargh.
     */
    if (rrlist)
	*rrlist = 0;

    /*
     * TODO: bail out if there was no match?
     */
    (void) pmock_expect_apply(&dns_lookup_x_sig,
			      &inputs.mock_expect, (void *) &targets);
    return (retval);
}

/* dns_get_h_errno - return prepared answer */

int     dns_get_h_errno(void)
{
    return (global_herrval);
}

/* dns_set_h_errno - return prepared answer */

void    dns_set_h_errno(int herrval)
{
    global_herrval = herrval;
}
