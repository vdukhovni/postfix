/*++
/* NAME
/*	dns_strtype 3
/* SUMMARY
/*	name service resource record printable forms
/* SYNOPSIS
/*	#include <dns.h>
/*
/*	char	*dns_strrecord(buf, record)
/*	VSTRING	*buf;
/*	DNS_RR	*record;
/* DESCRIPTION
/*	dns_strrecord() formats a DNS resource record as "name ttl
/*	class type preference value", where the class field is
/*	always "IN", the preference field exists only for MX records,
/*	and all names end in ".". The result value is the payload
/*	of the buffer argument.
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

/* Utility library. */

#include <vstring.h>
#include <msg.h>

/* DNS library. */

#include <dns.h>

/* dns_strrecord - format resource record as generic string */

char   *dns_strrecord(VSTRING *buf, DNS_RR *rr)
{
    const char myname[] = "dns_strrecord";
    MAI_HOSTADDR_STR host;

    vstring_sprintf(buf, "%s. %u IN %s ",
		    rr->rname, rr->ttl, dns_strtype(rr->type));
    switch (rr->type) {
    case T_A:
#ifdef T_AAAA
    case T_AAAA:
#endif
	if (dns_rr_to_pa(rr, &host) == 0)
	    msg_fatal("%s: conversion error for resource record type %s: %m",
		      myname, dns_strtype(rr->type));
	vstring_sprintf_append(buf, "%s", host.buf);
	break;
    case T_CNAME:
    case T_DNAME:
    case T_MB:
    case T_MG:
    case T_MR:
    case T_NS:
    case T_PTR:
    case T_TXT:
	vstring_sprintf_append(buf, "%s.", rr->data);
	break;
    case T_MX:
	vstring_sprintf_append(buf, "%u %s.", rr->pref, rr->data);
	break;
    case T_TLSA:
	if (rr->data_len >= 3) {
	    uint8_t *ip = (uint8_t *) rr->data;
	    uint8_t usage = *ip++;
	    uint8_t selector = *ip++;
	    uint8_t mtype = *ip++;
	    unsigned i;

	    /* /\.example\. \d+ IN TLSA \d+ \d+ \d+ [\da-f]*$/ IGNORE */
	    vstring_sprintf_append(buf, "%d %d %d ", usage, selector, mtype);
	    for (i = 3; i < rr->data_len; ++i)
		vstring_sprintf_append(buf, "%02x", *ip++);
	} else {
	    vstring_sprintf_append(buf, "[truncated record]");
	}
	break;
    default:
	msg_fatal("%s: don't know how to print type %s",
		  myname, dns_strtype(rr->type));
    }
    return (vstring_str(buf));
}
