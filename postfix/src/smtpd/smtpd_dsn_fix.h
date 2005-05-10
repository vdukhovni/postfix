/*++
/* NAME
/*	smtpd_check 3h
/* SUMMARY
/*	SMTP client request filtering
/* SYNOPSIS
/*	#include "smtpd.h"
/*	#include "smtpd_check_int.h"
/* DESCRIPTION
/* .nf

 /*
  * Internal interface.
  */
#define SMTPD_NAME_CLIENT	"Client host"
#define SMTPD_NAME_CCERT	"Client certificate"
#define SMTPD_NAME_HELO		"Helo command"
#define SMTPD_NAME_SENDER	"Sender address"
#define SMTPD_NAME_RECIPIENT	"Recipient address"
#define SMTPD_NAME_ETRN		"Etrn command"
#define SMTPD_NAME_DATA		"Data command"
#define SMTPD_NAME_EOD		"End-of-data"

extern const char *smtpd_dsn_fix(const char *, const char *);

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
