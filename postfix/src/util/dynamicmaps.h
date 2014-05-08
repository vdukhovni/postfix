#ifndef _DYNAMICMAPS_H_INCLUDED_
#define _DYNAMICMAPS_H_INCLUDED_

/*++
/* NAME
/*	dynamicmaps 3h
/* SUMMARY
/*	load dictionaries dynamically
/* SYNOPSIS
/*	#include <dynamicmaps.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <argv.h>

 /*
  * External interface.
  */
#ifdef USE_DYNAMIC_LIBS

typedef DICT *(*dymap_open_t) (const char *, int, int);
typedef void *(*dymap_mkmap_t) (const char *);

extern void dymap_init(const char *);
extern ARGV *dymap_list(ARGV *);
extern dymap_open_t dymap_get_open_fn(const char *);
extern dymap_mkmap_t dymap_get_mkmap_fn(const char *);

#endif
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	LaMont Jones
/*	Hewlett-Packard Company
/*	3404 Harmony Road
/*	Fort Collins, CO 80528, USA
/*
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

#endif
