#ifndef _WATCH_FD_H_INCLUDED_
#define _WATCH_FD_H_INCLUDED_

/*++
/* NAME
/*	watch_fd 3h
/* SUMMARY
/*	monitor file descriptors for change
/* SYNOPSIS
/*	#include "watch_fd.h"
 DESCRIPTION
 .nf

 /*
  * External interface.
  */
extern void watch_fd_register(int);
extern void watch_fd_remove(int);
extern int watch_fd_changed(void);

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

#endif
