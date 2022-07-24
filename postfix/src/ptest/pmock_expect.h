#ifndef _PMOCK_EXPECT_H_INCLUDED_
#define _PMOCK_EXPECT_H_INCLUDED_

/*++
/* NAME
/*	pmock_expect 3h
/* SUMMARY
/*	mock test support
/* SYNOPSIS
/*	#include <pmock_expect.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <vstring.h>

 /*
  * Test library.
  */
#include <ptest.h>

 /*
  * Generic MOCK expectation parent class. Real mock applications will
  * subclass this, and add their own application-specific fields with
  * expected inputs and prepared outputs.
  */
typedef struct MOCK_EXPECT {
    char   *file;			/* __FILE__ */
    int     line;			/* __LINE__ */
    int     calls_expected;		/* expected count */
    int     calls_made;			/* actual count */
    struct MOCK_EXPECT *next;		/* linkage */
} MOCK_EXPECT;

 /*
  * Application signature with MOCK_EXPECT generic utility functions.
  */
typedef int (*MOCK_EXPECT_MATCH_FN) (const MOCK_EXPECT *, const MOCK_EXPECT *);
typedef void (*MOCK_EXPECT_ASSIGN_FN) (const MOCK_EXPECT *, void *);
typedef char *(*MOCK_EXPECT_PRNT_FN) (const MOCK_EXPECT *, VSTRING *);
typedef void (*MOCK_EXPECT_FREE_FN) (MOCK_EXPECT *);

 /*
  * Common information for all expectations of a specific mock application.
  */
typedef struct MOCK_APPL_SIG {
    const char *name;			/* application sans mock_ prefix */
    MOCK_EXPECT_MATCH_FN match_expect;	/* match expectation inputs */
    MOCK_EXPECT_ASSIGN_FN assign_expect;/* assign expectation outputs */
    MOCK_EXPECT_PRNT_FN print_expect;	/* print call or expectation */
    MOCK_EXPECT_FREE_FN free_expect;	/* destruct expectation */
} MOCK_APPL_SIG;

extern MOCK_EXPECT *pmock_expect_create(const MOCK_APPL_SIG *, const char *file,
					        int line, int calls_expected,
					        ssize_t);
extern int pmock_expect_apply(const MOCK_APPL_SIG *, const MOCK_EXPECT *, void *);
extern void pmock_expect_free(MOCK_EXPECT *);

 /*
  * Report unused expectations and destroy all evidence and expectations.
  */
extern void pmock_expect_wrapup(PTEST_CTX *);

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	Google, Inc.
/*	111 8th Avenue
/*	New York, NY 10011, USA
/*--*/

#endif
