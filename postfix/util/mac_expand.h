#ifndef _MAC_EXPAND_H_INCLUDED_
#define _MAC_EXPAND_H_INCLUDED_

/*++
/* NAME
/*	mac_expand 3h
/* SUMMARY
/*	expand macro references in string
/* SYNOPSIS
/*	#include <mac_expand.h>
 DESCRIPTION
 .nf

 /*
  * Utility library.
  */
#include <vstring.h>

 /*
  * External interface.
  */
#define MAC_EXP_FLAG_NONE	(0)
#define MAC_EXP_FLAG_UNDEF	(1<<0)
#define MAC_EXP_FLAG_RECURSE	(1<<1)

#define MAC_EXP_ARG_END		0
#define MAC_EXP_ARG_ATTR	1
#define MAC_EXP_ARG_TABLE	2
#define MAC_EXP_ARG_FILTER	3
#define MAC_EXP_ARG_CLOBBER	4

extern int mac_expand(VSTRING *, const char *, int, int,...);

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
