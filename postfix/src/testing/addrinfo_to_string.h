#ifndef _ADDRINFO_TO_STRING_H_INCLUDED_
#define _ADDRINFO_TO_STRING_H_INCLUDED_

/*++
/* NAME
/*	addrinfo_to_string 3h
/* SUMMARY
/*	address info to string conversion
/* SYNOPSIS
/*	#include <addrinfo_to_string.h>
/* DESCRIPTION
/* .nf

 /*
  * System library.
  */
#include <sys_defs.h>
#include <wrap_netdb.h>

 /*
  * Utility library.
  */
#include <vstring.h>

 /*
  * External interface.
  */
extern const char *pf_to_string(int);
extern const char *af_to_string(int);
extern const char *socktype_to_string(int);
extern const char *ipprotocol_to_string(int);
extern const char *ai_flags_to_string(VSTRING *, int);
extern const char *ni_flags_to_string(VSTRING *, int);
extern char *append_addrinfo_to_string(VSTRING *, const struct addrinfo *);
extern char *addrinfo_hints_to_string(VSTRING *, const struct addrinfo *);
extern char *sockaddr_to_string(VSTRING *, const struct sockaddr *, size_t);

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

#endif
