/*++
/* NAME
/*	rcpt_buf
/* SUMMARY
/*	recipient buffer manager
/* SYNOPSIS
/*	#include <rcpt_buf.h>
/*
/*	typedef struct {
/* .in +4
/*		VSTRING *address;	/* final recipient */
/*		VSTRING *orig_addr;	/* original recipient */
/*		VSTRING *dsn_orcpt;	/* dsn original recipient */
/*		int     dsn_notify;	/* DSN notify flags */
/*		long    offset;		/* REC_TYPE_RCPT byte */
/* .in -4
/*	} RCPT_BUF;
/*
/*	RCPT_BUF *rcpb_create(void)
/*
/*	void	rcpb_free(rcpt)
/*	RCPT_BUF *rcpt;
/*
/*	int	rcpb_scan(stream, flags, ptr)
/*	VSTREAM *stream;
/*	int	flags;
/*	void	*ptr;
/* DESCRIPTION
/*	rcpb_scan() reads a recipient buffer from the named stream
/*	using the default attribute scan routines. This function
/*	is meant to be passed as a call-back to attr_scan(), thusly:
/*
/*	... ATTR_SCAN_FUNC, rcpb_scan, (void *) rcpt_buf, ...
/*
/*	rcpb_create() and rcpb_free() create and destroy
/*	recipient buffer instances.
/* DIAGNOSTICS
/*	Fatal: out of memory.
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

/* Syste, library. */

#include <sys_defs.h>

/* Utility library. */

#include <mymalloc.h>
#include <vstring.h>
#include <vstream.h>

/* Global library. */

#include <mail_proto.h>
#include <rcpt_buf.h>

/* Application-specific. */

/* rcpb_create - create recipient buffer */

RCPT_BUF *rcpb_create(void)
{
    RCPT_BUF *rcpt;

    rcpt = (RCPT_BUF *) mymalloc(sizeof(*rcpt));
    rcpt->offset = 0;
    rcpt->dsn_orcpt = vstring_alloc(10);
    rcpt->dsn_notify = 0;
    rcpt->orig_addr = vstring_alloc(10);
    rcpt->address = vstring_alloc(10);
    return (rcpt);
}

/* rcpb_free - destroy recipient buffer */

void    rcpb_free(RCPT_BUF *rcpt)
{
    vstring_free(rcpt->dsn_orcpt);
    vstring_free(rcpt->orig_addr);
    vstring_free(rcpt->address);
    myfree((char *) rcpt);
}

/* rcpb_scan - receive recipient buffer */

int     rcpb_scan(VSTREAM *fp, int flags, void *ptr)
{
    RCPT_BUF *rcpt = (RCPT_BUF *) ptr;
    int     ret;

    /*
     * As with DSN, the order of attributes is determined by historical
     * compatibility and can be fixed after all the ad-hoc read/write code is
     * replaced.
     */
    ret = attr_scan(fp, flags | ATTR_FLAG_MORE,
		    ATTR_TYPE_STR, MAIL_ATTR_ORCPT, rcpt->orig_addr,
		    ATTR_TYPE_STR, MAIL_ATTR_RECIP, rcpt->address,
		    ATTR_TYPE_LONG, MAIL_ATTR_OFFSET, &rcpt->offset,
		    ATTR_TYPE_STR, MAIL_ATTR_DSN_ORCPT, rcpt->dsn_orcpt,
		    ATTR_TYPE_NUM, MAIL_ATTR_DSN_NOTIFY, &rcpt->dsn_notify,
		    ATTR_TYPE_END);
    return (ret == 5 ? 1 : -1);
}
