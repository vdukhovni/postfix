#ifndef _REWRITE_CLNT_H_INCLUDED_
#define _REWRITE_CLNT_H_INCLUDED_

/*++
/* NAME
/*	rewrite_clnt 3h
/* SUMMARY
/*	address rewriter client
/* SYNOPSIS
/*	#include <rewrite_clnt.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <vstring.h>

 /*
  * External interface.
  */
#define REWRITE_ADDR	"rewrite"
#define REWRITE_CANON	REWRITE_LOCAL	/* backwards compatibility */

 /*
  * XXX These should be moved to mail_proto.h because they appear as
  * attribute values in queue file records and delivery requests.
  */
#define REWRITE_LOCAL	"local"
#define REWRITE_REMOTE	"remote"
#define REWRITE_NONE	"none"

extern VSTRING *rewrite_clnt(const char *, const char *, VSTRING *);
extern VSTRING *rewrite_clnt_internal(const char *, const char *, VSTRING *);

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
