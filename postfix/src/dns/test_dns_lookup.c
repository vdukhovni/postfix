/*++
/* NAME
/*	test_dns_lookup 1
/* SUMMARY
/*	DNS lookup test program
/* SYNOPSIS
/*	test_dns_lookup query-type domain-name
/* DESCRIPTION
/*	test_dns_lookup performs a DNS query of the specified resource
/*	type for the specified resource name.
/* DIAGNOSTICS
/*	Problems are reported to the standard error stream.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include <sys_defs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>

/* Utility library. */

#include <vstring.h>
#include <msg.h>
#include <msg_vstream.h>
#include <mymalloc.h>
#include <argv.h>

/* Application-specific. */

#include "dns.h"

static void print_rr(VSTRING *buf, DNS_RR *rr)
{
    while (rr) {
	vstream_printf("ad: %u, rr: %s\n",
		       rr->dnssec_valid, dns_strrecord(buf, rr));
	rr = rr->next;
    }
}

static NORETURN usage(char **argv)
{
    msg_fatal("usage: %s [-npv] [-f filter] types name", argv[0]);
}

int     main(int argc, char **argv)
{
    ARGV   *types_argv;
    unsigned type;
    char   *name;
    VSTRING *fqdn = vstring_alloc(100);
    VSTRING *why = vstring_alloc(100);
    VSTRING *buf;
    int     rcode;
    DNS_RR *rr;
    int     i;
    int     ch;
    int     status;
    int     lflags = 0;

    msg_vstream_init(argv[0], VSTREAM_ERR);
    while ((ch = GETOPT(argc, argv, "f:npv")) > 0) {
	switch (ch) {
	case 'v':
	    msg_verbose++;
	    break;
	case 'f':
	    dns_rr_filter_compile("DNS reply filter", optarg);
	    break;
	case 'n':
	    lflags |= DNS_REQ_FLAG_NCACHE_TTL;
	    break;
	case 'p':
	    dns_ncache_ttl_fix_enable = 1;
	    break;
	default:
	    usage(argv);
	}
    }
    if (argc != optind + 2)
	usage(argv);
    buf = vstring_alloc(100);
    types_argv = argv_split(argv[optind], CHARS_COMMA_SP);
    for (i = 0; i < types_argv->argc; i++) {
	if ((type = dns_type(types_argv->argv[i])) == 0)
	    msg_fatal("invalid query type: %s", types_argv->argv[i]);
	name = argv[optind + 1];
	msg_verbose = 1;
	status = dns_lookup_x(name, type, RES_USE_DNSSEC, &rr, fqdn, why,
			      &rcode, lflags);
	if (status != DNS_OK)
	    msg_warn("%s (rcode=%d)", vstring_str(why), rcode);
	if (rr) {
	    vstream_printf("%s: fqdn: %s\n", name, vstring_str(fqdn));
	    print_rr(buf, rr);
	    dns_rr_free(rr);
	}
	vstream_fflush(VSTREAM_OUT);
    }
    argv_free(types_argv);
    vstring_free(buf);
    exit(0);
}
