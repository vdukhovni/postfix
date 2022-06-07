#ifndef _DICT_MONGODB_INCLUDED_
#define _DICT_MONGODB_INCLUDED_

/*++
/* NAME
/*	dict_mongodb 3h
/* SUMMARY
/*	dictionary interface to mongodb databases
/* SYNOPSIS
/*	#include <dict_mongodb.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <dict.h>

 /*
  * External interface.
  */
#define DICT_TYPE_MONGODB "mongodb"

extern DICT *dict_mongodb_open(const char *, int, int);

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*  Hamid Maadani (hamid@dexo.tech)
/*  Dextrous Technologies, LLC
/*  P.O. Box 213
/*  5627 Kanan Rd.,
/*  Agoura Hills, CA
/*  
/* Based on the work done by:
/*  Stephan Ferraro, Aionda GmbH
/*  E-Mail: stephan@ferraro.net
/*--*/

#endif
