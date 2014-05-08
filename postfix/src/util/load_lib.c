/*++
/* NAME
/*	load_lib 3
/* SUMMARY
/*	library loading wrappers
/* SYNOPSIS
/*	#include <load_lib.h>
/*
/*	extern int  load_library_symbols(const char *, LIB_FN *, LIB_FN *);
/*	const char *libname;
/*	LIB_FN     *libfuncs;
/*	LIB_FN     *libdata;
/*
/* DESCRIPTION
/*	This module loads functions from libraries, returning pointers
/*	to the named functions.
/*
/*	load_library_symbols() loads all of the desired functions, and
/*	returns zero for success, or exits via msg_fatal().
/*
/* SEE ALSO
/*	msg(3) diagnostics interface
/* DIAGNOSTICS
/*	Problems are reported via the msg(3) diagnostics routines:
/*	library not found, symbols not found, other fatal errors.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	LaMont Jones
/*	Hewlett-Packard Company
/*	3404 Harmony Road
/*	Fort Collins, CO 80528, USA
/*
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System libraries. */

#include "sys_defs.h"
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#if defined(HAS_DLOPEN)
#include <dlfcn.h>
#elif defined(HAS_SHL_LOAD)
#include <dl.h>
#endif

/* Application-specific. */

#include "msg.h"
#include "load_lib.h"

int     load_library_symbols(const char *libname, LIB_FN *libfuncs,
			             LIB_FN *libdata)
{
    static const char *myname = "load_library_symbols";
    LIB_FN *fn;

#if defined(HAS_DLOPEN)
    void   *handle;
    char   *emsg;

    if ((handle = dlopen(libname, RTLD_NOW)) == 0) {
	emsg = dlerror();
	msg_fatal("%s: dlopen failure loading %s: %s", myname, libname, emsg);
    }
    if (libfuncs) {
	for (fn = libfuncs; fn->name; fn++) {
	    if ((*(fn->ptr) = dlsym(handle, fn->name)) == 0) {
		emsg = dlerror();
		msg_fatal("%s: dlsym failure looking up %s in %s: %s", myname,
			  fn->name, libname, emsg);
	    }
	    if (msg_verbose > 1) {
		msg_info("loaded %s = %lx", fn->name, *((long *) (fn->ptr)));
	    }
	}
    }
    if (libdata) {
	for (fn = libdata; fn->name; fn++) {
	    if ((*(fn->ptr) = dlsym(handle, fn->name)) == 0) {
		emsg = dlerror();
		msg_fatal("%s: dlsym failure looking up %s in %s: %s", myname,
			  fn->name, libname, emsg);
	    }
	    if (msg_verbose > 1) {
		msg_info("loaded %s = %lx", fn->name, *((long *) (fn->ptr)));
	    }
	}
    }
#elif defined(HAS_SHL_LOAD)
    shl_t   handle;

    handle = shl_load(libname, BIND_IMMEDIATE, 0);

    if (libfuncs) {
	for (fn = libfuncs; fn->name; fn++) {
	    if (shl_findsym(&handle, fn->name, TYPE_PROCEDURE, fn->ptr) != 0) {
		msg_fatal("%s: shl_findsym failure looking up %s in %s: %m",
			  myname, fn->name, libname);
	    }
	    if (msg_verbose > 1) {
		msg_info("loaded %s = %x", fn->name, *((long *) (fn->ptr)));
	    }
	}
    }
    if (libdata) {
	for (fn = libdata; fn->name; fn++) {
	    if (shl_findsym(&handle, fn->name, TYPE_DATA, fn->ptr) != 0) {
		msg_fatal("%s: shl_findsym failure looking up %s in %s: %m",
			  myname, fn->name, libname);
	    }
	    if (msg_verbose > 1) {
		msg_info("loaded %s = %x", fn->name, *((long *) (fn->ptr)));
	    }
	}
    }
#else
    msg_fatal("%s: need dlopen or shl_load support for dynamic libraries",
	      myname);
#endif
    return 0;
}
