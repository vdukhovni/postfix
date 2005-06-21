#ifndef _DSN_H_INCLUDED_
#define _DSN_H_INCLUDED_

/*++
/* NAME
/*	dsn 3h
/* SUMMARY
/*	RFC-compliant delivery status information
/* SYNOPSIS
/*	#include <dsn.h>
/* DESCRIPTION
/* .nf

 /*
  * Global library.
  */
#include <dsn_buf.h>

 /*
  * External interface.
  */
typedef struct {
    const char *status;			/* RFC 3463 status */
    const char *action;			/* Null / RFC 3464 action */
    const char *reason;			/* descriptive reason */
    const char *dtype;			/* Null / RFC 3464 diagnostic type */
    const char *dtext;			/* Null / RFC 3464 diagnostic code */
    const char *mtype;			/* Null / RFC 3464 MTA type */
    const char *mname;			/* Null / RFC 3464 remote MTA */
} DSN;

 /*
  * Ditto, without const poisoning.
  */
typedef struct {
    char   *status;			/* RFC 3463 status */
    char   *action;			/* Null / RFC 3464 action */
    char   *reason;			/* descriptive reason */
    char   *dtype;			/* Null / RFC 3464 diagnostic type */
    char   *dtext;			/* Null / RFC 3464 diagnostic code */
    char   *mtype;			/* Null / RFC 3464 MTA type */
    char   *mname;			/* Null / RFC 3464 remote MTA */
} DSN_VAR;

extern DSN *dsn_create(const char *, const char *, const char *, const char *,
		               const char *, const char *, const char *);
extern void dsn_free(DSN *);

#define DSN_ASSIGN(dsn, _status, _action, _reason, _dtype, _dtext, _mtype, _mname) \
    (((dsn)->status = (_status)), \
     ((dsn)->action = (_action)), \
     ((dsn)->reason = (_reason)), \
     ((dsn)->dtype = (_dtype)), \
     ((dsn)->dtext = (_dtext)), \
     ((dsn)->mtype = (_mtype)), \
     ((dsn)->mname = (_mname)), \
     (dsn))

#define DSN_SIMPLE(dsn, _status, _reason) \
    (((dsn)->status = (_status)), \
     ((dsn)->action = 0), \
     ((dsn)->reason = (_reason)), \
     ((dsn)->dtype = 0), \
     ((dsn)->dtext = 0), \
     ((dsn)->mtype = 0), \
     ((dsn)->mname = 0), \
     (dsn))

#define DSN_SMTP(dsn, _status, _dtext, _reason) \
    (((dsn)->status = (_status)), \
     ((dsn)->action = 0), \
     ((dsn)->reason = (_reason)), \
     ((dsn)->dtype = DSB_DTYPE_SMTP), \
     ((dsn)->dtext = _dtext), \
     ((dsn)->mtype = 0), \
     ((dsn)->mname = 0), \
     (dsn))

#define DSN_NO_DTYPE	0
#define DSN_NO_DTEXT	0
#define DSN_NO_MTYPE	0
#define DSN_NO_MNAME	0

 /*
  * In order to save space in the queue manager, some DSN fields may be null
  * pointers so that we don't waste memory making copies of empty strings. In
  * addition, sanity requires that the status and reason are never null or
  * empty; this is enforced by dsn_create() which is invoked by DSN_COPY().
  * This complicates the bounce_log(3) and bounce(8) daemons, as well as the
  * server reply parsing code in the smtp(8) and lmtp(8) clients. They must
  * be able to cope with null pointers, and they must never supply empty
  * strings for the required fields.
  */
#define DSN_COPY(dsn) \
    dsn_create((dsn)->status, (dsn)->action, (dsn)->reason, \
	(dsn)->dtype, (dsn)->dtext, \
	(dsn)->mtype, (dsn)->mname)

#define DSN_STRING_OR_NULL(s) ((s)[0] ? (s) : 0)

#define DSN_FROM_DSN_BUF(dsn, dsb) \
    DSN_ASSIGN((dsn), vstring_str((dsb)->status), \
	DSN_STRING_OR_NULL(vstring_str((dsb)->action)), \
	vstring_str((dsb)->reason), \
	DSN_STRING_OR_NULL(vstring_str((dsb)->dtype)), \
	DSN_STRING_OR_NULL(vstring_str((dsb)->dtext)), \
	DSN_STRING_OR_NULL(vstring_str((dsb)->mtype)), \
	DSN_STRING_OR_NULL(vstring_str((dsb)->mname)))

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
