#ifndef _POSTLIFT_SIG_H_INCLUDED_
#define _POSTLIFT_SIG_H_INCLUDED_

/*++
/* NAME
/*	postlift_sig 3h
/* SUMMARY
/*	handle postlift child death events
/* SYNOPSIS
/*	#include <postlift_sig.h>
/* DESCRIPTION
/* .nf

 /*
  * External interface.
  */
extern int plift_gotsigchld;
extern void plift_sigsetup(void);

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

#endif
