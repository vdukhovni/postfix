/*++
/* NAME
/*	dns_rr 3
/* SUMMARY
/*	resource record memory and list management
/* SYNOPSIS
/*	#include <dns.h>
/*
/*	DNS_RR	*dns_rr_create(qname, rname, type, class, ttl, preference,
/*				data, data_len)
/*	const char *qname;
/*	const char *rname;
/*	unsigned short type;
/*	unsigned short class;
/*	unsigned int ttl;
/*	unsigned preference;
/*	const char *data;
/*	size_t data_len;
/*
/*	void	dns_rr_free(list)
/*	DNS_RR	*list;
/*
/*	DNS_RR	*dns_rr_copy(record)
/*	DNS_RR	*record;
/*
/*	DNS_RR	*dns_rr_append(list, record)
/*	DNS_RR	*list;
/*	DNS_RR	*record;
/*
/*	DNS_RR	*dns_rr_sort(list, compar)
/*	DNS_RR	*list
/*	int	(*compar)(DNS_RR *, DNS_RR *);
/*
/*	int	dns_rr_compare_pref_ipv6(DNS_RR *a, DNS_RR *b)
/*	DNS_RR	*list
/*	DNS_RR	*list
/*
/*	int	dns_rr_compare_pref_ipv4(DNS_RR *a, DNS_RR *b)
/*	DNS_RR	*list
/*	DNS_RR	*list
/*
/*	int	dns_rr_compare_pref_any(DNS_RR *a, DNS_RR *b)
/*	DNS_RR	*list
/*	DNS_RR	*list
/*
/*	DNS_RR	*dns_rr_shuffle(list)
/*	DNS_RR	*list;
/*
/*	DNS_RR	*dns_rr_remove(list, record)
/*	DNS_RR	*list;
/*	DNS_RR	*record;
/*
/*	int	var_dns_rr_list_limit;
/* DESCRIPTION
/*	The routines in this module maintain memory for DNS resource record
/*	information, and maintain lists of DNS resource records.
/*
/*	dns_rr_create() creates and initializes one resource record.
/*	The \fIqname\fR field specifies the query name.
/*	The \fIrname\fR field specifies the reply name.
/*	\fIpreference\fR is used for MX records; \fIdata\fR is a null
/*	pointer or specifies optional resource-specific data;
/*	\fIdata_len\fR is the amount of resource-specific data.
/*
/*	dns_rr_free() releases the resource used by of zero or more
/*	resource records.
/*
/*	dns_rr_copy() makes a copy of a resource record.
/*
/*	dns_rr_append() appends an input resource record list to
/*	an output list. Null arguments are explicitly allowed.
/*	When the result would be longer than var_dns_rr_list_limit
/*	(default: 100), dns_rr_append() logs a warning, flags the
/*	output list as truncated, and discards the excess elements.
/*	Once an output list is flagged as truncated (test with
/*	DNS_RR_IS_TRUNCATED()), the caller is expected to stop
/*	trying to append records to that list. Note: the 'truncated'
/*	flag is transitive, i.e. when appending a input list that
/*	was flagged as truncated to an output list, the output list
/*	will also be flagged as truncated.
/*
/*	dns_rr_sort() sorts a list of resource records into ascending
/*	order according to a user-specified criterion. The result is the
/*	sorted list.
/*
/*	dns_rr_compare_pref_XXX() are dns_rr_sort() helpers to sort
/*	records by their MX preference and by their address family.
/*
/*	dns_rr_shuffle() randomly permutes a list of resource records.
/*
/*	dns_rr_remove() removes the specified record from the specified list.
/*	The updated list is the result value.
/*	The record MUST be a list member.
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
#include <string.h>
#include <stdlib.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <myrand.h>

/* DNS library. */

#include "dns.h"

 /*
  * A generous safety limit for the number of DNS resource records that the
  * Postfix DNS client library will admit into a list. The default value 100
  * is 20x the default limit on the number address records that the Postfix
  * SMTP client is willing to consider.
  * 
  * Mutable, to make code testable.
  */
int     var_dns_rr_list_limit = 100;

/* dns_rr_create - fill in resource record structure */

DNS_RR *dns_rr_create(const char *qname, const char *rname,
		              ushort type, ushort class,
		              unsigned int ttl, unsigned pref,
		              const char *data, size_t data_len)
{
    DNS_RR *rr;

    rr = (DNS_RR *) mymalloc(sizeof(*rr) + data_len - 1);
    rr->qname = mystrdup(qname);
    rr->rname = mystrdup(rname);
    rr->type = type;
    rr->class = class;
    rr->ttl = ttl;
    rr->dnssec_valid = 0;
    rr->pref = pref;
    if (data && data_len > 0)
	memcpy(rr->data, data, data_len);
    rr->data_len = data_len;
    rr->next = 0;
    rr->flags = 0;
    return (rr);
}

/* dns_rr_free - destroy resource record structure */

void    dns_rr_free(DNS_RR *rr)
{
    if (rr) {
	if (rr->next)
	    dns_rr_free(rr->next);
	myfree(rr->qname);
	myfree(rr->rname);
	myfree((void *) rr);
    }
}

/* dns_rr_copy - copy resource record */

DNS_RR *dns_rr_copy(DNS_RR *src)
{
    ssize_t len = sizeof(*src) + src->data_len - 1;
    DNS_RR *dst;

    /*
     * Combine struct assignment and data copy in one block copy operation.
     */
    dst = (DNS_RR *) mymalloc(len);
    memcpy((void *) dst, (void *) src, len);
    dst->qname = mystrdup(src->qname);
    dst->rname = mystrdup(src->rname);
    dst->next = 0;
    return (dst);
}

/* dns_rr_append_with_limit - append resource record to limited list */

static void dns_rr_append_with_limit(DNS_RR *list, DNS_RR *rr, int limit)
{

    /*
     * Pre: list != 0, all lists are concatenated with dns_rr_append().
     * 
     * Post: all elements have the DNS_RR_FLAG_TRUNCATED flag value set, or all
     * elements have it cleared, so that there is no need to update code in
     * legacy stable releases that deletes or reorders elements.
     */
    if (limit <= 1) {
	if (list->next || rr) {
	    msg_warn("DNS record count limit (%d) exceeded -- dropping"
		     " excess record(s) after qname=%s qtype=%s",
		     var_dns_rr_list_limit, list->qname,
		     dns_strtype(list->type));
	    list->flags |= DNS_RR_FLAG_TRUNCATED;
	    dns_rr_free(list->next);
	    dns_rr_free(rr);
	    list->next = 0;
	}
    } else {
	if (list->next == 0 && rr) {
	    list->next = rr;
	    rr = 0;
	}
	if (list->next) {
	    dns_rr_append_with_limit(list->next, rr, limit - 1);
	    list->flags |= list->next->flags;
	}
    }
}

/* dns_rr_append - append resource record(s) to list, or discard */

DNS_RR *dns_rr_append(DNS_RR *list, DNS_RR *rr)
{

    /*
     * Note: rr is not length checked; when multiple lists are concatenated,
     * the output length may be a small multiple of var_dns_rr_list_limit.
     */
    if (rr == 0)
	return (list);
    if (list == 0)
	return (rr);
    if (!DNS_RR_IS_TRUNCATED(list)) {
	dns_rr_append_with_limit(list, rr, var_dns_rr_list_limit);
    } else {
	dns_rr_free(rr);
    }
    return (list);
}

/* dns_rr_compare_pref_ipv6 - compare records by preference, ipv6 preferred */

int     dns_rr_compare_pref_ipv6(DNS_RR *a, DNS_RR *b)
{
    if (a->pref != b->pref)
	return (a->pref - b->pref);
#ifdef HAS_IPV6
    if (a->type == b->type)			/* 200412 */
	return 0;
    if (a->type == T_AAAA)
	return (-1);
    if (b->type == T_AAAA)
	return (+1);
#endif
    return 0;
}

/* dns_rr_compare_pref_ipv4 - compare records by preference, ipv4 preferred */

int     dns_rr_compare_pref_ipv4(DNS_RR *a, DNS_RR *b)
{
    if (a->pref != b->pref)
	return (a->pref - b->pref);
#ifdef HAS_IPV6
    if (a->type == b->type)
	return 0;
    if (a->type == T_AAAA)
	return (+1);
    if (b->type == T_AAAA)
	return (-1);
#endif
    return 0;
}

/* dns_rr_compare_pref_any - compare records by preference, protocol-neutral */

int     dns_rr_compare_pref_any(DNS_RR *a, DNS_RR *b)
{
    if (a->pref != b->pref)
	return (a->pref - b->pref);
    return 0;
}

/* dns_rr_compare_pref - binary compatibility helper after name change */

int     dns_rr_compare_pref(DNS_RR *a, DNS_RR *b)
{
    return (dns_rr_compare_pref_ipv6(a, b));
}

/* dns_rr_sort_callback - glue function */

static int (*dns_rr_sort_user) (DNS_RR *, DNS_RR *);

static int dns_rr_sort_callback(const void *a, const void *b)
{
    DNS_RR *aa = *(DNS_RR **) a;
    DNS_RR *bb = *(DNS_RR **) b;

    return (dns_rr_sort_user(aa, bb));
}

/* dns_rr_sort - sort resource record list */

DNS_RR *dns_rr_sort(DNS_RR *list, int (*compar) (DNS_RR *, DNS_RR *))
{
    int     (*saved_user) (DNS_RR *, DNS_RR *);
    DNS_RR **rr_array;
    DNS_RR *rr;
    int     len;
    int     i;

    /*
     * Save state and initialize.
     */
    saved_user = dns_rr_sort_user;
    dns_rr_sort_user = compar;

    /*
     * Build linear array with pointers to each list element.
     */
    for (len = 0, rr = list; rr != 0; len++, rr = rr->next)
	 /* void */ ;
    rr_array = (DNS_RR **) mymalloc(len * sizeof(*rr_array));
    for (len = 0, rr = list; rr != 0; len++, rr = rr->next)
	rr_array[len] = rr;

    /*
     * Sort by user-specified criterion.
     */
    qsort((void *) rr_array, len, sizeof(*rr_array), dns_rr_sort_callback);

    /*
     * Fix the links.
     */
    for (i = 0; i < len - 1; i++)
	rr_array[i]->next = rr_array[i + 1];
    rr_array[i]->next = 0;
    list = rr_array[0];

    /*
     * Cleanup.
     */
    myfree((void *) rr_array);
    dns_rr_sort_user = saved_user;
    return (list);
}

/* dns_rr_shuffle - shuffle resource record list */

DNS_RR *dns_rr_shuffle(DNS_RR *list)
{
    DNS_RR **rr_array;
    DNS_RR *rr;
    int     len;
    int     i;
    int     r;

    /*
     * Build linear array with pointers to each list element.
     */
    for (len = 0, rr = list; rr != 0; len++, rr = rr->next)
	 /* void */ ;
    rr_array = (DNS_RR **) mymalloc(len * sizeof(*rr_array));
    for (len = 0, rr = list; rr != 0; len++, rr = rr->next)
	rr_array[len] = rr;

    /*
     * Shuffle resource records. Every element has an equal chance of landing
     * in slot 0.  After that every remaining element has an equal chance of
     * landing in slot 1, ...  This is exactly n! states for n! permutations.
     */
    for (i = 0; i < len - 1; i++) {
	r = i + (myrand() % (len - i));		/* Victor&Son */
	rr = rr_array[i];
	rr_array[i] = rr_array[r];
	rr_array[r] = rr;
    }

    /*
     * Fix the links.
     */
    for (i = 0; i < len - 1; i++)
	rr_array[i]->next = rr_array[i + 1];
    rr_array[i]->next = 0;
    list = rr_array[0];

    /*
     * Cleanup.
     */
    myfree((void *) rr_array);
    return (list);
}

/* dns_rr_remove - remove record from list, return new list */

DNS_RR *dns_rr_remove(DNS_RR *list, DNS_RR *record)
{
    if (list == 0)
	msg_panic("dns_rr_remove: record not found");

    if (list == record) {
	list = record->next;
	record->next = 0;
	dns_rr_free(record);
    } else {
	list->next = dns_rr_remove(list->next, record);
    }
    return (list);
}
