#ifndef _POSTLIFT_CREDS_H_INCLUDED_
#define _POSTLIFT_CREDS_H_INCLUDED_

/*++
/* NAME
/*	postlift_creds 3h
/* SUMMARY
/*	postlift security zone hygiene
/* SYNOPSIS
/*	#include <postlift_creds.h>
/* DESCRIPTION
/* .nf

 /*
  * External interface.
  */
extern void plift_valid_creds_or_die(uid_t my_uid, gid_t mygid);

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

#endif
