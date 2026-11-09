/*++
/* NAME
/*	num_from_string 3
/* SUMMARY
/*	Convert decimal string to basic integer type
/* SYNOPSIS
/*	#include <num_from_string.h>
/*
/*	int	ssize_from_string(const char *uid_str, ssize_t *out)
/*
/*	int	long_from_string(const char *uid_str, long *out)
/*
/*	int	int_from_string(const char *uid_str, int *out)
/*
/*	int	short_from_string(const char *uid_str, short *out)
/*
/*	int	schar_from_string(const char *uid_str, signed char *out)
/*
/*	int	size_from_string(const char *gid_str, size_t *out)
/*
/*	int	ulong_from_string(const char *uid_str, unsigned long *out)
/*
/*	int	unsigned_from_string(const char *gid_str, unsigned *out)
/*
/*	int	ushort_from_string(const char *gid_str, unsigned short *out)
/*
/*	int	uchar_from_string(const char *uid_str, unsigned char *out)
/* AUXILIARY MACROS
/*	All but two functions in this module are instantiated using one
/*	of the following two macros (the exceptions are long_from_string()
/*	and ulong_from_string()).
/*
/*	SIGNED_FROM_STRING(fname, otype) instantiates a function named
/*	fname that calls long_from_string() and that verifies that no
/*	bits are lost after converting the result to type otype.
/*
/*	UNSIGNED_FROM_STRING(fname, otype) provides similar functionality
/*	for conversions to an unsigned type.
/* DESCRIPTION
/*	ssize_from_string(), long_from_string(), int_from_string(),
/*	short_from_string(), and schar_from_string() convert a string
/*	in decimal notation to a basic signed integer type.
/*
/*	size_from_string(), ulong_from_string(), unsigned_from_string(),
/*	ushort_from_string(), and uchar_from_string() perform similar
/*	conversions for unsigned integer types.
/*
/*	These functions overwrite output only when successful.
/* DIAGNOSTICS
/*	The result is 0 for success, -1 for failure.
/* BUGS
/*	These functions are wrappers around strtol() and strtoul(),
/*	and therefore cannot convert types larger than long.
/* SEE ALSO
/*	sane_strtol(3)
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/	Wietse Venema
/*	porcupine.org
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>
#include <errno.h>
#include <stdlib.h>

 /*
  * Utility library.
  */
#include <num_from_string.h>
#include <sane_strtol.h>

int long_from_string(const char *str, long *out)
{ 
    long n; 
    char *end; 
    
    n = sane_strtol(str, &end, 10); 
    if (str == end || *end != 0 || errno != 0)
	return (-1); 
    *out = n; 
    return (0); 
}

SIGNED_FROM_STRING(ssize_from_string, ssize_t)
SIGNED_FROM_STRING(int_from_string, int)
SIGNED_FROM_STRING(short_from_string, short)
SIGNED_FROM_STRING(schar_from_string, signed char)

int ulong_from_string(const char *str, unsigned long *out)
{
    unsigned long n;
    char *end; 

    n = sane_strtoul(str, &end, 10);
    if (str == end || *end != 0 || errno != 0)
        return (-1);
    *out = n;
    return (0);
}

UNSIGNED_FROM_STRING(size_from_string, size_t)
UNSIGNED_FROM_STRING(unsigned_from_string, unsigned)
UNSIGNED_FROM_STRING(ushort_from_string, unsigned short)
UNSIGNED_FROM_STRING(uchar_from_string, unsigned char)
