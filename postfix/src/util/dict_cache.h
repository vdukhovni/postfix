#ifndef _DICT_CACHE_H_INCLUDED_
#define _DICT_CACHE_H_INCLUDED_

/*++
/* NAME
/*	dict_cache 3h
/* SUMMARY
/*	External cache manager
/* SYNOPSIS
/*	#include <dict_cache.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <dict.h>

 /*
  * External interface.
  */
typedef struct DICT_CACHE DICT_CACHE;
typedef int (*DICT_CACHE_VALIDATOR_FN) (const char *, const char *, char *);

extern DICT_CACHE *dict_cache_open(const char *, int, int);
extern void dict_cache_close(DICT_CACHE *);
extern const char *dict_cache_lookup(DICT_CACHE *, const char *);
extern void dict_cache_update(DICT_CACHE *, const char *, const char *);
extern int dict_cache_delete(DICT_CACHE *, const char *);
extern int dict_cache_sequence(DICT_CACHE *, int, const char **, const char **);
extern void dict_cache_expire(DICT_CACHE *, int, int, DICT_CACHE_VALIDATOR_FN, char *);
extern const char *dict_cache_name(DICT_CACHE *);
extern DICT_CACHE *dict_cache_import(DICT *);

#define DICT_CACHE_FLAG_EXP_VERBOSE	(1<<0)
#define DICT_CACHE_FLAG_EXP_SUMMARY	(1<<1)

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
