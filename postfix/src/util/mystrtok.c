/*++
/* NAME
/*	mystrtok 3
/* SUMMARY
/*	safe tokenizer
/* SYNOPSIS
/*	#include <stringops.h>
/*
/*	char	*mystrtok(bufp, delimiters)
/*	char	**bufp;
/*	const char *delimiters;
/*
/*	char	*mystrtokq(bufp, delimiters, parens)
/*	char	**bufp;
/*	const char *delimiters;
/*	const char *parens;
/*
/*	char	*mystrtokdq(bufp, delimiters)
/*	char	**bufp;
/*	const char *delimiters;
/* DESCRIPTION
/*	mystrtok() splits a buffer on the specified \fIdelimiters\fR.
/*	Tokens are delimited by runs of delimiters, so this routine
/*	cannot return zero-length tokens.
/*
/*	mystrtokq() is like mystrtok() but will not split text
/*	between balanced parentheses.  \fIparens\fR specifies the
/*	opening and closing parenthesis (one of each).  The set of
/*	\fIparens\fR must be distinct from the set of \fIdelimiters\fR.
/*
/*	mystrtokdq() is like mystrtok() but will not split text
/*	between double quotes. The backslash character may be used
/*	to escape characters. The double quote and backslash
/*	character must not appear in the set of \fIdelimiters\fR.
/*
/*	The \fIbufp\fR argument specifies the start of the search; it
/*	is updated with each call. The input is destroyed.
/*
/*	The result value is the next token, or a null pointer when the
/*	end of the buffer was reached.
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
/*	Google, Inc.
/*	111 8th Avenue
/*	New York, NY 10011, USA
/*--*/

/* System library. */

#include "sys_defs.h"
#include <string.h>

/* Utility library. */

#include "stringops.h"

/* mystrtok - safe tokenizer */

char   *mystrtok(char **src, const char *sep)
{
    char   *start = *src;
    char   *end;

    /*
     * Skip over leading delimiters.
     */
    start += strspn(start, sep);
    if (*start == 0) {
	*src = start;
	return (0);
    }

    /*
     * Separate off one token.
     */
    end = start + strcspn(start, sep);
    if (*end != 0)
	*end++ = 0;
    *src = end;
    return (start);
}

/* mystrtokq - safe tokenizer with quoting support */

char   *mystrtokq(char **src, const char *sep, const char *parens)
{
    char   *start = *src;
    static char *cp;
    int     ch;
    int     level;

    /*
     * Skip over leading delimiters.
     */
    start += strspn(start, sep);
    if (*start == 0) {
	*src = start;
	return (0);
    }

    /*
     * Parse out the next token.
     */
    for (level = 0, cp = start; (ch = *(unsigned char *) cp) != 0; cp++) {
	if (ch == parens[0]) {
	    level++;
	} else if (level > 0 && ch == parens[1]) {
	    level--;
	} else if (level == 0 && strchr(sep, ch) != 0) {
	    *cp++ = 0;
	    break;
	}
    }
    *src = cp;
    return (start);
}

/* mystrtokdq - safe tokenizer, double quote and backslash support */

char   *mystrtokdq(char **src, const char *sep)
{
    char   *cp = *src;
    char   *start;

    /*
     * Skip leading delimiters.
     */
    cp += strspn(cp, sep);

    /*
     * Skip to next unquoted space or comma.
     */
    if (*cp == 0) {
	start = 0;
    } else {
	int     in_quotes;

	for (in_quotes = 0, start = cp; *cp; cp++) {
	    if (*cp == '\\') {
		if (*++cp == 0)
		    break;
	    } else if (*cp == '"') {
		in_quotes = !in_quotes;
	    } else if (!in_quotes && strchr(sep, *(unsigned char *) cp) != 0) {
		*cp++ = 0;
		break;
	    }
	}
    }
    *src = cp;
    return (start);
}
