#ifndef _CRATE_CLNT_H_INCLUDED_
#define _CRATE_CLNT_H_INCLUDED_

/*++
/* NAME
/*	crate_clnt 3h
/* SUMMARY
/*	connection rate client interface
/* SYNOPSIS
/*	#include <crate_clnt.h>
/* DESCRIPTION
/* .nf

 /*
  * System library.
  */
#include <stdarg.h>

 /*
  * Utility library.
  */
#include <attr_clnt.h>

 /*
  * Protocol interface: requests and endpoints.
  */
#define CRATE_SERVICE		"crate"
#define CRATE_CLASS		"private"

#define CRATE_ATTR_REQ		"request"
#define CRATE_REQ_CONN		"connect"
#define CRATE_REQ_DISC		"disconnect"
#define CRATE_REQ_LOOKUP	"lookup"
#define CRATE_ATTR_IDENT	"ident"
#define CRATE_ATTR_COUNT	"count"
#define CRATE_ATTR_RATE		"rate"
#define CRATE_ATTR_STATUS	"status"

#define CRATE_STAT_OK		0
#define CRATE_STAT_FAIL		(-1)

 /*
  * Functional interface.
  */
typedef struct CRATE_CLNT CRATE_CLNT;

extern CRATE_CLNT *crate_clnt_create(void);
extern int crate_clnt_connect(CRATE_CLNT *, const char *, const char *, int *, int *);
extern int crate_clnt_disconnect(CRATE_CLNT *, const char *, const char *);
extern void crate_clnt_free(CRATE_CLNT *);

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
