#ifndef _DICT_ML_H_INCLUDED_
#define _DICT_ML_H_INCLUDED_

/*++
/* NAME
/*	dict_ml 3h
/* SUMMARY
/*	dictionary manager, multi-line entry support
/* SYNOPSIS
/*	#include <dict_ml.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <vstream.h>
#include <dict.h>

 /*
  * External interface.
  */
extern void dict_ml_load_file(const char *, const char *);
extern void dict_ml_load_fp(const char *, VSTREAM *);

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
