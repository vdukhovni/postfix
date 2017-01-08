#ifndef _MAIL_ADDR_CRUNCH_H_INCLUDED_
#define _MAIL_ADDR_CRUNCH_H_INCLUDED_

/*++
/* NAME
/*	mail_addr_crunch 3h
/* SUMMARY
/*	parse and canonicalize addresses, apply address extension
/* SYNOPSIS
/*	#include <mail_addr_crunch.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <argv.h>

 /*
  * Global library.
  */
#include <mail_addr_form.h>
#include <argv.h>

 /*
  * External interface.
  */
extern ARGV *mail_addr_crunch(const char *, const char *, int, int);

 /* The least-overhead form. */
#define mail_addr_crunch_ext_to_int(string, extension) \
	mail_addr_crunch((string), (extension), MAIL_ADDR_FORM_EXTERNAL, \
			MAIL_ADDR_FORM_INTERNAL)

 /* The legacy form. */
#define mail_addr_crunch_noconv(string, extension) \
	mail_addr_crunch((string), (extension), MAIL_ADDR_FORM_NOCONV, \
			MAIL_ADDR_FORM_NOCONV)

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
