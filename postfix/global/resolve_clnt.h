#ifndef _RESOLVE_CLNT_H_INCLUDED_
#define _RESOLVE_CLNT_H_INCLUDED_

/*++
/* NAME
/*	resolve_clnt 3h
/* SUMMARY
/*	address resolver client
/* SYNOPSIS
/*	#include <resolve_clnt.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <vstring.h>

 /*
  * External interface.
  */
#define RESOLVE_ADDR	"resolve"

typedef struct RESOLVE_REPLY {
    VSTRING *transport;
    VSTRING *nexthop;
    VSTRING *recipient;
} RESOLVE_REPLY;

extern void resolve_clnt_init(RESOLVE_REPLY *);
extern void resolve_clnt_query(const char *, RESOLVE_REPLY *);
extern void resolve_clnt_free(RESOLVE_REPLY *);

#define RESOLVE_CLNT_ASSIGN(reply, transport, nexthop, recipient) { \
	(reply).transport = (transport); \
	(reply).nexthop = (nexthop); \
	(reply).recipient = (recipient); \
    }

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
