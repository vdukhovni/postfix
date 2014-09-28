#ifndef _ATTR_OVERRIDE_H_INCLUDED_
#define _ATTR_OVERRIDE_H_INCLUDED_

/*++
/* NAME
/*	attr_override 3h
/* SUMMARY
/*	apply name=value settings from string
/* SYNOPSIS
/*	#include <attr_override.h>
/* DESCRIPTION
/* .nf

 /*
  * External interface.
  */
extern void attr_override(char *, const char *, const char *,...);

typedef const char *CONST_CHAR_STAR;

typedef struct {
    const char *name;
    int     min;
    int     max;
} ATTR_OVER_STR;

typedef struct {
    const char *name;
    const char *defval;
    int     min;
    int     max;
} ATTR_OVER_TIME;

typedef struct {
    const char *name;
    int     min;
    int     max;
} ATTR_OVER_INT;

#define ATTR_OVER_END		0
#define ATTR_OVER_STR_TABLE	1
#define ATTR_OVER_TIME_TABLE	2
#define ATTR_OVER_INT_TABLE	3

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
