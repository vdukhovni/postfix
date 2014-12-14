#ifndef _ATTR_H_INCLUDED_
#define _ATTR_H_INCLUDED_

/*++
/* NAME
/*	attr 3h
/* SUMMARY
/*	attribute list manipulations
/* SYNOPSIS
/*	#include "attr.h"
 DESCRIPTION
 .nf

 /*
  * System library.
  */
#include <stdarg.h>

 /*
  * Utility library.
  */
#include <vstream.h>

 /*
  * Attribute types. See attr_scan(3) for documentation.
  */
#define ATTR_TYPE_END		0	/* end of data */
#define ATTR_TYPE_INT		1	/* Unsigned integer */
#define ATTR_TYPE_NUM		ATTR_TYPE_INT
#define ATTR_TYPE_STR		2	/* Character string */
#define ATTR_TYPE_HASH		3	/* Hash table */
#define ATTR_TYPE_NV		3	/* Name-value table */
#define ATTR_TYPE_LONG		4	/* Unsigned long */
#define ATTR_TYPE_DATA		5	/* Binary data */
#define ATTR_TYPE_FUNC		6	/* Function pointer */

#define ATTR_HASH_LIMIT		1024	/* Size of hash table */

 /*
  * Optional wrappers to enable type checking on varargs argument lists. Each
  * non-pointer argument is handled by a want_xxx_val() wrapper (xxx = int,
  * long or ssize_t), and each pointer argument is handled by a
  * want_xxx_ptr() wrapper. With VARARGS_ATTR_DEBUG defined, the wrappers
  * will detect type mis-matches of interest.
  * 
  * Note 1: Non-pointer types. With VARARGS_ATTR_DEBUG defined, we don't detect
  * type mismatches between non-pointer types. Reason: the want_xxx_val()
  * wrappers cannot force the caller to actually pass an int, long or ssize_t
  * argument. We do detect type mismatches between pointer/non-pointer types.
  * Since type mismatches between non-pointers cannot be detected with
  * VARARGS_ATTR_DEBUG defined, the want_xxx_val() wrappers use a typecast
  * with VARARGS_ATTR_DEBUG undefined.
  * 
  * Note 2: Pointer types. With VARARGS_ATTR_DEBUG defined, we do detect type
  * mismatches between pointer types. The want_(const_)void_ptr() wrappers
  * expect char* arguments instead of void*, because the latter would never
  * complain about pointer type mismatches. If the caller actually passes a
  * void* argument, then the implicit conversion to char* will be silent, and
  * that is exactly what we want. We also detect type mismatches between
  * pointer/non-pointer types. Since all type mismatches of interest can be
  * detected with VARARGS_ATTR_DEBUG defined, the want_xxx_ptr() wrappers
  * become NOOPs with VARARGS_ATTR_DEBUG undefined.
  */
#ifdef VARARGS_ATTR_DEBUG
static inline int want_int_val(int v)
{					/* Note 1 */
    return (v);
}
static inline long want_long_val(long v)
{					/* Note 1 */
    return (v);
}
static inline ssize_t want_ssize_t_val(ssize_t v)
{					/* Note 1 */
    return (v);
}
static inline int *want_int_ptr(int *p)
{
    return (p);
}
static inline const char *want_const_char_ptr(const char *p)
{
    return (p);
}
static inline char *want_char_ptr(char *p)
{
    return (p);
}
static inline struct VSTRING *want_vstr_ptr(struct VSTRING *p)
{
    return (p);
}
static inline const struct HTABLE *want_const_ht_ptr(const struct HTABLE *p)
{
    return (p);
}
static inline struct HTABLE *want_ht_ptr(struct HTABLE *p)
{
    return (p);
}
static inline const struct NVTABLE *want_const_nv_ptr(const struct NVTABLE * p)
{
    return (p);
}
static inline struct NVTABLE *want_nv_ptr(struct NVTABLE * p)
{
    return (p);
}
static inline long *want_long_ptr(long *p)
{
    return (p);
}
static inline const char *want_const_void_ptr(const char *p)
{					/* Note 2 */
    return (p);
}
static inline char *want_void_ptr(char *p)
{					/* Note 2 */
    return (p);
}

#else
#define want_int_val(val)	(int) (val)	/* Note 1 */
#define want_long_val(val)	(long) (val)	/* Note 1 */
#define want_ssize_t_val(val)	(ssize_t) (val)	/* Note 1 */
#define want_int_ptr(val)	(val)
#define want_const_char_ptr(val) (val)
#define want_char_ptr(val)	(val)
#define want_vstr_ptr(val)	(val)
#define want_const_ht_ptr(val)	(val)
#define want_ht_ptr(val)	(val)
#define want_const_nv_ptr(val)	(val)
#define want_nv_ptr(val)	(val)
#define want_long_ptr(val)	(val)
#define want_const_void_ptr(val) (val)
#define want_void_ptr(val)	(val)
#endif

#define want_name(name)	want_const_char_ptr(name)

#define SEND_ATTR_INT(name, val)	ATTR_TYPE_INT, want_name(name), want_int_val(val)
#define SEND_ATTR_STR(name, val)	ATTR_TYPE_STR, want_name(name), want_const_char_ptr(val)
#define SEND_ATTR_HASH(name, val)	ATTR_TYPE_HASH, want_name(name), want_const_ht_ptr(val)
#define SEND_ATTR_NV(name, val)		ATTR_TYPE_NV, want_name(name), want_const_nv_ptr(val)
#define SEND_ATTR_LONG(name, val)	ATTR_TYPE_LONG, want_name(name), want_long_val(val)
#define SEND_ATTR_DATA(name, len, val)	ATTR_TYPE_DATA, want_name(name), want_ssize_t_val(len), want_const_void_ptr(val)
#define SEND_ATTR_FUNC(func, val)	ATTR_TYPE_FUNC, (func), want_const_void_ptr(val)

#define RECV_ATTR_INT(name, val)	ATTR_TYPE_INT, want_name(name), want_int_ptr(val)
#define RECV_ATTR_STR(name, val)	ATTR_TYPE_STR, want_name(name), want_vstr_ptr(val)
#define RECV_ATTR_HASH(name, val)	ATTR_TYPE_HASH, want_name(name), want_ht_ptr(val)
#define RECV_ATTR_NV(name, val)		ATTR_TYPE_NV, want_name(name), want_nv_ptr(val)
#define RECV_ATTR_LONG(name, val)	ATTR_TYPE_LONG, want_name(name), want_long_ptr(val)
#define RECV_ATTR_DATA(name, val)	ATTR_TYPE_DATA, want_name(name), want_vstr_ptr(val)
#define RECV_ATTR_FUNC(func, val)	ATTR_TYPE_FUNC, (func), want_void_ptr(val)

 /*
  * Flags that control processing. See attr_scan(3) for documentation.
  */
#define ATTR_FLAG_NONE		0
#define ATTR_FLAG_MISSING	(1<<0)	/* Flag missing attribute */
#define ATTR_FLAG_EXTRA		(1<<1)	/* Flag spurious attribute */
#define ATTR_FLAG_MORE		(1<<2)	/* Don't skip or terminate */

#define ATTR_FLAG_STRICT	(ATTR_FLAG_MISSING | ATTR_FLAG_EXTRA)
#define ATTR_FLAG_ALL		(07)

 /*
  * Delegation for better data abstraction.
  */
typedef int (*ATTR_SCAN_MASTER_FN) (VSTREAM *, int,...);
typedef int (*ATTR_SCAN_SLAVE_FN) (ATTR_SCAN_MASTER_FN, VSTREAM *, int, void *);
typedef int (*ATTR_PRINT_MASTER_FN) (VSTREAM *, int,...);
typedef int (*ATTR_PRINT_SLAVE_FN) (ATTR_PRINT_MASTER_FN, VSTREAM *, int, void *);

 /*
  * Default to null-terminated, as opposed to base64-encoded.
  */
#define attr_print	attr_print0
#define attr_vprint	attr_vprint0
#define attr_scan	attr_scan0
#define attr_vscan	attr_vscan0

 /*
  * attr_print64.c.
  */
extern int attr_print64(VSTREAM *, int,...);
extern int attr_vprint64(VSTREAM *, int, va_list);

 /*
  * attr_scan64.c.
  */
extern int attr_scan64(VSTREAM *, int,...);
extern int attr_vscan64(VSTREAM *, int, va_list);

 /*
  * attr_print0.c.
  */
extern int attr_print0(VSTREAM *, int,...);
extern int attr_vprint0(VSTREAM *, int, va_list);

 /*
  * attr_scan0.c.
  */
extern int attr_scan0(VSTREAM *, int,...);
extern int attr_vscan0(VSTREAM *, int, va_list);

 /*
  * attr_scan_plain.c.
  */
extern int attr_print_plain(VSTREAM *, int,...);
extern int attr_vprint_plain(VSTREAM *, int, va_list);

 /*
  * attr_print_plain.c.
  */
extern int attr_scan_plain(VSTREAM *, int,...);
extern int attr_vscan_plain(VSTREAM *, int, va_list);


 /*
  * Attribute names for testing the compatibility of the read and write
  * routines.
  */
#ifdef TEST
#define ATTR_NAME_INT		"number"
#define ATTR_NAME_STR		"string"
#define ATTR_NAME_LONG		"long_number"
#define ATTR_NAME_DATA		"data"
#endif

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
