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
extern const char *mail_addr_find_opt(MAPS *, const char *, char **, int, int);
extern const char *mail_addr_find_trans(MAPS *, const char *, char **);

 /* The least-overhead form. */
#define mail_addr_find_int_to_ext(maps, address, extension) \
	mail_addr_find_opt((maps), (address), (extension), \
	    MAIL_ADDR_FORM_INTERNAL, MAIL_ADDR_FORM_EXTERNAL)

 /* The legacy form. */
#define mail_addr_find(maps, address, extension) \
	mail_addr_find_opt((maps), (address), (extension), \
	    MAIL_ADDR_FORM_NOCONV, MAIL_ADDR_FORM_NOCONV)

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
