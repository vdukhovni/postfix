/*++
/* NAME
/*	midna 3
/* SUMMARY
/*	Postfix domain name conversion
/* SYNOPSIS
/*	#include <midna.h>
/*
/*	int midna_cache_size;
/*
/*	const char *midna_to_ascii(
/*	const char *name)
/*
/*	const char *midna_to_utf8(
/*	const char *name)
/*
/*	const char *midna_suffix_to_ascii(
/*	const char *name)
/*
/*	const char *midna_suffix_to_utf8(
/*	const char *name)
/* DESCRIPTION
/*	The functions in this module transform domain names from
/*	or to IDNA form. The result is cached to avoid repeated
/*	conversion of the same name.
/*
/*	midna_to_ascii() converts an UTF-8 or ASCII domain name to
/*	ASCII.  The result is a null pointer in case of error.  This
/*	function verifies that the result is a valid ASCII domainname.
/*
/*	midna_to_utf8() converts an UTF-8 or ASCII domain name to
/*	UTF-8.  The result is a null pointer in case of error.  This
/*	function verifies that the result converts to a valid ASCII
/*	domainname.
/*
/*	midna_suffix_to_ascii() and midna_suffix_to_utf8() take a
/*	name that starts with '.' and otherwise perform the same
/*	operations as midna_to_ascii() and midna_to_utf8().
/*
/*	midna_cache_size specifies the size of the conversion result
/*	cache.  This value is used only once, upon the first lookup
/*	request.
/* SEE ALSO
/*	msg(3) diagnostics interface
/* DIAGNOSTICS
/*	Fatal errors: memory allocation problem.
/*	Warnings: conversion error or result validation error.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Arnt Gulbrandsen
/*
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>
#include <string.h>
#include <ctype.h>

#ifndef NO_EAI
#include <unicode/uidna.h>

 /*
  * Utility library.
  */
#include <mymalloc.h>
#include <msg.h>
#include <ctable.h>
#include <stringops.h>
#include <valid_hostname.h>
#include <midna.h>

 /*
  * Application-specific.
  */
#define DEF_MIDNA_CACHE_SIZE	256

int     midna_cache_size = DEF_MIDNA_CACHE_SIZE;
static VSTRING *midna_buf;		/* x.suffix */

#define STR(x)	vstring_str(x)

/* midna_to_ascii_create - convert domain to ASCII */

static void *midna_to_ascii_create(const char *name, void *unused_context)
{
    static const char myname[] = "midna_to_ascii_create";
    char    buf[1024];			/* XXX */
    UErrorCode error = U_ZERO_ERROR;
    UIDNAInfo info = UIDNA_INFO_INITIALIZER;
    UIDNA  *idna;
    int     anl;

    /*
     * Paranoia: do not expose uidna_*() to unfiltered network data.
     */
    if (allascii(name) == 0 && valid_utf8_string(name, strlen(name)) == 0) {
	msg_warn("%s: Problem translating domain \"%s\" to ASCII form: %s",
		 myname, name, "malformed UTF-8");
	return (0);
    }

    /*
     * Perform the requested conversion.
     */
    idna = uidna_openUTS46(UIDNA_DEFAULT, &error);
    anl = uidna_nameToASCII_UTF8(idna,
				 name, strlen(name),
				 buf, sizeof(buf),
				 &info,
				 &error);
    uidna_close(idna);

    /*
     * Paranoia: verify that the result is a valid ASCII domain name. A quick
     * check shows that the UTS46 implementation will reject labels that
     * start or end in '-' or that are over-long, but let's play safe here.
     */
    if (U_SUCCESS(error) && info.errors == 0 && anl > 0) {
	buf[anl] = 0;				/* XXX */
	if (!valid_hostname(buf, DONT_GRIPE)) {
	    msg_warn("%s: Problem translating domain \"%s\" to ASCII form: %s",
		     myname, name, "malformed ASCII label(s)");
	    return (0);
	}
	return (mystrndup(buf, anl));
    } else {
	msg_warn("%s: Problem translating domain \"%s\" to ASCII form: %s",
		 myname, name, u_errorName(error));
	return (0);
    }
}

/* midna_to_utf8_create - convert domain to UTF8 */

static void *midna_to_utf8_create(const char *name, void *unused_context)
{
    static const char myname[] = "midna_to_utf8_create";
    char    buf[1024];			/* XXX */
    UErrorCode error = U_ZERO_ERROR;
    UIDNAInfo info = UIDNA_INFO_INITIALIZER;
    UIDNA  *idna;
    int     anl;

    /*
     * Paranoia: do not expose uidna_*() to unfiltered network data.
     */
    if (allascii(name) == 0 && valid_utf8_string(name, strlen(name)) == 0) {
	msg_warn("%s: Problem translating domain \"%s\" to UTF-8 form: %s",
		 myname, name, "malformed UTF-8");
	return (0);
    }

    /*
     * Perform the requested conversion.
     */
    idna = uidna_openUTS46(UIDNA_DEFAULT, &error);
    anl = uidna_nameToUnicodeUTF8(idna,
				  name, strlen(name),
				  buf, sizeof(buf),
				  &info,
				  &error);
    uidna_close(idna);

    /*
     * Paranoia: UTS46 toUTF8 will accept and produce a name that does not
     * convert to a valid ASCII domain name. So we enforce sanity here.
     */
    if (U_SUCCESS(error) && info.errors == 0 && anl > 0) {
	buf[anl] = 0;				/* XXX */
	if (midna_to_ascii(buf) == 0)
	    return (0);
	return (mystrndup(buf, anl));
    } else {
	msg_warn("%s: Problem translating domain \"%s\" to UTF8 form: %s",
		 myname, name, u_errorName(error));
	return (0);
    }
}

/* midna_cache_free - cache element destructor */

static void midna_cache_free(void *value, void *unused_context)
{
    if (value)
	myfree(value);
}

/* midna_to_ascii - convert name to ASCII */

const char *midna_to_ascii(const char *name)
{
    static CTABLE *midna_to_ascii_cache = 0;

    if (midna_to_ascii_cache == 0)
	midna_to_ascii_cache = ctable_create(midna_cache_size,
					     midna_to_ascii_create,
					     midna_cache_free,
					     (void *) 0);
    return (ctable_locate(midna_to_ascii_cache, name));
}

/* midna_to_utf8 - convert name to UTF8 */

const char *midna_to_utf8(const char *name)
{
    static CTABLE *midna_to_utf8_cache = 0;

    if (midna_to_utf8_cache == 0)
	midna_to_utf8_cache = ctable_create(midna_cache_size,
					    midna_to_utf8_create,
					    midna_cache_free,
					    (void *) 0);
    return (ctable_locate(midna_to_utf8_cache, name));
}

/* midna_suffix_to_ascii - convert .name to ASCII */

const char *midna_suffix_to_ascii(const char *suffix)
{
    const char *cache_res;

    /*
     * If prepending x to .name causes the result to become too long, then
     * the suffix is bad.
     */
    if (midna_buf == 0)
	midna_buf = vstring_alloc(100);
    vstring_sprintf(midna_buf, "x%s", suffix);
    if ((cache_res = midna_to_ascii(STR(midna_buf))) == 0)
	return (0);
    else
	return (cache_res + 1);
}

/* midna_suffix_to_utf8 - convert .name to UTF8 */

const char *midna_suffix_to_utf8(const char *name)
{
    const char *cache_res;

    /*
     * If prepending x to .name causes the result to become too long, then
     * the suffix is bad.
     */
    if (midna_buf == 0)
	midna_buf = vstring_alloc(100);
    vstring_sprintf(midna_buf, "x%s", name);
    if ((cache_res = midna_to_utf8(STR(midna_buf))) == 0)
	return (0);
    else
	return (cache_res + 1);
}

#ifdef TEST

 /*
  * Test program - reads names from stdin, reports invalid names to stderr.
  */
#include <stdlib.h>
#include <locale.h>

#include <stringops.h>			/* XXX temp_utf8_kludge */
#include <vstring.h>
#include <vstream.h>
#include <vstring_vstream.h>
#include <msg_vstream.h>

int     main(int argc, char **argv)
{
    VSTRING *buffer = vstring_alloc(1);
    const char *bp;
    const char *ascii;
    const char *utf8;

    if (setlocale(LC_ALL, "C") == 0)
	msg_fatal("setlocale(LC_ALL, C) failed: %m");

    msg_vstream_init(argv[0], VSTREAM_ERR);
    msg_verbose = 1;
    temp_utf8_kludge = 1;

    while (vstring_fgets_nonl(buffer, VSTREAM_IN)) {
	bp = STR(buffer);
	msg_info("> %s", bp);
	while (ISSPACE(*bp))
	    bp++;
	if (*bp == '#' || *bp == 0)
	    continue;
	if (!allascii(bp)) {
	    utf8 = midna_to_utf8(bp);
	    if (utf8 != 0)
		msg_info("\"%s\" ->utf8 \"%s\"", bp, utf8);
	    ascii = midna_to_ascii(bp);
	    if (ascii != 0) {
		msg_info("\"%s\" ->ascii \"%s\"", bp, ascii);
		utf8 = midna_to_utf8(ascii);
		if (utf8 != 0) {
		    msg_info("\"%s\" ->ascii \"%s\" ->utf8 \"%s\"",
			     bp, ascii, utf8);
		    if (strcmp(utf8, bp) != 0)
			msg_warn("\"%s\" != \"%s\"", bp, utf8);
		}
	    }
	} else {
	    ascii = midna_to_ascii(bp);
	    if (ascii != 0)
		msg_info("\"%s\" ->ascii \"%s\"", bp, ascii);
	    utf8 = midna_to_utf8(bp);
	    if (utf8 != 0) {
		msg_info("\"%s\" ->utf8 \"%s\"", bp, utf8);
		ascii = midna_to_ascii(utf8);
		if (ascii != 0) {
		    msg_info("\"%s\" ->utf8 \"%s\" ->ascii \"%s\"",
			     bp, utf8, ascii);
		    if (strcmp(ascii, bp) != 0)
			msg_warn("\"%s\" != \"%s\"", bp, ascii);
		}
	    }
	}
    }
    exit(0);
}

#endif					/* TEST */

#endif					/* NO_EAI */
