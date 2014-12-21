/*++
/* NAME
/*	attr 3
/* SUMMARY
/*	attribute list manipulations
/* SYNOPSIS
/*	#include <attr.h>
/* DESCRIPTION
/*	This module allocates storage for dummy variables that are
/*	never referenced. They are used in expressions that are
/*	discarded by the compiler.  But they are here, just in case.
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

 /*
  * System library.
  */
#include <sys_defs.h>

 /*
  * Utility library.
  */
#include <attr.h>

 /*
  * These should never be referenced, but they are here just in case.
  */
int     CHECK_VAL_DUMMY(int);
long    CHECK_VAL_DUMMY(long);
ssize_t CHECK_VAL_DUMMY(ssize_t);
int    *CHECK_PTR_DUMMY(int);
long   *CHECK_PTR_DUMMY(long);
void   *CHECK_PTR_DUMMY(void);
const char *CHECK_CONST_PTR_DUMMY(char);
const void *CHECK_CONST_PTR_DUMMY(void);
VSTRING *CHECK_PTR_DUMMY(VSTRING);
HTABLE *CHECK_PTR_DUMMY(HTABLE);
const HTABLE *CHECK_CONST_PTR_DUMMY(HTABLE);
NVTABLE *CHECK_PTR_DUMMY(NVTABLE);
const NVTABLE *CHECK_CONST_PTR_DUMMY(NVTABLE);
