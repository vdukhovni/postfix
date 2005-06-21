#ifndef _DSN_BUF_H_INCLUDED_
#define _DSN_BUF_H_INCLUDED_

/*++
/* NAME
/*	dsbuf 3h
/* SUMMARY
/*	DSN support routines
/* SYNOPSIS
/*	#include <dsn_buf.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <vstring.h>

 /*
  * Delivery status buffer, Postfix-internal form.
  */
typedef struct {
    VSTRING *status;			/* RFC 3463 */
    VSTRING *action;			/* RFC 3464 */
    VSTRING *mtype;			/* null or remote MTA type */
    VSTRING *mname;			/* null or remote MTA name */
    VSTRING *dtype;			/* null, smtp, x-unix-command */
    int     dcode;			/* null, RFC 2821, sysexits.h */
    VSTRING *dtext;			/* null, RFC 2821, sysexits.h */
    VSTRING *reason;			/* free text */
} DSN_BUF;

#define DSB_DEF_ACTION	((char *) 0)

#define DSB_SKIP_RMTA	((char *) 0), ((char *) 0)
#define DSB_MTYPE_NONE	((char *) 0)
#define DSB_MTYPE_DNS	"dns"		/* RFC 2821 */

#define DSB_SKIP_REPLY	(char *) 0, (int) 0, " "	/* XXX Bogus? */
#define DSB_DTYPE_NONE	((char *) 0)
#define DSB_DTYPE_SMTP	"smtp"		/* RFC 2821 */
#define DSB_DTYPE_UNIX	"x-unix"	/* sysexits.h */
#define DSB_DTYPE_SASL	"x-sasl"	/* libsasl */

extern DSN_BUF *dsb_create(void);
extern DSN_BUF *PRINTFLIKE(9, 10) dsb_update(DSN_BUF *, const char *, const char *, const char *, const char *, const char *, int, const char *, const char *,...);
extern DSN_BUF *PRINTFLIKE(3, 4) dsb_simple(DSN_BUF *, const char *, const char *,...);
extern DSN_BUF *PRINTFLIKE(5, 6) dsb_smtp(DSN_BUF *, const char *, int, const char *, const char *,...);
extern DSN_BUF *PRINTFLIKE(5, 6) dsb_unix(DSN_BUF *, const char *, int, const char *, const char *,...);
extern DSN_BUF *PRINTFLIKE(5, 6) dsb_smtp(DSN_BUF *, const char *, int, const char *, const char *,...);
extern DSN_BUF *dsb_formal(DSN_BUF *, const char *, const char *, const char *, const char *, const char *, int, const char *);
extern DSN_BUF *dsb_status(DSN_BUF *, const char *);
extern void dsb_reset(DSN_BUF *);
extern void dsb_free(DSN_BUF *);

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
