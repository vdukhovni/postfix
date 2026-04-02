#ifndef _MOCK_SERVENT_H_INCLUDED_
#define _MOCK_SERVENT_H_INCLUDED_

/*++
/* NAME
/*	mock_servent 3h
/* SUMMARY
/*	getservbyname mock for hermetic tests
/* SYNOPSIS
/*	#include <mock_servent.h>
/* DESCRIPTION
/* .nf

 /*
  * System library.
  */
#include <sys_defs.h>
#include <wrap_netdb.h>

 /*
  * Test library.
  */
#include <ptest.h>

 /*
  * Manage expectations and responses. Capture the source file name and line
  * number for better diagnostics.
  */
#define expect_getservbyname(...) \
	_expect_getservbyname(__FILE__, __LINE__, __VA_ARGS__)

extern void _expect_getservbyname(const char *, int, int, struct servent *,
				          const char *, const char *);

#define expect_setservent(...) \
	_expect_setservent(__FILE__, __LINE__, __VA_ARGS__)

extern void _expect_setservent(const char *, int, int, int);

#define expect_endservent(...) \
	_expect_endservent(__FILE__, __LINE__, __VA_ARGS__)

extern void _expect_endservent(const char *, int, int);

 /*
  * Matcher predicates. Capture the source file name and line number for
  * better diagnostics.
  */
#define eq_servent(...) _eq_servent(__FILE__, __LINE__, __VA_ARGS__)

extern int _eq_servent(const char *, int,PTEST_CTX *, 
		               const char *, struct servent *,
		               struct servent *);

 /*
  * Helper to create test data.
  */
extern struct servent *make_servent(const char *, int, const char *);

extern void free_servent(struct servent *);

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	Google, Inc.
/*	111 8th Avenue
/*	New York, NY 10011, USA
/*
/*	Wietse Venema
/*	porcupine.org
/*--*/

#endif
