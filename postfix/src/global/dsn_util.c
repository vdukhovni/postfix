/*++
/* NAME
/*	dsn_util 3
/* SUMMARY
/*	DSN support routines
/* SYNOPSIS
/*	#include <dsn_util.h>
/*
/*	#define DSN_SIZE ...
/*
/*	typedef struct { ... } DSN_BUF;
/*
/*	void	DSN_UPDATE(dsn_buf, dsn, len)
/*	DSN_BUF	dsn_buf;
/*	const char *dsn;
/*	size_t	len;
/*
/*	const char *DSN_CODE(dsn_buf)
/*	DSN_BUF	dsn_buf;
/*
/*	char	*DSN_CLASS(dsn_buf)
/*	DSN_BUF	dsn_buf;
/*
/*	typedef struct {
/* .in +4
/*		DSN_BUF dsn;		/* RFC 3463 detail */
/*		const char *text;	/* Free text */
/* .in -4
/*	} DSN_SPLIT;
/*
/*	DSN_SPLIT *dsn_split(dp, def_dsn, text)
/*	DSN_SPLIT *dp;
/*	const char *def_dsn;
/*	const char *text;
/*
/*	char	*dsn_prepend(def_dsn, text)
/*	const char *def_dsn;
/*	const char *text;
/*
/*	typedef struct {
/* .in +4
/*		DSN_BUF dsn;		/* RFC 3463 detail */
/*		VSTRING *text;		/* Free text */
/* .in -4
/*	} DSN_VSTRING;
/*
/*	DSN_VSTRING *dsn_vstring_alloc(len)
/*	int	len;
/*
/*	DSN_VSTRING *dsn_vstring_update(dv, dsn, format, ...)
/*	DSN_VSTRING *dv;
/*	const char *dsn;
/*	const char *format;
/*
/*	void	dsn_vstring_free(dv)
/*	DSN_VSTRING *dv;
/*
/*	size_t	dsn_valid(text)
/*	const char *text;
/* DESCRIPTION
/*	The functions in this module manipulate pairs of RFC 3463
/*	X.X.X detail codes and descriptive free text.
/*
/*	dsn_split() splits text into an RFC 3463 detail code and
/*	descriptive free text.  When the text does not start with
/*	a detail code, the specified default detail code is used
/*	instead.  Whitespace before the optional detail code or
/*	text is skipped.  dsn_split() returns a copy of the RFC
/*	3463 detail code, and returns a pointer to (not copy of)
/*	the remainder of the text.  The result value is the first
/*	argument.
/*
/*	dsn_prepend() prepends the specified default RFC 3463 detail
/*	code to the specified text if no detail code is present in
/*	the text. This function produces the same result as calling
/*	concatenate() with the results from dsn_split().  The result
/*	should be passed to myfree(). Whitespace before the optional
/*	detail code or text is skipped.
/*
/*	dsn_vstring_alloc() creates initialized storage for an RFC
/*	3463 detail code and descriptive free text.
/*
/*	dsn_vstring_update() updates the detail code, the descriptive
/*	free text, or both. Specify a null pointer (or zero-length
/*	string) for information that should not be updated.
/*
/*	dsn_vstring_free() recycles the storage that was allocated
/*	by dsn_vstring_alloc() and dsn_vstring_update().
/*
/*	DSN_UPDATE() is a helper macro to safely update an
/*	RFC 3463 detail code.
/*
/*	DSN_CODE() is a helper macro to safely read an
/*	RFC 3463 detail code.
/*
/*	DSN_CLASS() is a helper macro to safely read or update an
/*	RFC 3463 detail code class (i.e. the first digit).
/*
/*	DSN_SIZE is the maximal length of an enhanced status
/*	code including the null string terminator.
/*
/*	dsn_valid() returns the length of the RFC 3463 detail code
/*	at the beginning of text, or zero. It does not skip initial
/*	whitespace.
/*
/*	Arguments:
/* .IP def_dsn
/*	Null-terminated default RFC 3463 detail code that will be
/*	used when the free text does not start with one.
/* .IP dp
/*	Pointer to storage for copy of DSN detail code, and for
/*	pointer to free text.
/* .IP dsn
/*	Null-terminated RFC 3463 detail code.
/* .IP text
/*	Null-terminated free text.
/* .IP vp
/*	VSTRING buffer, or null pointer.
/* SEE ALSO
/*	msg(3) diagnostics interface
/* DIAGNOSTICS
/*	Panic: invalid default DSN code; invalid dsn_vstring_update()
/*	DSN argument.
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
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <vstring.h>
#include <stringops.h>

/* Global library. */

#include <dsn_util.h>

/* dsn_valid - check RFC 3463 enhanced status code, return length or zero */

size_t  dsn_valid(const char *text)
{
    const unsigned char *cp = (unsigned char *) text;
    int     len;

    /* First portion is one digit followed by dot. */
    if ((cp[0] != '2' && cp[0] != '4' && cp[0] != '5') || cp[1] != '.')
	return (0);

    /* Second portion is 1-3 digits followed by dot. */
    cp += 2;
    if ((len = strspn((char *) cp, "0123456789")) < 1 || len > DSN_DIGS2
	|| cp[len] != '.')
	return (0);

    /* Last portion is 1-3 digits followed by end-of-string or whitespace. */
    cp += len + 1;
    if ((len = strspn((char *) cp, "0123456789")) < 1 || len > DSN_DIGS3
	|| (cp[len] != 0 && !ISSPACE(cp[len])))
	return (0);

    return (((char *) cp - text) + len);
}

/* dsn_split - split text into DSN and text */

DSN_SPLIT *dsn_split(DSN_SPLIT *dp, const char *def_dsn, const char *text)
{
    const char *cp = text;
    size_t  len;

    /*
     * Look for an optional RFC 3463 enhanced status code.
     * 
     * XXX If we want to enforce that the first digit of the status code in the
     * text matches the default status code, then pipe_command() needs to be
     * changed. It currently auto-detects the reply code without knowing in
     * advance if the result will start with '4' or '5'.
     */
    while (ISSPACE(*cp))
	cp++;
    if ((len = dsn_valid(cp)) > 0) {
	DSN_UPDATE(dp->dsn, cp, len);
	cp += len + 1;
    } else {
	len = strlen(def_dsn);
	DSN_UPDATE(dp->dsn, def_dsn, len);
    }

    /*
     * The remainder is free text.
     */
    while (ISSPACE(*cp))
	cp++;
    dp->text = cp;

    return (dp);
}

/* dsn_prepend - prepend optional detail to text, result on heap */

char   *dsn_prepend(const char *def_dsn, const char *text)
{
    DSN_SPLIT dp;

    dsn_split(&dp, def_dsn, text);
    return (concatenate(DSN_CODE(dp.dsn), " ", dp.text, (char *) 0));
}

/* dsn_vstring_alloc - create DSN+string storage */

DSN_VSTRING *dsn_vstring_alloc(int len)
{
    DSN_VSTRING *dv;

    dv = (DSN_VSTRING *) mymalloc(sizeof(*dv));
    DSN_CLASS(dv->dsn) = 0;
    dv->vstring = vstring_alloc(len);
    return(dv);
}

/* dsn_vstring_free - destroy DSN+string storage */

void    dsn_vstring_free(DSN_VSTRING *dv)
{
    vstring_free(dv->vstring);
    myfree((char *) dv);
}

/* dsn_vstring_update - update DSN and/or text */

DSN_VSTRING *dsn_vstring_update(DSN_VSTRING *dv, const char *dsn,
					  const char *format,...)
{
    va_list ap;
    size_t  len;

    if (dsn && *dsn) {
	if ((len = dsn_valid(dsn)) == 0)
	    msg_panic("dsn_vstring_update: bad dsn: \"%s\"", dsn);
	DSN_UPDATE(dv->dsn, dsn, len);
    }
    if (format && *format) {
	va_start(ap, format);
	vstring_vsprintf(dv->vstring, format, ap);
	va_end(ap);
    }
    return (dv);
}
