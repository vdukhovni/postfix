#ifndef _MAIL_ATTR_H_INCLUDED_
#define _MAIL_ATTR_H_INCLUDED_

/*++
/* NAME
/*	mail_attr 3h
/* SUMMARY
/*	mail internal IPC support
/* SYNOPSIS
/*	#include <mail_attr.h>
/* DESCRIPTION
/* .nf

 /*
  * Request attribute. Values are defined by individual applications.
  */
#define MAIL_REQUEST	"request"

 /*
  * Request completion status.
  */
#define MAIL_STATUS		"status"
#define MAIL_STAT_OK		"success"
#define MAIL_STAT_FAIL		"failed"
#define MAIL_STAT_RETRY		"retry"
#define MAIL_STAT_REJECT	"reject"

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
