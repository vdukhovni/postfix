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
extern int mail_flush_enable(const char *);
extern int mail_flush_site(const char *);
extern int mail_flush_append(const char *, const char *);
extern void mail_flush_append_init(void);

 /*
  * Mail flush server requests.
  */
#define FLUSH_REQ_APPEND	"append"/* append queue ID to site log */
#define FLUSH_REQ_SEND		"send"	/* flush mail queued for site */
#define FLUSH_REQ_ENABLE	"enable"/* flush mail queued for site */

 /*
  * Mail flush server status codes.
  */
#define FLUSH_STAT_FAIL		-1	/* everyone */
#define FLUSH_STAT_OK		0	/* everyone */
#define FLUSH_STAT_UNKNOWN	2	/* mail_flush_site() only */
#define FLUSH_STAT_BAD		3	/* mail_flush_site() only */


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
