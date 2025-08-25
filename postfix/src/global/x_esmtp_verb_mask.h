#ifndef _ESMTP_VERB_H_INCLUDED_
#define _ESMTP_VERB_H_INCLUDED_

/*++
/* NAME
/*	x_esmtp_verb_mask 3h
/* SUMMARY
/*	Parse X-Esmtp-Verb header value
/* SYNOPSIS
/*	#include <x_esmtp_verb_mask.h>
/* DESCRIPTION
/* .nf

 /*
  * External API
  */
extern int x_esmtp_verb_mask(const char *);
extern const char *str_x_esmtp_verb_mask(int);

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

#endif
