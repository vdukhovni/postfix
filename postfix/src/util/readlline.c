/*++
/* NAME
/*	readlline 3
/* SUMMARY
/*	read logical line
/* SYNOPSIS
/*	#include <readlline.h>
/*
/*	VSTRING	*readlline(buf, fp, lineno, strip_noise)
/*	VSTRING	*buf;
/*	VSTREAM	*fp;
/*	int	*lineno;
/*	int	strip_noise;
/* DESCRIPTION
/*	readlline() reads one logical line from the named stream.
/*
/*	A line that starts with whitespace (space or tab) is a continuation
/*	of the previous line. An empty line terminates the previous line,
/*	as does a line that starts with non-whitespace (text or comment). A
/*	comment line that starts with whitespace does not terminate multi-line
/*	text.
/*
/*	The # is recognized as the start of a comment, but only when it is
/*	the first non-whitespace character on a line.  A comment terminates
/*	at the end of the line, even when the next line starts with whitespace.
/*
/*	The result value is the input buffer argument or a null pointer
/*	when no input is found.
/*
/*	Arguments:
/* .IP buf
/*	A variable-length buffer for input.
/* .IP fp
/*	Handle to an open stream.
/* .IP lineno
/*	A null pointer, or a pointer to an integer that is incremented
/*	after reading a newline.
/* .IP strip_noise
/*	Non-zero to strip newlines, empty lines and comments from the result.
/*	For convenience, READLL_STRIP_NOISE requests stripping while
/*	READLL_KEEP_NOISE disables stripping.
/* .RE
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

/* Utility library. */

#include "vstream.h"
#include "vstring.h"
#include "readlline.h"

 /*
  * Comment stripper states. States are (1)->(2) or (1)->(3) as we proceed
  * through a line of text.
  */
#define READLL_STATE_WANT_LWSP	1	/* expecting leading whitespace */
#define READLL_STATE_IN_COMMENT	2	/* inside comment */
#define READLL_STATE_IN_TEXT	3	/* inside other text */

#define LWSP_CHARACTER(ch) ((ch) == ' ' || (ch) == '\t')

#define STR(x) vstring_str(x)

/* readlline - read one logical line */

VSTRING *readlline(VSTRING *buf, VSTREAM *fp, int *lineno, int strip_noise)
{
    int     ch;
    int     next;
    int     state;

    /*
     * Lines that start with whitespace continue the preceding line. Comments
     * always terminate at the first newline.
     */
    VSTRING_RESET(buf);
    if (strip_noise)
	state = READLL_STATE_WANT_LWSP;

    while ((ch = VSTREAM_GETC(fp)) != VSTREAM_EOF) {
	/* Skip leading whitespace that doesn't continue a previous line. */
	if (VSTRING_LEN(buf) == 0 && LWSP_CHARACTER(ch))
	    continue;
	if (ch == '\n') {
	    if (lineno)
		*lineno += 1;
	    if (strip_noise) {
		state = READLL_STATE_WANT_LWSP;
		/* Skip empty, whitespace, or comment line before text. */
		if (VSTRING_LEN(buf) == 0)
		    continue;
	    } else {
		VSTRING_ADDCH(buf, ch);
		/* Terminate empty, whitespace, or comment line before text. */
		if (VSTRING_LEN(buf) == 1 || STR(buf)[0] == '#')
		    break;
	    }
	    next = VSTREAM_GETC(fp);
	    /* Continue this line if the next line starts with whitespace. */
	    if (LWSP_CHARACTER(next)) {
		ch = next;
	    } else {
		if (next != VSTREAM_EOF)
		    vstream_ungetc(fp, next);
		break;
	    }
	}
	/* Update the comment stripping state machine. */
	if (strip_noise) {
	    if (state == READLL_STATE_WANT_LWSP) {
		if (ch == '#') {
		    state = READLL_STATE_IN_COMMENT;
		} else if (!LWSP_CHARACTER(ch)) {
		    state = READLL_STATE_IN_TEXT;
		}
	    }
	    if (state == READLL_STATE_IN_COMMENT)
		continue;
	}
	VSTRING_ADDCH(buf, ch);
    }
    VSTRING_TERMINATE(buf);
    return (VSTRING_LEN(buf) || ch == '\n' ? buf : 0);
}
