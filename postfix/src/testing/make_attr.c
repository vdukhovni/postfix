/*++
/* NAME
/*	make_attr 3
/* SUMMARY
/*	create serialized attribute request or response
/* SYNOPSIS
/*	#include <make_attr.h>
/*
/*	VSTRING *make_attr(int flags, ...)
/* DESCRIPTION
/*	make_attr() creates a serialized request or response attribute
/*	list. The arguments are like attr_print().
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	Google, Inc.
/*	111 8th Avenue
/*	New York, NY 10011, USA
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>
#include <stdarg.h>

 /*
  * Utility library.
  */
#include <attr.h>
#include <vstring.h>

 /*
  * Test library.
  */
#include <ptest.h>
#include <make_attr.h>

/* make_attr - serialize attribute list */

VSTRING *make_attr(int flags,...)
{
    static const char myname[] = "make_attr";
    VSTRING *res = vstring_alloc(100);
    VSTREAM *stream;
    va_list ap;
    int     err;

    if ((stream = vstream_memopen(res, O_WRONLY)) == 0)
	ptest_fatal(ptest_ctx_current(), "%s: vstream_memopen: %m", myname);;
    va_start(ap, flags);
    err = attr_vprint(stream, flags, ap);
    va_end(ap);
    if (vstream_fclose(stream) != 0 || err)
	ptest_fatal(ptest_ctx_current(), "%s: write attributes: %m", myname);
    return (res);
}
