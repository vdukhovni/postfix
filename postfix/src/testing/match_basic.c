/*++
/* NAME
/*	match_basic 3
/* SUMMARY
/*	basic matchers
/* SYNOPSIS
/*	#include <match_basic.h>
/*
/*	int	eq_int(
/*	PTEST_CTX *t,
/*	const char *what,
/*	int	got,
/*	int	want)
/*
/*	int	eq_size_t(
/*	PTEST_CTX *t,
/*	const char *what,
/*	size_t	got,
/*	size_t	want)
/*
/*	int	eq_ssize_t(
/*	PTEST_CTX *t,
/*	const char *what,
/*	ssize_t	got,
/*	ssize_t	want)
/*
/*	int	eq_flags(
/*	PTEST_CTX *t,
/*	const char *what,
/*	int	got,
/*	int	want,
/*	const char *(*flags_to_str) (VSTRING *, int))
/*
/*	int	eq_enum(
/*	PTEST_CTX *t,
/*	const char *what,
/*	int	got,
/*	int	want,
/*	const char *(*enum_to_str) (int))
/*
/*	int	eq_str(
/*	PTEST_CTX *t,
/*	const char *what,
/*	const char *got,
/*	const char *want)
/*
/*	int	eq_argv(
/*	PTEST_CTX *t,
/*	const char *what,
/*	const ARGV *got,
/*	const ARGV *want)
/* DESCRIPTION
/*	The functions described here are actually safe macros that
/*	include call-site information (file name, line number) in
/*	error messages.
/*
/*	eq_int() compares two integers, and if t is not null, reports
/*	values that differ with ptest_error());
/*
/*	eq_flags() compares two integer bitmasks, and if t is not
/*	null, reports values that differ with ptest_error());
/*
/*	eq_enum() compares two integer enum values, and if t is not
/*	null, reports values that differ with ptest_error());
/*
/*	eq_str() compares two null-terminated strings, and if t is
/*	not null, reports values that differ with ptest_error());
/*
/*	eq_argv() compares two null-terminated string arrays, and
/*	if t is not null, reports values that differ with ptest_error());
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

 /*
  * System library.
  */
#include <sys_defs.h>
#include <string.h>

 /*
  * Utility library.
  */
#include <vstring.h>

 /*
  * Test library.
  */
#include <ptest.h>
#include <match_basic.h>

#define STR_OR_NULL(s)		((s) ? (s) : "(null)")

/* _eq_int - match integers */

int     _eq_int(const char *file, int line, PTEST_CTX *t,
		        const char *what, int got, int want)
{
    if (got != want) {
	if (t)
	    ptest_error(t, "%s:%d: %s: got %d, want %d", file, line, what, got, want);
	return (0);
    }
    return (1);
}

/* _eq_size_t - match size_t */

int     _eq_size_t(const char *file, int line, PTEST_CTX *t,
		           const char *what, size_t got, size_t want)
{
    if (got != want) {
	if (t)
	    ptest_error(t, "%s:%d: %s: got %lu, want %lu",
			file, line, what, (long) got, (long) want);
	return (0);
    }
    return (1);
}

/* _eq_ssize_t - match ssize_t */

int     _eq_ssize_t(const char *file, int line, PTEST_CTX *t,
		            const char *what, ssize_t got, ssize_t want)
{
    if (got != want) {
	if (t)
	    ptest_error(t, "%s:%d: %s: got %ld, want %ld",
			file, line, what, (long) got, (long) want);
	return (0);
    }
    return (1);
}

/* _eq_flags - match flags */

int     _eq_flags(const char *file, int line, PTEST_CTX *t,
		          const char *what, int got, int want,
		          const char *(flags_to_string) (VSTRING *, int))
{
    if (got != want) {
	if (t) {
	    VSTRING *got_buf = vstring_alloc(100);
	    VSTRING *want_buf = vstring_alloc(100);

	    ptest_error(t, "%s:%d: %s: got %s, want %s", file, line, what,
			flags_to_string(got_buf, got),
			flags_to_string(want_buf, want));
	    vstring_free(got_buf);
	    vstring_free(want_buf);
	}
	return (0);
    }
    return (1);
}

/* _eq_enum - match enum */

int     _eq_enum(const char *file, int line, PTEST_CTX *t,
		         const char *what, int got, int want,
		         const char *(enum_to_string) (int))
{
    if (got != want) {
	if (t)
	    ptest_error(t, "%s:%d: %s: got %s, want %s", file, line, what,
			enum_to_string(got), enum_to_string(want));
	return (0);
    }
    return (1);
}

/* _eq_str - match null-terminated strings */

int     _eq_str(const char *file, int line, PTEST_CTX *t,
		        const char *what, const char *got, const char *want)
{
    if (strcmp(got, want) != 0) {
	if (t)
	    ptest_error(t, "%s:%d: %s: got '%s', want '%s'",
			file, line, what, got, want);
	return (0);
    }
    return (1);
}

/* _eq_argv - match ARGV instances */

int     _eq_argv(const char *file, int line, PTEST_CTX *t,
		         const char *what, const ARGV *got, const ARGV *want)
{
    char  **gpp, **wpp;

    for (gpp = got->argv, wpp = want->argv; /* see below */ ; gpp++, wpp++) {
	if (*gpp == 0 && *wpp == 0) {
	    return (1);
	} else if (*gpp == 0 || *wpp == 0) {
	    if (t)
		ptest_error(t, "%s:%d: %s: got %s, want %s",
			    file, line, what, STR_OR_NULL(*gpp),
			    STR_OR_NULL(*wpp));
	    return (0);
	} else if (!_eq_str(file, line, t, what, *gpp, *wpp)) {
	    return (0);
	}
    }
}
