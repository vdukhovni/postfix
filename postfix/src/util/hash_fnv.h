#ifndef _HASH_FNV_H_INCLUDED_
#define _HASH_FNV_H_INCLUDED_

/*++
/* NAME
/*	hash_fnv 3h
/* SUMMARY
/*	Fowler/Noll/Vo hash function
/* SYNOPSIS
/*	#include <hash_fnv.h>
/* DESCRIPTION
/* .nf

 /*
  * Systemn library.
  */
#ifndef NO_STDINT_H
#include <stdint.h>
#endif

 /*
  * External interface.
  */
#ifdef NO_64_BITS
#define HASH_FNV_T	uint32_t
#else
#define	HASH_FNV_T	uint64_t
#endif

extern HASH_FNV_T hash_fnv(const void *, size_t);

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
