#ifndef _MAF_STRATEGY_H_INCLUDED_
#define _MAF_STRATEGY_H_INCLUDED_

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
				              int, int, int, int);

#define MAF_STRATEGY_FULL	(1<<0)	/* localpart+ext@domain */
#define MAF_STRATEGY_NOEXT	(1<<1)	/* localpart@domain */
#define MAF_STRATEGY_LOCALPART_IF_LOCAL \
				(1<<2)	/* localpart (maybe localpart+ext) */
#define MAF_STRATEGY_LOCALPART_AT_IF_LOCAL \
				(1<<3)	/* ditto, with @ at end */
#define MAF_STRATEGY_AT_DOMAIN	(1<<4)	/* @domain */
#define MAF_STRATEGY_DOMAIN	(1<<5)	/* domain */
#define MAF_STRATEGY_PMS	(1<<6)	/* parent matches subdomain */
#define MAF_STRATEGY_PMDS	(1<<7)	/* parent matches dot-subdomain */
#define MAF_STRATEGY_LOCALPART_AT	\
				(1<<8)	/* localpart@ (maybe localpart+ext@) */

#define MAF_STRATEGY_DEFAULT	(MAF_STRATEGY_FULL | MAF_STRATEGY_NOEXT \
				| MAF_STRATEGY_LOCALPART_IF_LOCAL \
				| MAF_STRATEGY_AT_DOMAIN)

 /* The least-overhead form. */
#define mail_addr_find_int_to_ext(maps, address, extension) \
	mail_addr_find_opt((maps), (address), (extension), \
	    MAIL_ADDR_FORM_INTERNAL, MAIL_ADDR_FORM_EXTERNAL, \
	    MAIL_ADDR_FORM_EXTERNAL, MAF_STRATEGY_DEFAULT)

 /* The legacy forms. */
#define MAF_FORM_LEGACY \
	MAIL_ADDR_FORM_INTERNAL, MAIL_ADDR_FORM_EXTERNAL_FIRST, \
	    MAIL_ADDR_FORM_EXTERNAL

#define mail_addr_find_strategy(maps, address, extension, strategy) \
	mail_addr_find_opt((maps), (address), (extension), \
	    MAF_FORM_LEGACY, (strategy))

#define mail_addr_find(maps, address, extension) \
	mail_addr_find_strategy((maps), (address), (extension), \
	    MAF_STRATEGY_DEFAULT)

#define mail_addr_find_to_internal(maps, address, extension) \
	mail_addr_find_opt((maps), (address), (extension), \
	    MAF_FORM_LEGACY, MAF_STRATEGY_DEFAULT)

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
