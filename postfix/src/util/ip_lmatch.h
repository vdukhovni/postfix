#ifndef _IP_LMATCH_H_INCLUDED_
#define _IP_LMATCH_H_INCLUDED_

/*++
/* NAME
/*	ip_lmatch 3h
/* SUMMARY
/*	lazy IP address pattern matching
/* SYNOPSIS
/*	#include <ip_lmatch.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <vstring.h>

 /*
  * External interface.
  */
extern int ip_lmatch(char *, const char *, VSTRING **);

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
