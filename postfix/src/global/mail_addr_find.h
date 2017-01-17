#ifndef _MAIL_ADDR_FIND_H_INCLUDED_
#define _MAIL_ADDR_FIND_H_INCLUDED_

/*++
/* NAME
/*	mail_addr_find 3h
/* SUMMARY
/*	generic address-based lookup
/* SYNOPSIS
/*	#include <mail_addr_find.h>
/* DESCRIPTION
/* .nf

 /*
  * Global library.
  */
#include <mail_addr_form.h>
#include <maps.h>

 /*
  * External interface.
  */
extern const char *mail_addr_find_opt(MAPS *, const char *, char **, 
	int, int, int);

#define MAIL_ADDR_FIND_FULL	(1<<0)	/* localpart+ext@domain */
#define MAIL_ADDR_FIND_NOEXT	(1<<1)	/* localpart@domain */
#define MAIL_ADDR_FIND_LOCALPART_IF_LOCAL \
				(1<<2)	/* localpart (maybe localpart+ext) */
#define MAIL_ADDR_FIND_LOCALPART_AT_IF_LOCAL \
				(1<<3)	/* ditto, with @ at end */
#define MAIL_ADDR_FIND_ATDOMAIN	(1<<4)	/* @domain */
#define MAIL_ADDR_FIND_DOMAIN	(1<<5)	/* domain */
#define MAIL_ADDR_FIND_PMS	(1<<6)	/* parent matches subdomain */
#define MAIL_ADDR_FIND_PMDS	(1<<7)	/* parent matches dot-subdomain */
#define MAIL_ADDR_FIND_LOCALPART_AT	\
				(1<<8)	/* localpart@ (maybe localpart+ext@) */

#define MAIL_ADDR_FIND_DEFAULT  (MAIL_ADDR_FIND_FULL | MAIL_ADDR_FIND_NOEXT \
				| MAIL_ADDR_FIND_LOCALPART_IF_LOCAL \
				| MAIL_ADDR_FIND_ATDOMAIN)

 /* The least-overhead form. */
#define mail_addr_find_int_to_ext(maps, address, extension) \
	mail_addr_find_opt((maps), (address), (extension), \
	    MAIL_ADDR_FORM_INTERNAL, MAIL_ADDR_FORM_EXTERNAL, \
	MAIL_ADDR_FIND_DEFAULT)

 /* The legacy form. */
extern const char *mail_addr_find_strategy(MAPS *, const char *, char **, int);

#define mail_addr_find(maps, address, extension) \
	mail_addr_find_strategy((maps), (address), (extension), \
	    MAIL_ADDR_FIND_DEFAULT)

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
