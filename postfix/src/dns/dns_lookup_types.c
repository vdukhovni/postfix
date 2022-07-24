/*++
/* NAME
/*	dns_lookup_types 3
/* SUMMARY
/*	domain name service lookup for multiple types
/* SYNOPSIS
/*	#include <dns.h>
/*
/*	int	dns_lookup_l(name, rflags, list, fqdn, why, lflags, ...)
/*	const char *name;
/*	unsigned rflags;
/*	DNS_RR	**list;
/*	VSTRING *fqdn;
/*	VSTRING *why;
/*	int	lflags;
/*
/*	int	dns_lookup_v(name, rflags, list, fqdn, why, lflags, ltype)
/*	const char *name;
/*	unsigned rflags;
/*	DNS_RR	**list;
/*	VSTRING *fqdn;
/*	VSTRING *why;
/*	int	lflags;
/*	unsigned *ltype;
/* AUXILIARY FUNCTIONS
/*	int	dns_lookup_rl(name, rflags, list, fqdn, why, rcode, lflags, ...)
/*	const char *name;
/*	unsigned rflags;
/*	DNS_RR	**list;
/*	VSTRING *fqdn;
/*	VSTRING *why;
/*	int	*rcode;
/*	int	lflags;
/*
/*	int	dns_lookup_rv(name, rflags, list, fqdn, why, rcode, lflags,
/*				ltype)
/*	const char *name;
/*	unsigned rflags;
/*	DNS_RR	**list;
/*	VSTRING *fqdn;
/*	VSTRING *why;
/*	int	*rcode;
/*	int	lflags;
/*	unsigned *ltype;
/* DESCRIPTION
/*	These functions iterate over a sequence of unsigned resource
/*	types, call dns_lookup_x() for each type, and carefully
/*	aggregate the resulting error and non-error results.
/*
/*	dns_lookup_rl() and dns_lookup_l() iterate over a variadic
/*	list of query types, while dns_lookup_rv() and dns_lookup_v()
/*	iterate over a vector of query types.
/* DIAGNOSTICS
/* SEE ALSO
/*	dns_lookup(3), domain name service lookup
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
/*	Google, Inc.
/*	111 8th Avenue
/*	New York, NY 10011, USA
/*--*/

/* System library. */

#include <sys_defs.h>

/* Utility library. */

#include <vstring.h>
#include <msg.h>

/* DNS library. */

#define LIBDNS_INTERNAL
#include <dns.h>

 /*
  * KISS memory management.
  */
#define MAX_TYPE	10

/* dns_lookup_rl - DNS lookup interface with types list */

int     dns_lookup_rl(const char *name, unsigned flags, DNS_RR **rrlist,
		              VSTRING *fqdn, VSTRING *why, int *rcode,
		              int lflags,...)
{
    va_list ap;
    unsigned type, types[MAX_TYPE];
    int     count = 0;

    va_start(ap, lflags);
    for (type = va_arg(ap, unsigned); type != 0; type = va_arg(ap, unsigned)) {
	if (count >= MAX_TYPE - 1)
	    msg_panic("dns_lookup_rl: too many types");
	types[count++] = type;
    }
    types[count] = 0;
    va_end(ap);
    return (dns_lookup_rv(name, flags, rrlist, fqdn, why, rcode, lflags, types));
}

/* dns_lookup_rv - DNS lookup interface with types vector */

int     dns_lookup_rv(const char *name, unsigned flags, DNS_RR **rrlist,
		              VSTRING *fqdn, VSTRING *why, int *rcode,
		              int lflags, const unsigned *types)
{
    unsigned type, next;
    int     status = DNS_NOTFOUND;
    int     hpref_status = INT_MIN;
    VSTRING *hpref_rtext = 0;
    int     hpref_rcode;
    int     hpref_h_errno;
    DNS_RR *rr;

    /* Save intermediate highest-priority result. */
#define SAVE_HPREF_STATUS() do { \
	hpref_status = status; \
	if (rcode) \
	    hpref_rcode = *rcode; \
	if (why && status != DNS_OK) \
	    vstring_strcpy(hpref_rtext ? hpref_rtext : \
			   (hpref_rtext = vstring_alloc(VSTRING_LEN(why))), \
			   vstring_str(why)); \
	hpref_h_errno = dns_get_h_errno(); \
    } while (0)

    /* Restore intermediate highest-priority result. */
#define RESTORE_HPREF_STATUS() do { \
	status = hpref_status; \
	if (rcode) \
	    *rcode = hpref_rcode; \
	if (why && status != DNS_OK) \
	    vstring_strcpy(why, vstring_str(hpref_rtext)); \
	dns_set_h_errno(hpref_h_errno); \
    } while (0)

    if (rrlist)
	*rrlist = 0;
    for (type = *types++; type != 0; type = next) {
	next = *types++;
	if (msg_verbose)
	    msg_info("lookup %s type %s flags %s",
		     name, dns_strtype(type), dns_str_resflags(flags));
	status = dns_lookup_x(name, type, flags, rrlist ? &rr : (DNS_RR **) 0,
			      fqdn, why, rcode, lflags);
	if (rrlist && rr)
	    *rrlist = dns_rr_append(*rrlist, rr);
	if (status == DNS_OK) {
	    if (lflags & DNS_REQ_FLAG_STOP_OK)
		break;
	} else if (status == DNS_INVAL) {
	    if (lflags & DNS_REQ_FLAG_STOP_INVAL)
		break;
	} else if (status == DNS_POLICY) {
	    if (type == T_MX && (lflags & DNS_REQ_FLAG_STOP_MX_POLICY))
		break;
	} else if (status == DNS_NULLMX) {
	    if (lflags & DNS_REQ_FLAG_STOP_NULLMX)
		break;
	}
	/* XXX Stop after NXDOMAIN error. */
	if (next == 0)
	    break;
	if (status >= hpref_status)
	    SAVE_HPREF_STATUS();		/* save last info */
    }
    if (status < hpref_status)
	RESTORE_HPREF_STATUS();			/* else report last info */
    if (hpref_rtext)
	vstring_free(hpref_rtext);
    return (status);
}
