#ifndef _MAIL_FLUSH_H_INCLUDED_
#define _MAIL_FLUSH_H_INCLUDED_

/*++
/* NAME
/*	mail_flush 3h
/* SUMMARY
/*	flush backed up mail
/* SYNOPSIS
/*	#include <mail_flush.h>
/* DESCRIPTION
/* .nf

 /*
  * External interface.
  */
extern int mail_flush_deferred(void);
extern int mail_flush_site(const char *);
extern int mail_flush_append(const char *, const char *);

 /*
  * Mail flush server requests.
  */
#define FLUSH_REQ_ADD		"add"	/* add queue ID to site log */
#define FLUSH_REQ_SEND		"send"	/* flush mail queued for site */

 /*
  * Mail flush server status codes.
  */
#define FLUSH_STAT_FAIL		-1	/* everyone */
#define FLUSH_STAT_OK		0	/* everyone */
#define FLUSH_STAT_UNKNOWN	2	/* mail_flush_site() only */
#define FLUSH_STAT_BAD		3	/* mail_flush_site() only */


/* LICENSE
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/
/**INDENT** Error@33: Unmatched #endif */

#endif
