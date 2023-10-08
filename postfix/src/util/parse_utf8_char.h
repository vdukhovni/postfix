/*++
/* NAME
/*	parse_utf8_char 3h
/* SUMMARY
/*	parse one UTF-8 multibyte character
/* SYNOPSIS
/*	#include <parse_utf8_char.h>
/*
/*	char	*parse_utf8_char(str, len)
/*	const char *str;
/*	ssize_t len;
/* DESCRIPTION
/*	parse_utf8_char() determines if the \fBlen\fR bytes starting
/*	at \fBstr\fR begin with a complete UTF-8 multi-byte character
/*	as defined in RFC 3629. That is, it contains a proper
/*	encoding of code points U+0000..U+10FFFF, excluding over-long
/*	encodings and excluding U+D800..U+DFFF surrogates.
/*
/*	When the \fBlen\fR bytes starting at \fBstr\fR begin with
/*	a complete UTF-8 multi-byte character, this function returns
/*	a pointer to the last byte in that character. Otherwise,
/*	it returns a null pointer.
/* BUGS
/*	Code points in the range U+FDD0..U+FDEF and ending in FFFE
/*	or FFFF are non-characters in UNICODE. This function does
/*	not reject these.
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

 /*
  * System library.
  */
#include <sys_defs.h>

#ifdef NO_INLINE
#define inline /* */
#endif

/* parse_utf8_char - parse and validate one UTF8 multibyte sequence */

static inline char *parse_utf8_char(const char *str, const char *end)
{
    const unsigned char *cp = (const unsigned char *) str;
    const unsigned char *ep = (const unsigned char *) end;
    unsigned char c0, ch;

    /*
     * Optimized for correct input, time, space, and for CPUs that have a
     * decent number of registers.
     */
    /* Single-byte encodings. */
    if (EXPECTED((c0 = *cp) <= 0x7f) /* we know that c0 >= 0x0 */ ) {
	return ((char *) cp);
    }
    /* Two-byte encodings. */
    else if (EXPECTED(c0 <= 0xdf) /* we know that c0 >= 0x80 */ ) {
	/* Exclude over-long encodings. */
	if (UNEXPECTED(c0 < 0xc2)
	    || UNEXPECTED(cp + 1 >= ep)
	/* Require UTF-8 tail byte. */
	    || UNEXPECTED(((ch = *++cp) & 0xc0) != 0x80))
	    return (0);
	return ((char *) cp);
    }
    /* Three-byte encodings. */
    else if (EXPECTED(c0 <= 0xef) /* we know that c0 >= 0xe0 */ ) {
	if (UNEXPECTED(cp + 2 >= ep)
	/* Exclude over-long encodings. */
	    || UNEXPECTED((ch = *++cp) < (c0 == 0xe0 ? 0xa0 : 0x80))
	/* Exclude U+D800..U+DFFF. */
	    || UNEXPECTED(ch > (c0 == 0xed ? 0x9f : 0xbf))
	/* Require UTF-8 tail byte. */
	    || UNEXPECTED(((ch = *++cp) & 0xc0) != 0x80))
	    return (0);
	return ((char *) cp);
    }
    /* Four-byte encodings. */
    else if (EXPECTED(c0 <= 0xf4) /* we know that c0 >= 0xf0 */ ) {
	if (UNEXPECTED(cp + 3 >= ep)
	/* Exclude over-long encodings. */
	    || UNEXPECTED((ch = *++cp) < (c0 == 0xf0 ? 0x90 : 0x80))
	/* Exclude code points above U+10FFFF. */
	    || UNEXPECTED(ch > (c0 == 0xf4 ? 0x8f : 0xbf))
	/* Require UTF-8 tail byte. */
	    || UNEXPECTED(((ch = *++cp) & 0xc0) != 0x80)
	/* Require UTF-8 tail byte. */
	    || UNEXPECTED(((ch = *++cp) & 0xc0) != 0x80))
	    return (0);
	return ((char *) cp);
    }
    /* Invalid: c0 >= 0xf5 */
    else {
	return (0);
    }
}

#undef inline 
