#ifndef _DEFS_FOR_MAIN_H_INCLUDED_
#define _DEFS_FOR_MAIN_H_INCLUDED_

/*++
/* NAME
/*	defs_for_main 3h
/* SUMMARY
/*	definitions for main() programs only
/* SYNOPSIS
/*	#include <defs_for_main.h>
/* DESCRIPTION
/*	This file provides definitions that are needed only in files
/*	that provide a (non-test) main() function.
/* .nf

 /*
  * System library.
  */
#include <sys_defs.h>

 /*
  * Optionally, log UBSAN findings.
  */
#include <ubsan_logger.h>

 /*
  * Optionally, log ASAN findings.
  */
#include <asan_logger.h>

 /*
  * Code to be called from main().
  */
#if defined(HAS_UBSAN) || defined(HAS_ASAN)
#define CODE_FOR_MAIN() do { \
	INIT_UBSAN_LOGGER(); \
	INIT_ASAN_LOGGER(); \
    } while (0)

#endif					/* HAS_UBSAN || HAS_ASAN */

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

#endif
