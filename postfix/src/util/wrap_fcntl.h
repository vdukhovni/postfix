#ifndef _WRAP_FCNTL_H_INCLUDED_
#define _WRAP_FCNTL_H_INCLUDED_

/*++
/* NAME
/*	wrap_fcntl 3h
/* SUMMARY
/*	mockable fcntl/flock wrappers
/* SYNOPSIS
/*	#include <wrap_fcntl.h>
/* DESCRIPTION
/* .nf

 /*
  * System library.
  */
#include <sys/file.h>
#include <fcntl.h>
#include <stdarg.h>

 /*
  * Mockable API.
  */
#ifndef NO_MOCK_WRAPPERS
extern int wrap_fcntl(int, int,...);
extern int wrap_flock(int, int);

#define fcntl(fd, cmd, ...)	wrap_fcntl(fd, cmd, __VA_ARGS__)
#define flock(fd, cmd)		wrap_flock(fd, cmd)
#endif

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

#endif
