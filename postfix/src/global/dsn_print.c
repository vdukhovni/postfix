/*++
/* NAME
/*	dsn_print
/* SUMMARY
/*	write DSN structure to stream
/* SYNOPSIS
/*	#include <dsn_print.h>
/*
/*	int	dsn_print(stream, flags, ptr)
/*	VSTREAM *stream;
/*	int	flags;
/*	void	*ptr;
/* DESCRIPTION
/*	dsn_print() writes a DSN structure to the named stream using
/*	the default attribute print routines. This function is meant
/*	to be passed as a call-back to attr_print(), thusly:
/*
/*	... ATTR_PRINT_FUNC, dsn_print, (void *) dsn, ...
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

/* System library. */

#include <sys_defs.h>

/* Utility library. */

#include <attr.h>

/* Global library. */

#include <mail_proto.h>
#include <dsn_print.h>

/* dsn_print - write DSN to stream */

int     dsn_print(VSTREAM *fp, int flags, void *ptr)
{
    DSN    *dsn = (DSN *) ptr;
    int     ret;

#define S(s) ((s) ? (s) : "")

    /*
     * The attribute order is determined by backwards compatibility. It can
     * be sanitized after all the ad-hoc DSN read/write code is replaced.
     */
    ret = attr_print(fp, flags | ATTR_FLAG_MORE,
		     ATTR_TYPE_STR, MAIL_ATTR_DSN_STATUS, dsn->status,
		     ATTR_TYPE_STR, MAIL_ATTR_DSN_DTYPE, S(dsn->dtype),
		     ATTR_TYPE_STR, MAIL_ATTR_DSN_DTEXT, S(dsn->dtext),
		     ATTR_TYPE_STR, MAIL_ATTR_DSN_MTYPE, S(dsn->mtype),
		     ATTR_TYPE_STR, MAIL_ATTR_DSN_MNAME, S(dsn->mname),
		     ATTR_TYPE_STR, MAIL_ATTR_DSN_ACTION, S(dsn->action),
		     ATTR_TYPE_STR, MAIL_ATTR_WHY, dsn->reason,
		     ATTR_TYPE_END);
    return (ret);
}
