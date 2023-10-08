/*++
/* NAME
/*	printable 3
/* SUMMARY
/*	mask non-printable characters
/* SYNOPSIS
/*	#include <stringops.h>
/*
/*	int	util_utf8_enable;
/*
/*	char	*printable(buffer, replacement)
/*	char	*buffer;
/*	int	replacement;
/*
/*	char	*printable_except(buffer, replacement, except)
/*	char	*buffer;
/*	int	replacement;
/*	const char *except;
/* DESCRIPTION
/*	printable() replaces non-printable characters
/*	in its input with the given replacement.
/*
/*	util_utf8_enable controls whether UTF8 is considered printable.
/*	With util_utf8_enable equal to zero, non-ASCII text is replaced.
/*
/*	Arguments:
/* .IP buffer
/*	The null-terminated input string.
/* .IP replacement
/*	Replacement value for characters in \fIbuffer\fR that do not
/*	pass the ASCII isprint(3) test or that are not valid UTF8.
/* .IP except
/*	Null-terminated sequence of non-replaced ASCII characters.
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
/*
/*	Wietse Venema
/*	porcupine.org
/*	Amawalk, NY 10501, USA
/*--*/

/* System library. */

#include "sys_defs.h"
#include <ctype.h>
#include <string.h>

/* Utility library. */

#include "stringops.h"
#include "parse_utf8_char.h"

int util_utf8_enable = 0;

/* printable -  binary compatibility */

#undef printable

char   *printable(char *, int);

char   *printable(char *string, int replacement)
{
    return (printable_except(string, replacement, (char *) 0));
}

/* printable_except -  pass through printable or other preserved characters */

char   *printable_except(char *string, int replacement, const char *except)
{
    char   *cp;
    char   *ep = string + strlen(string);
    char   *last;
    int     ch;

    /*
     * In case of a non-UTF8 sequence (bad leader byte, bad non-leader byte,
     * over-long encodings, out-of-range code points, etc), replace the first
     * byte, and try to resynchronize at the next byte.
     */
#define PRINT_OR_EXCEPT(ch) (ISPRINT(ch) || (except && strchr(except, ch)))

    for (cp = string; (ch = *(unsigned char *) cp) != 0; cp++) {
	if (util_utf8_enable == 0) {
	    if (ISASCII(ch) && PRINT_OR_EXCEPT(ch))
		continue;
	} else if ((last = parse_utf8_char(cp, ep)) == cp) {	/* ASCII */
	    if (PRINT_OR_EXCEPT(ch))
		continue;
	} else if (last > cp) {			/* Other UTF8 */
	    cp = last;
	    continue;
	}
	*cp = replacement;
    }
    return (string);
}

#ifdef TEST

#include <stdlib.h>
#include <string.h>
#include <msg.h>
#include <vstring_vstream.h>

int     main(int argc, char **argv)
{
    VSTRING *in = vstring_alloc(10);

    util_utf8_enable = 1;

    while (vstring_fgets_nonl(in, VSTREAM_IN)) {
        printable(vstring_str(in), '?');
        vstream_fwrite(VSTREAM_OUT, vstring_str(in), VSTRING_LEN(in));
        VSTREAM_PUTC('\n', VSTREAM_OUT);
    }
    vstream_fflush(VSTREAM_OUT);
    exit(0);
}

#endif
