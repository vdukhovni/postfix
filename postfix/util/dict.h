#ifndef _DICT_H_INCLUDED_
#define _DICT_H_INCLUDED_

/*++
/* NAME
/*	dict 3h
/* SUMMARY
/*	dictionary manager
/* SYNOPSIS
/*	#include <dict.h>
/* DESCRIPTION
/* .nf

 /*
  * System library.
  */
#include <fcntl.h>

 /*
  * Utility library.
  */
#include <vstream.h>

 /*
  * Generic dictionary interface - in reality, a dictionary extends this
  * structure with private members to maintain internal state.
  */
typedef struct DICT {
    int     flags;			/* see below */
    const char *(*lookup) (struct DICT *, const char *);
    void    (*update) (struct DICT *, const char *, const char *);
    void    (*close) (struct DICT *);
    int     fd;				/* for dict_update() lock */
} DICT;

#define DICT_FLAG_DUP_WARN	(1<<0)	/* if file, warn about dups */
#define DICT_FLAG_DUP_IGNORE	(1<<1)	/* if file, ignore dups */
#define DICT_FLAG_TRY0NULL	(1<<2)	/* do not append 0 to key/value */
#define DICT_FLAG_TRY1NULL	(1<<3)	/* append 0 to key/value */
#define DICT_FLAG_FIXED		(1<<4)	/* fixed key map */
#define DICT_FLAG_PATTERN	(1<<5)	/* keys are patterns */
#define DICT_FLAG_LOCK		(1<<6)	/* lock before access */

extern int dict_unknown_allowed;
extern int dict_errno;

#define DICT_ERR_RETRY	1		/* soft error */

 /*
  * High-level interface, with logical dictionary names and with implied
  * locking.
  */
extern void dict_register(const char *, DICT *);
extern DICT *dict_handle(const char *);
extern void dict_unregister(const char *);
extern void dict_update(const char *, const char *, const char *);
extern const char *dict_lookup(const char *, const char *);
extern void dict_load_file(const char *, const char *);
extern void dict_load_fp(const char *, VSTREAM *);
extern const char *dict_eval(const char *, const char *, int);

 /*
  * Low-level interface, with physical dictionary handles and no implied
  * locking.
  */
extern DICT *dict_open(const char *, int, int);
extern DICT *dict_open3(const char *, const char *, int, int);
extern void dict_open_register(const char *, DICT *(*)(const char *, int, int));
#define dict_get(dp, key)	(dp)->lookup((dp), (key))
#define dict_put(dp, key, val)	(dp)->update((dp), (key), (val))
#define dict_close(dp)		(dp)->close(dp)

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
