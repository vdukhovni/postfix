#ifndef _ATTR_PRINT_H_INCLUDED_
#define _ATTR_PRINT_H_INCLUDED_

/*++
/* NAME
/*	attr_type 3h
/* SUMMARY
/*	attribute list I/O
/* SYNOPSIS
/*	#include "attr_type.h"
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

 /*
  * External interface.
  */
#define ATTR_TYPE_END		0	/* end of data */
#define ATTR_TYPE_NUM		1	/* Unsigned integer */
#define ATTR_TYPE_STR		2	/* Character string */
#define ATTR_TYPE_NUM_ARRAY	3	/* Unsigned integer sequence */
#define ATTR_TYPE_STR_ARRAY	4	/* Character string sequence */

#define ATTR_FLAG_MISSING	(1<<0)	/* Flag missing attribute */
#define ATTR_FLAG_EXTRA		(1<<1)	/* Flag spurious attribute */

extern int attr_print(VSTREAM *,...);
extern int attr_vprint(VSTREAM *, va_list);
extern int attr_scan(VSTREAM *, int,...);
extern int attr_vscan(VSTREAM *, int, va_list);

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
