#ifndef _MOCK_DNS_H_INCLUDED_
#define _MOCK_DNS_H_INCLUDED_

/*++
/* NAME
/*	mock_dns 3h
/* SUMMARY
/*	emulate DNS support for hermetic tests
/* SYNOPSIS
/*	#include <mock_dns.h>
/* DESCRIPTION
/* .nf

 /*
  * DNS library.
  */
#include <dns.h>

 /*
  * Test library.
  */
#include <ptest.h>

 /*
  * Manage expectations and responses. Capture the source file name and line
  * number for better diagnostics.
  */
#define expect_dns_lookup_x(exp_calls, herrval, retval, name, type, rflags, \
				list, fqdn, why, rcode, lflags) \
	_expect_dns_lookup_x(__FILE__, __LINE__, (exp_calls), (herrval), \
				(retval), (name), (type), (rflags), (list), \
				(fqdn), (why), (rcode), (lflags))

extern void _expect_dns_lookup_x(const char *, int, int, int, int,
			         const char *, unsigned, unsigned, DNS_RR *,
			               VSTRING *, VSTRING *, int, unsigned);

 /*
  * Matcher predicates. Capture the source file name and line number for
  * better diagnostics.
  */
#define eq_dns_rr(t, what, got, want) _eq_dns_rr((t), __FILE__, __LINE__, \
				(what), (got), (want))

extern int _eq_dns_rr(PTEST_CTX *, const char *, int, const char *, DNS_RR *, DNS_RR *);

 /*
  * Helper to create test data.
  */
extern DNS_RR *make_dns_rr(const char *, const char *, unsigned, unsigned,
			           unsigned, unsigned, unsigned,
			           void *, size_t);

 /*
  * Other helper.
  */
extern const char *dns_status_to_string(int);

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

#endif
