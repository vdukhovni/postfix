/*++
/* NAME
/*	quote_821_local 3h
/* SUMMARY
/*	quote rfc 821 local part
/* SYNOPSIS
/*	#include "quote_821_local.h"
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <vstring.h>

 /*
  * External interface.
  */
extern VSTRING *quote_821_local(VSTRING *, char *);

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*
/*      Alterations for LMTP by:
/*      Philip A. Prindeville
/*      Mirapoint, Inc.
/*      USA.
/*
/*      Additional work on LMTP by:
/*      Amos Gouaux
/*      University of Texas at Dallas
/*      P.O. Box 830688, MC34
/*      Richardson, TX 75083, USA
/*--*/
