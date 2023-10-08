/*++
/* NAME
/*	valid_utf8_string 3
/* SUMMARY
/*	predicate if string is valid UTF-8
/* SYNOPSIS
/*	#include <stringops.h>
/*
/*	int	valid_utf8_string(str, len)
/*	const char *str;
/*	ssize_t	len;
/* DESCRIPTION
/*	valid_utf8_string() determines if all bytes in a string
/*	satisfy parse_utf8_char(3h) checks. See there for any
/*	implementation limitations.
/*
/*	A zero-length string is considered valid.
/* DIAGNOSTICS
/*	The result value is zero when the caller specifies a negative
/*	length, or a string that does not pass parse_utf8_char(3h) checks.
/* SEE ALSO
/*	parse_utf8_char(3h), parse one UTF-8 multibyte character
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
/*	Amawalk, NY 10501, USA
/*--*/

/* System library. */

#include <sys_defs.h>

/* Utility library. */

#include <stringops.h>
#include <parse_utf8_char.h>

/* valid_utf8_string - validate string according to RFC 3629 */

int     valid_utf8_string(const char *str, ssize_t len)
{
    const char *ep = str + len;
    const char *cp;
    const char *last;

    if (len < 0)
	return (0);
    if (len <= 0)
	return (1);

    /*
     * Ideally, the compiler will inline parse_utf8_char().
     */
    for (cp = str; cp < ep; cp++) {
	if ((last = parse_utf8_char(cp, ep)) != 0)
	    cp = last;
	else
	    return (0);
    }
    return (1);
}

 /*
  * Stand-alone test program. Each string is a line without line terminator.
  */
#ifdef TEST
#include <stdlib.h>
#include <vstream.h>
#include <vstring.h>
#include <vstring_vstream.h>

#define STR(x) vstring_str(x)
#define LEN(x) VSTRING_LEN(x)

int     main(void)
{
    VSTRING *buf = vstring_alloc(1);

    while (vstring_get_nonl(buf, VSTREAM_IN) != VSTREAM_EOF) {
	vstream_printf("%c", (LEN(buf) && !valid_utf8_string(STR(buf), LEN(buf))) ?
		       '!' : ' ');
	vstream_fwrite(VSTREAM_OUT, STR(buf), LEN(buf));
	vstream_printf("\n");
    }
    vstream_fflush(VSTREAM_OUT);
    vstring_free(buf);
    exit(0);
}

#endif
