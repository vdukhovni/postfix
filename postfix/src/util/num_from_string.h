#ifndef _UGID_FROM_STRING_H_INCLUDED_
#define _UGID_FROM_STRING_H_INCLUDED_

/*++
/* NAME
/*	num_from_string 3h
/* SUMMARY
/*	Convert string to basic integer types
/* SYNOPSIS
/*	#include <num_from_string.h>
/* DESCRIPTION
/* .nf

 /*
  * External interface.
  */
#define SIGNED_FROM_STRING(fname, out_type) \
    int fname(const char *str, out_type *out) \
    { \
	long n; \
	out_type num; \
	if (long_from_string(str, &n) < 0 || (num = (out_type) n) != n) \
	    return (-1); \
	*out = num; \
	return (0); \
    }

#define UNSIGNED_FROM_STRING(fname, out_type) \
    int fname(const char *str, out_type *out) \
    { \
	unsigned long n; \
	out_type num; \
	if (ulong_from_string(str, &n) < 0 || (num = (out_type) n) != n) \
	    return (-1); \
	*out = num; \
	return (0); \
    }

extern int ssize_from_string(const char *str, ssize_t *out);
extern int long_from_string(const char *str, long *out);
extern int int_from_string(const char *str, int *out);
extern int short_from_string(const char *str, short *out);
extern int schar_from_string(const char *str, signed char *out);
extern int size_from_string(const char *str, size_t *out);
extern int ulong_from_string(const char *str, unsigned long *out);
extern int unsigned_from_string(const char *str, unsigned *out);
extern int ushort_from_string(const char *str, unsigned short *out);
extern int uchar_from_string(const char *str, unsigned char *out);

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

#endif
