#ifndef _PLIFT_CLNT_H_INCLUDED_
#define _PLIFT_CLNT_H_INCLUDED_

/*++
/* NAME
/*	plift_clnt 3h
/* SUMMARY
/*	Interact with postlift service
/* SYNOPSIS
/*	#include <plift_clnt.h>
/* DESCRIPTION
/* .nf

 /*
  * System library.
  */
#include <sys_defs.h>

 /*
  * Utility library.
  */
#include <vstream.h>

 /*
  * External interface.
  */
#define PLIST_ENV_PEER_UID	"PEER_UID"
#define PLIST_REQ_MAXLEN	1024
#define PLIST_INIT_TIMEOUT	10

extern int plift_clnt_request(const char *, const char *, int, char **,
				        int, int, int, const char *);

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

#endif
