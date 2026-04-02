#ifndef _MAKE_ATTR_H_INCLUDED_
#define _MAKE_ATTR_H_INCLUDED_

/*++
/* NAME
/*	make_attr 3h
/* SUMMARY
/*	create serialized attributes
/* SYNOPSIS
/*	#include <make_attr.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <vstring.h>

 /*
  * External interface.
  */
extern VSTRING *make_attr(int,...);

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
