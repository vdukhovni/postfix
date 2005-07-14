#ifndef _BOUNCE_LOG_H_INCLUDED_
#define _BOUNCE_LOG_H_INCLUDED_

/*++
/* NAME
/*	bounce_log 3h
/* SUMMARY
/*	bounce file reader
/* SYNOPSIS
/*	#include <bounce_log.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <vstream.h>
#include <vstring.h>

 /*
  * Global library.
  */
#include <recipient_list.h>
#include <rcpt_buf.h>
#include <dsn_buf.h>
#include <dsn.h>

 /*
  * External interface.
  */
typedef struct {
    /* Private. */
    VSTREAM *fp;			/* open file */
    VSTRING *buf;			/* I/O buffer */
    RCPT_BUF *rcpt_buf;			/* recipient info */
    DSN_BUF *dsn_buf;			/* delivery status */
    char    *compat_status;		/* old logfile compatibility */
    char    *compat_action;		/* old logfile compatibility */
    /* Public. */
    RECIPIENT rcpt;			/* recipient info */
    DSN     dsn;			/* delivery status */
} BOUNCE_LOG;

extern BOUNCE_LOG *bounce_log_open(const char *, const char *, int, mode_t);
extern BOUNCE_LOG *bounce_log_read(BOUNCE_LOG *);
extern BOUNCE_LOG *bounce_log_delrcpt(BOUNCE_LOG *);
extern BOUNCE_LOG *bounce_log_forge(RECIPIENT *, DSN *);
extern int bounce_log_close(BOUNCE_LOG *);

#define bounce_log_rewind(bp) vstream_fseek((bp)->fp, 0L, SEEK_SET)

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
