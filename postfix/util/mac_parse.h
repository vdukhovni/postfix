#ifndef _MAC_PARSE_H_INCLUDED_
#define _MAC_PARSE_H_INCLUDED_

/*++
/* NAME
/*	mac_parse 3h
/* SUMMARY
/*	locate macro references in string
/* SYNOPSIS
/*	#include <mac_parse.h>
 DESCRIPTION
 .nf

 /*
  * Utility library.
  */
#include <vstring.h>

 /*
  * External interface.
  */
#define MAC_PARSE_LITERAL	1
#define MAC_PARSE_VARNAME	2

typedef void (*MAC_PARSE_FN)(int, VSTRING *, char *);

extern void mac_parse(const char *, MAC_PARSE_FN, char *);

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
