/*++
/* NAME
/*	allprint 3
/* SUMMARY
/*	predicate if string is all printable
/* SYNOPSIS
/*	#include <stringops.h>
/*
/*	int	all_isprint(buffer)
/*	const char *buffer;
/*
/*	int	all_isprint_tab(buffer)
/*	const char *buffer;
/* LEGACY API
/*	int	allprint(buffer)
/*	const char *buffer;
/* DESCRIPTION
/*	all_isprint() determines if its argument contains only isprint()
/*	characters (letters, digits, punctuation, space).
/*
/*	all_isprint_tab() determines if its argument contains only
/*	isprint() or TAB characters. This function follows the "white
/*	space" specification in RFC 5322 section 2.2.1.
/*
/*	allprint() implements ABI backwards compatibility. For new
/*	programs, allprint is an alias for all_isprint.
/*
/*	Arguments:
/* .IP buffer
/*	The null-terminated input string.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*
/*	Wietse Venema
/*	porcupine.org
/*--*/

/* System library. */

#include <sys_defs.h>
#include <ctype.h>

/* Utility library. */

#include "stringops.h"

/* allprint - ABI compatibility */

#undef allprint

int     allprint(const char *string)
{
    return (all_isprint(string));
}

/* all_isprint - return true if string contains only isprint() characters */

int   all_isprint(const char *string)
{
    const char *cp;
    int     ch;

    if (*string == 0)
	return (0);
    for (cp = string; (ch = *(unsigned char *) cp) != 0; cp++)
	if (!ISASCII(ch) || !ISPRINT(ch))
	    return (0);
    return (1);
}

/* all_isprint_tab - return true with only isprint() or TAB characters */

int     all_isprint_tab(const char *string)
{
    const char *cp;
    int     ch;

    if (*string == 0)
	return (0);
    for (cp = string; (ch = *(unsigned char *) cp) != 0; cp++)
	if (!ISASCII(ch) || (!ISPRINT(ch) && ch != '\t'))
	    return (0);
    return (1);
}
