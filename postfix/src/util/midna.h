#ifndef _MIDNA_H_INCLUDED_
#define _MIDNA_H_INCLUDED_

/*++
/* NAME
/*	mail_idna 3h
/* SUMMARY
/*	domain name conversion
/* SYNOPSIS
/*	#include <mail_idna.h>
/* DESCRIPTION
/* .nf

 /*
  * External interface.
  */
extern const char *midna_utf8_to_ascii(const char *);
extern const char *midna_ascii_to_utf8(const char *);

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Arnt Gulbrandsen
/*
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

#endif
