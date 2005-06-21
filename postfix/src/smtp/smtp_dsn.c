/*++
/* NAME
/*	smtp_dsn 3
/* SUMMARY
/*	application-specific DSN wrappers
/* SYNOPSIS
/*	#include <smtp.h>
/*
/*	void	smtp_dsn_update(dsb, mta_name, status, code, reply,
/*				reason_fmt, ...)
/*	SMTP_DSN	*dsb;
/*	const char *mta_name;
/*	const char *status;
/*	int	code;
/*	const char *reply;
/*	const char *reason_fmt;
/*
/*	void	vsmtp_dsn_update(dsb, mta_name, status, code, reply,
/*				reason_fmt, ap)
/*	SMTP_DSN	*dsb;
/*	const char *mta_name;
/*	const char *status;
/*	int	code;
/*	const char *reply;
/*	const char *reason_fmt;
/*	va_list	ap;
/*
/*	void	smtp_dsn_formal(dsb, mta_name, status, code, reply)
/*	DSN_BUF	*dsb;
/*	const char *mta_name;
/*	const char *status;
/*	int	code;
/*	const char *reply;
/*
/*	DSN	*SMTP_DSN_ASSIGN(dsn, mta_name, status, action, reply, reason)
/*	DSN	*dsn;
/*	const char *mta_name;
/*	const char *status;
/*	const char *action;
/*	const char *reply;
/*	const char *reason;
/* DESCRIPTION
/*	This module implements application-specific wrappers for
/*	the dsbuf(3) delivery status information module. The purpose
/*	of the wrappers is to eliminate clutter from the code.
/*
/*	smtp_dsn_update() updates the formal and informal delivery
/*	status attributes.
/*
/*	vsmtp_dsn_update() implements an alternative interface.
/*
/*	smtp_dsn_formal() updates the formal delivery status
/*	attributes and leaves the informal reason attribute unmodified.
/*
/*	SMTP_DSN_ASSIGN() is a wrapper around the DSN_ASSIGN macro.
/*
/*	Arguments:
/* .IP dsb
/*	Delivery status information. See dsbuf(3).
/* .IP mta_name
/*	The name of the MTA that issued the response given with the
/*	status and reply arguments. Specify DSN_BY_LOCAL_MTA for
/*	status and "reply" information that was issued by the local
/*	MTA.
/* .IP status
/*	RFC 3463 status code.
/* .IP code
/*	SMTP reply code.
/* .IP reply
/*	SMTP reply code followed by text. The bounce(8) server
/*	replaces non-printable characters by '?', so it is a good
/*	idea to replace embedded newline characters by spaces
/*	before using this module.
/* .IP reason_fmt
/*	Format string for the informal reason attribute.
/* .IP DIAGNOSTICS
/*	Fatal: out of memory. Panic: invalid arguments.
/* BUGS
/*	It seems wasteful to copy mostly-constant information into
/*	VSTRING buffers, but it's unavoidable if one needs to
/*	delegate work to a subordinate routine, and report the error
/*	after the subordinate has terminated.
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
#include <stdlib.h>			/* 44BSD stdarg.h uses abort() */
#include <stdarg.h>

/* Utility library. */

#include <vstring.h>

/* Global library. */

#include <dsn_buf.h>

/* Application-specific. */

#include <smtp.h>

/* smtp_dsn_update - update formal and informal DSN attributes */

void    smtp_dsn_update(DSN_BUF *why, const char *mta_name,
			        const char *status, int code,
			        const char *reply,
			        const char *format,...)
{
    va_list ap;

    va_start(ap, format);
    vsmtp_dsn_update(why, mta_name, status, code, reply, format, ap);
    va_end(ap);
}

/* vsmtp_dsn_update - update formal and informal DSN attributes */

void    vsmtp_dsn_update(DSN_BUF *why, const char *mta_name,
			         const char *status, int code,
			         const char *reply,
			         const char *format, va_list ap)
{
    dsb_formal(why, status, DSB_DEF_ACTION,
	       mta_name ? DSB_MTYPE_DNS : DSB_MTYPE_NONE,
	       mta_name, DSB_DTYPE_SMTP, code, reply);
    vstring_vsprintf(why->reason, format, ap);
}

/* smtp_dsn_formal - update formal DSN attributes only */

void    smtp_dsn_formal(DSN_BUF *why, const char *mta_name,
			        const char *status, int code,
			        const char *reply)
{
    dsb_formal(why, status, DSB_DEF_ACTION,
	       mta_name ? DSB_MTYPE_DNS : DSB_MTYPE_NONE,
	       mta_name, DSB_DTYPE_SMTP, code, reply);
}
