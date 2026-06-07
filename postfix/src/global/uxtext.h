#ifndef _UXTEXT_H_INCLUDED_
#define _UXTEXT_H_INCLUDED_

/*++
/* NAME
/*	uxtext 3h
/* SUMMARY
/*	quote/unquote text, RFC 6533 style.
/* SYNOPSIS
/*	#include <uxtext.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <vstring.h>

 /*
  * Legacy interface.
  */
extern VSTRING *uxtext_quote(VSTRING *, const char *, const char *);
extern VSTRING *uxtext_quote_append(VSTRING *, const char *, const char *);

 /*
  * External interface.
  */
extern VSTRING *uxtext_quote_opt(VSTRING *, const char *, const char *, int);
extern VSTRING *uxtext_quote_opt_append(VSTRING *, const char *, const char *, int);
extern VSTRING *uxtext_unquote(VSTRING *, const char *);
extern VSTRING *uxtext_unquote_append(VSTRING *, const char *);

 /*
  * Helpers.
  */
#define UXTEXT_QUOTE_OPT_7BIT	(1<<0)	/* Quote all non-ASCII UTF8 */
#define UXTEXT_QUOTE_OPT_NONE	(0)

 /*
  * Produce RFC 6533 utf-8-addr-xtext.
  */
#define uxtext_quote(...) \
	uxtext_quote_opt(__VA_ARGS__, UXTEXT_QUOTE_OPT_7BIT)
#define uxtext_quote_append(...) \
	uxtext_quote_opt_append(__VA_ARGS__, UXTEXT_QUOTE_OPT_7BIT)

 /*
  * Produce RFC 6533 utf-8-addr-unitext.
  */
#define unitext_quote(...) \
	uxtext_quote_opt(__VA_ARGS__, UXTEXT_QUOTE_OPT_NONE)
#define unitext_quote_append(...) \
	uxtext_quote_opt_append(__VA_ARGS__, UXTEXT_QUOTE_OPT_NONE)

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Arnt Gulbrandsen
/*
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*
/*	Wietse Venema
/*	porcupine.org
/*--*/

#endif
