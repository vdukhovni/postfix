#ifndef _UGID_FROM_STRING_H_INCLUDED_
#define _UGID_FROM_STRING_H_INCLUDED_

/*++
/* NAME
/*	ugid_from_string 3h
/* SUMMARY
/*	Convert string to uid_t or gid_t
/* SYNOPSIS
/*	#include <ugid_from_string.h>
/* DESCRIPTION
/* .nf

 /*
  * External interface.
  */
extern int uid_from_string(const char *uid_str, uid_t *out);
extern int gid_from_string(const char *gid_str, gid_t *out);

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

#endif
