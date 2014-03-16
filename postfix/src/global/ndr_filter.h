#ifndef _NDR_FILTER_H_INCLUDED_
#define _NDR_FILTER_H_INCLUDED_

/*++
/* NAME
/*	ndr_filter 3h
/* SUMMARY
/*	bounce/defer DSN filter
/* SYNOPSIS
/*	#include <ndr_filter.h>
/* DESCRIPTION
/* .nf

 /*
  * External interface.
  */
typedef struct NDR_FILTER NDR_FILTER;

extern NDR_FILTER *ndr_filter_create(const char *, const char *);
extern DSN *ndr_filter_lookup(NDR_FILTER *, DSN *);
extern void ndr_filter_free(NDR_FILTER *);

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

#endif
