#ifndef _MAPS_H_INCLUDED_
#define _MAPS_H_INCLUDED_

/*++
/* NAME
/*	maps 3h
/* SUMMARY
/*	multi-dictionary search
/* SYNOPSIS
/*	#include <maps.h>
/* DESCRIPTION
/* .nf

 /*
  * Dictionary name storage. We're borrowing from the argv(3) module.
  */
typedef struct MAPS {
    char   *title;
    struct ARGV *argv;
} MAPS;

extern MAPS *maps_create(const char *, const char *);
extern const char *maps_find(MAPS *, const char *);
extern MAPS *maps_free(MAPS *);

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
