/*++
/* NAME
/*	casefold 3
/* SUMMARY
/*	casefold text for caseless comparison
/* SYNOPSIS
/*	#include <stringops.h>
/*
/*	char	*casefold(
/*	int	utf8_request,
/*	VSTRING *src,
/*	const char *src,
/*	CONST_CHAR_STAR *err)
/* DESCRIPTION
/*	casefold() converts text to a form that is suitable for
/*	caseless comparison, rather than presentation to humans.
/*
/*	When compiled without EAI support, casefold() implements
/*	ASCII case folding, leaving non-ASCII byte values unchanged.
/*	This mode has no error returns.
/*
/*	When compiled with EAI support, casefold() implements UTF-8
/*	case folding using the en_US locale, as recommended when
/*	the conversion result is not meant to be presented to humans.
/*	When conversion fails the result is null, and the pointer
/*	referenced by err is updated.
/*
/*	With the ICU 4.8 library, there is no casefold error for
/*	UTF-8 code points U+0000..U+10FFFF (including surrogate
/*	range), not even when running inside an empty chroot jail.
/*
/*	Arguments:
/* .IP utf8_request
/*	Boolean parameter that enables UTF-8 case folding instead
/*	of folding only ASCII characters. This flag is ignored when
/*	compiled without EAI support.
/* .IP src
/*	Null-terminated input string.
/* .IP dest
/*	Output buffer, null-terminated if the function completes
/*	without reporting an error.
/* .IP err
/*	Null pointer, or pointer to "const char *". for descriptive
/*	text about errors.
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

/* System library. */

#include <sys_defs.h>
#include <string.h>
#include <ctype.h>
#ifndef NO_EAI
#include <unicode/ucasemap.h>
#include <unicode/ustring.h>
#include <unicode/uchar.h>
#endif

/* Utility library. */

#include <msg.h>
#include <stringops.h>

#define STR(x) vstring_str(x)
#define LEN(x) VSTRING_LEN(x)

/* casefold - casefold an UTF-8 string */

char   *casefold(int utf8_req, VSTRING *dest, const char *src,
		         CONST_CHAR_STAR *err)
{
#ifdef NO_EAI

    /*
     * ASCII mode only.
     */
    vstring_strcpy(dest, src);
    return (lowercase(STR(dest)));
#else

    /*
     * Unicode mode.
     */
    static UCaseMap *csm = 0;
    UErrorCode error;
    ssize_t space_needed;
    int     n;

    /*
     * All-ASCII input, or ASCII mode only.
     */
    if (utf8_req == 0 || allascii(src)) {
	vstring_strcpy(dest, src);
	return (lowercase(STR(dest)));
    }

    /*
     * ICU 4.8 ucasemap_utf8FoldCase() does not complain about UTF-8 syntax
     * errors. XXX Is this behavior guaranteed or accidental? We don't know,
     * therefore must check it here.
     */
    if (valid_utf8_string(src, strlen(src)) == 0) {
	if (err)
	    *err = "malformed UTF-8 or invalid codepoint";
	return (0);
    }

    /*
     * One-time initialization. With ICU 4.8 this works while chrooted.
     */
    if (csm == 0) {
	error = U_ZERO_ERROR;
	csm = ucasemap_open("en_US", U_FOLD_CASE_DEFAULT, &error);
	if (U_SUCCESS(error) == 0)
	    msg_fatal("ucasemap_open error: %s", u_errorName(error));
    }

    /*
     * Fold the input, adjusting the buffer size if needed. Safety: don't
     * loop forever.
     */
    VSTRING_RESET(dest);
    for (n = 0; n < 3; n++) {
	error = U_ZERO_ERROR;
	space_needed =
	    ucasemap_utf8FoldCase(csm, STR(dest), vstring_avail(dest),
				  src, strlen(src), &error);
	if (error == U_BUFFER_OVERFLOW_ERROR) {
	    VSTRING_SPACE(dest, space_needed);
	} else {
	    break;
	}
    }

    /*
     * Report the result. With ICU 4.8, there are no casefolding errors for
     * the entire RFC 3629 Unicode range (code points U+0000..U+10FFFF
     * including surrogates), nor are there casefolding errors for bad UTF-8
     * input. XXX Is this behavior guaranteed or accidental? We don't know,
     * therefore we have the UTF-8 syntax check (and range check) above.
     */
    if (U_SUCCESS(error) == 0) {
	if (err)
	    *err = u_errorName(error);
	return (0);
    } else {
	/* Position the write pointer at the null terminator. */
	VSTRING_AT_OFFSET(dest, space_needed - 1);
	return (STR(dest));
    }
#endif						/* NO_EAI */
}

#ifdef TEST

static void encode_utf8(VSTRING *buffer, int codepoint)
{
    const char myname[] = "encode_utf8";

    VSTRING_RESET(buffer);
    if (codepoint < 0x80) {
	VSTRING_ADDCH(buffer, codepoint);
    } else if (codepoint < 0x800) {
	VSTRING_ADDCH(buffer, 0xc0 | (codepoint >> 6));
	VSTRING_ADDCH(buffer, 0x80 | (codepoint & 0x3f));
    } else if (codepoint < 0x10000) {
	VSTRING_ADDCH(buffer, 0xe0 | (codepoint >> 12));
	VSTRING_ADDCH(buffer, 0x80 | ((codepoint >> 6) & 0x3f));
	VSTRING_ADDCH(buffer, 0x80 | (codepoint & 0x3f));
    } else if (codepoint <= 0x10FFFF) {
	VSTRING_ADDCH(buffer, 0xf0 | (codepoint >> 18));
	VSTRING_ADDCH(buffer, 0x80 | ((codepoint >> 12) & 0x3f));
	VSTRING_ADDCH(buffer, 0x80 | ((codepoint >> 6) & 0x3f));
	VSTRING_ADDCH(buffer, 0x80 | (codepoint & 0x3f));
    } else {
	msg_panic("%s: out-of-range codepoint U+%X", myname, codepoint);
    }
    VSTRING_TERMINATE(buffer);
}

#include <stdlib.h>
#include <stdio.h>
#include <locale.h>

#include <vstream.h>
#include <vstring_vstream.h>
#include <msg_vstream.h>

int     main(int argc, char **argv)
{
    VSTRING *buffer = vstring_alloc(1);
    VSTRING *dest = vstring_alloc(1);
    char   *bp;
    char   *conv_res;
    const char *fold_err;
    char   *cmd;
    int     codepoint, first, last, utf8_req;

    if (setlocale(LC_ALL, "C") == 0)
	msg_fatal("setlocale(LC_ALL, C) failed: %m");

    msg_vstream_init(argv[0], VSTREAM_ERR);

    utf8_req = util_utf8_enable = 1;

    VSTRING_SPACE(buffer, 256);			/* chroot pathname */

    while (vstring_fgets_nonl(buffer, VSTREAM_IN)) {
	bp = STR(buffer);
	vstream_printf("> %s\n", bp);
	cmd = mystrtok(&bp, CHARS_SPACE);
	if (cmd == 0 || *cmd == '#')
	    continue;
	while (ISSPACE(*bp))
	    bp++;

	/*
	 * Null-terminated string.
	 */
	if (strcmp(cmd, "fold") == 0) {
	    if ((conv_res = casefold(utf8_req, dest, bp, &fold_err)) != 0)
		vstream_printf("\"%s\" ->fold \"%s\"\n", bp, conv_res);
	    else
		vstream_printf("cannot casefold \"%s\": %s\n", bp, fold_err);
	}

	/*
	 * Codepoint range.
	 */
	else if (strcmp(cmd, "range") == 0
		 && sscanf(bp, "%i %i", &first, &last) == 2
		 && first <= last) {
	    for (codepoint = first; codepoint <= last; codepoint++) {
		if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
		    vstream_printf("skipping surrogate range\n");
		    codepoint = 0xDFFF;
		} else {
		    encode_utf8(buffer, codepoint);
		    if (msg_verbose)
			vstream_printf("U+%X -> %s\n", codepoint, STR(buffer));
		    if (valid_utf8_string(STR(buffer), LEN(buffer)) == 0)
			msg_fatal("bad utf-8 encoding for U+%X\n", codepoint);
		    if (casefold(utf8_req, dest, STR(buffer), &fold_err) == 0)
			vstream_printf("casefold error for U+%X: %s\n",
				       codepoint, fold_err);
		}
	    }
	    vstream_printf("range completed: 0x%x..0x%x\n", first, last);
	}

	/*
	 * Chroot directory.
	 */
	else if (strcmp(cmd, "chroot") == 0
		 && sscanf(bp, "%255s", STR(buffer)) == 1) {
	    if (geteuid() == 0) {
		if (chdir(STR(buffer)) < 0)
		    msg_fatal("chdir(%s): %m\n", STR(buffer));
		if (chroot(STR(buffer)) < 0)
		    msg_fatal("chroot(%s): %m\n", STR(buffer));
		vstream_printf("chroot %s completed\n", STR(buffer));
	    }
	}

	/*
	 * Verbose.
	 */
	else if (strcmp(cmd, "verbose") == 0
		 && sscanf(bp, "%i", &msg_verbose) == 1) {
	     /* void */ ;
	}

	/*
	 * Usage
	 */
	else {
	    vstream_printf("Usage: %s chroot <path> | fold <text> | range <first> <last> | verbose <int>\n",
			   argv[0]);
	}
	vstream_fflush(VSTREAM_OUT);
    }
    exit(0);
}

#endif					/* TEST */
