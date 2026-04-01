#ifndef _MATCH_ATTR_H_INCLUDED_
#define _MATCH_ATTR_H_INCLUDED_

/*++
/* NAME
/*	match_attr 3h
/* SUMMARY
/*	attribute matching
/* SYNOPSIS
/*	#include <match_attr.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <vstring.h>

 /*
  * Test library.
  */
#include <ptest.h>

 /*
  * External interface.
  */
extern int _eq_attr(const char *, int, PTEST_CTX *, const char *,
		            VSTRING *, VSTRING *);

#define eq_attr(...)	_eq_attr(__FILE__, __LINE__, __VA_ARGS__)

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	Google, Inc.
/*	111 8th Avenue
/*	New York, NY 10011, USA
/*--*/

#endif
