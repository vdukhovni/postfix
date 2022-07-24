#ifndef _MATCH_BASIC_H_INCLUDED_
#define _MATCH_BASIC_H_INCLUDED_

/*++
/* NAME
/*	match_basic 3h
/* SUMMARY
/*	basic matchers
/* SYNOPSIS
/*	#include <matchers.h>
/* DESCRIPTION
/* .nf

 /*
  * System library.
  */
#include <sys_defs.h>
#include <setjmp.h>

 /*
  * Test library.
  */
#include <ptest.h>

 /*
  * Matchers.
  */
#define eq_int(t, what, got, want) \
	_eq_int(__FILE__, __LINE__, (t), (what), (got), (want))

extern int _eq_int(const char *, int, PTEST_CTX *,
		           const char *, int, int);

#define eq_size_t(t, what, got, want) \
	_eq_int(__FILE__, __LINE__, (t), (what), (got), (want))

extern int _eq_size_t(const char *, int, PTEST_CTX *,
		              const char *, size_t, size_t);

#define eq_ssize_t(t, what, got, want) \
	_eq_int(__FILE__, __LINE__, (t), (what), (got), (want))

extern int _eq_ssize_t(const char *, int, PTEST_CTX *,
		               const char *, ssize_t, ssize_t);

#define eq_flags(t, what, got, want, flags_to_str) \
	_eq_flags(__FILE__, __LINE__, (t), (what), (got), (want), (flags_to_str))

extern int _eq_flags(const char *, int, PTEST_CTX *,
		             const char *, int, int,
		             const char *(*flags_to_str) (VSTRING *, int));

#define eq_enum(t, what, got, want, enum_to_str) \
	_eq_lflags(__FILE__, __LINE__, (t), (what), (got), (want), (enum_to_str))

extern int _eq_enum(const char *, int, PTEST_CTX *,
	          const char *, int, int, const char *(*enum_to_str) (int));

#define eq_str(t, what, got, want) \
	_eq_str(__FILE__, __LINE__, (t), (what), (got), (want))

extern int _eq_str(const char *, int, PTEST_CTX *,
		           const char *, const char *, const char *);

#define eq_argv(t, what, got, want) \
	_eq_argv(__FILE__, __LINE__, (t), (what), (got), (want))

extern int _eq_argv(const char *, int, PTEST_CTX *,
		            const char *, const ARGV *, const ARGV *);

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
