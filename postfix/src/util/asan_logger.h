#ifndef _ASAN_LOGGER_H_INCLUDED_
#define _ASAN_LOGGER_H_INCLUDED_

/*++
/* NAME
/*	asan_logger 3h
/* SUMMARY
/*	log libasan errors
/* SYNOPSIS
/*	#include <asan_logger.h>
/* DESCRIPTION
/*	This file provides definitions that are needed only in files
/*	that provide a (non-test) main() function.
/* .nf

 /*
  * System library.
  */
#ifdef HAS_ASAN
#include <sys_defs.h>

 /*
  * Utility library.
  */
#include <msg.h>

 /*
  * ASAN library.
  */
#include <sanitizer/asan_interface.h>

static void asan_logger_cb(const char *text)
{
    msg_warn("address sanitizer: %s", text);
}

#define INIT_ASAN_LOGGER() \
	__asan_set_error_report_callback(asan_logger_cb)

#else
#define INIT_ASAN_LOGGER()		/* empty */
#endif					/* HAS_ASAN */

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

#endif					/* _ASAN_LOGGER_H_INCLUDED_ */
