#ifndef _BINATTR_H_INCLUDED_
#define _BINATTR_H_INCLUDED_

/*++
/* NAME
/*	binattr 3h
/* SUMMARY
/*	binary-valued attribute list manager
/* SYNOPSIS
/*	#include <binattr.h>
/* DESCRIPTION
/* .nf

 /* Structure of one hash table entry. */

typedef void (*BINATTR_FREE_FN) (char *);

typedef struct BINATTR_INFO {
    char   *key;			/* lookup key */
    char   *value;			/* associated value */
    BINATTR_FREE_FN free_fn;		/* destructor */
} BINATTR_INFO;

 /* Structure of one hash table. */

typedef struct HTABLE BINATTR;

extern BINATTR *binattr_create(int);
extern void binattr_free(BINATTR *);
extern char *binattr_get(BINATTR *, const char *);
extern BINATTR_INFO *binattr_set(BINATTR *, const char *, char *, BINATTR_FREE_FN);
extern void binattr_unset(BINATTR *, const char *);

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/* CREATION DATE
/*	Fri Feb 14 13:43:19 EST 1997
/* LAST MODIFICATION
/*	%E% %U%
/* VERSION/RELEASE
/*	%I%
/*--*/

#endif
