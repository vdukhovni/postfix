#ifndef _ATTR_PRINT_H_INCLUDED_
#define _ATTR_PRINT_H_INCLUDED_

/*++
/* NAME
/*	attr 3h
/* SUMMARY
/*	attribute list manipulations
/* SYNOPSIS
/*	#include "attr.h"
 DESCRIPTION
 .nf

 /*
  * System library.
  */
#include <stdarg.h>

 /*
  * Utility library.
  */
#include <vstream.h>
#include <htable.h>

 /*
  * External interface.
  */
#define ATTR_TYPE_END		0	/* end of data */
#define ATTR_TYPE_NUM		1	/* Unsigned integer */
#define ATTR_TYPE_STR		2	/* Character string */
#define ATTR_TYPE_NUM_ARRAY	3	/* Unsigned integer sequence */
#define ATTR_TYPE_STR_ARRAY	4	/* Character string sequence */
#define ATTR_TYPE_HASH		5	/* Hash table */

#define ATTR_FLAG_NONE		0
#define ATTR_FLAG_MISSING	(1<<0)	/* Flag missing attribute */
#define ATTR_FLAG_EXTRA		(1<<1)	/* Flag spurious attribute */
#define ATTR_FLAG_MORE		(1<<2)	/* Don't skip or terminate */
#define ATTR_FLAG_ALL		(07)

 /*
  * attr_print.c.
  */
extern int attr_print(VSTREAM *, int,...);
extern int attr_vprint(VSTREAM *, int, va_list);

 /*
  * attr_scan.c.
  */
extern int attr_scan(VSTREAM *, int,...);
extern int attr_vscan(VSTREAM *, int, va_list);

 /*
  * attr.c.
  */
extern void attr_enter(HTABLE *, int,...);
extern int attr_find(HTABLE *, int,...);

 /*
  * Attribute names for testing the compatibility of the read and write
  * routines.
  */
#ifdef TEST
#define ATTR_NAME_NUM		"number"
#define ATTR_NAME_STR		"string"
#define ATTR_NAME_NUM_ARRAY	"number_array"
#define ATTR_NAME_STR_ARRAY	"string_array"
#endif

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
