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
/*	const char *midna_utf8_to_ascii(
/*	const char *name)
/* DESCRIPTION
/*	The functions in this module transform domain names from
/*	or to IDNA form. The result is cached to avoid repeated
/*	conversion of the same name.
/*
/*	midna_utf8_to_ascii() converts an UTF-8 domain name to
/*	ASCII.  The result is a null pointer in case of error.
/*
/*	midna_cache_size specifies the size of the conversion result
/*	cache.  This value is used only once, upon the first lookup
/*	request.
/* SEE ALSO
/*	msg(3) diagnostics interface
/* DIAGNOSTICS
/*	Fatal errors: memory allocation problem.
/*	Warnings: conversion error.
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

#ifndef NO_EAI
#include <unicode/uidna.h>

 /*
  * Utility library.
  */
#include <mymalloc.h>
#include <msg.h>
#include <ctable.h>
#include <stringops.h>
#include <midna.h>

 /*
  * Application-specific.
  */
#define DEF_MIDNA_CACHE_SIZE	100

int     midna_cache_size = DEF_MIDNA_CACHE_SIZE;

/* midna_utf8_to_ascii_create - convert UTF8 domain to ASCII */

static void *midna_utf8_to_ascii_create(const char *name, void *unused_context)
{
    const char myname[] = "midna_utf8_to_ascii_create";
    char    buf[1024];			/* XXX */
    UErrorCode error = U_ZERO_ERROR;
    UIDNAInfo info = UIDNA_INFO_INITIALIZER;
    UIDNA  *idna;
    int     anl;

    /*
     * Paranoia: do not expose uidna_*() to unfiltered network data.
     */
    if (valid_utf8_string(name, strlen(name)) == 0) {
	msg_warn("%s: Problem translating domain \"%s\" to IDNA form: %s",
		 myname, name, "malformed UTF-8");
	return (0);
    }
    idna = uidna_openUTS46(UIDNA_DEFAULT, &error);
    anl = uidna_nameToASCII_UTF8(idna,
				 name, strlen(name),
				 buf, sizeof(buf),
				 &info,
				 &error);
    uidna_close(idna);
    if (U_SUCCESS(error) && info.errors == 0 && anl > 0) {
	return (mystrndup(buf, anl));
    } else {
	msg_warn("%s: Problem translating domain \"%s\" to IDNA form: %s",
		 myname, name, u_errorName(error));
	return (0);
    }
}

/* midna_utf8_to_ascii_free - cache element destructor */

static void midna_utf8_to_ascii_free(void *value, void *unused_context)
{
    if (value)
	myfree(value);
}

/* midna_utf8_to_ascii - convert UTF8 hostname to ASCII */

const char *midna_utf8_to_ascii(const char *name)
{
    static CTABLE *midna_utf8_to_ascii_cache = 0;

    if (midna_utf8_to_ascii_cache == 0)
	midna_utf8_to_ascii_cache = ctable_create(midna_cache_size,
						  midna_utf8_to_ascii_create,
						  midna_utf8_to_ascii_free,
						  (void *) 0);
    return (ctable_locate(midna_utf8_to_ascii_cache, name));
}

#ifdef TEST

 /*
  * Test program - reads hostnames from stdin, reports invalid hostnames to
  * stderr.
  */
#include <stdlib.h>

#include <stringops.h>			/* XXX temp_utf8_kludge */
#include <vstring.h>
#include <vstream.h>
#include <vstring_vstream.h>
#include <msg_vstream.h>

int     main(int argc, char **argv)
{
    VSTRING *buffer = vstring_alloc(1);
    const char *res;

    msg_vstream_init(argv[0], VSTREAM_ERR);
    msg_verbose = 1;
    temp_utf8_kludge = 1;

    while (vstring_fgets_nonl(buffer, VSTREAM_IN)) {
	msg_info("testing: \"%s\"", vstring_str(buffer));
	res = midna_utf8_to_ascii(vstring_str(buffer));
	if (res != 0)
	    msg_info("result: \"%s\"", res);
    }
    exit(0);
}

#endif

#endif
