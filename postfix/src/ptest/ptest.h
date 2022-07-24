#ifndef _PTEST_H_INCLUDED_
#define _PTEST_H_INCLUDED_

/*++
/* NAME
/*	ptest 3h
/* SUMMARY
/*	run-time test support
/* SYNOPSIS
/*	#include <ptest.h>
/* DESCRIPTION
/* .nf

 /*
  * System library.
  */
#include <sys_defs.h>
#include <setjmp.h>

 /*
  * Utility library.
  */
#include <argv.h>
#include <msg.h>			/* XXX for MSG_JMP_BUF in PTEST_RUN */
#include <vstream.h>
#include <vstring.h>

 /*
  * TODO: factor out, and merge with DICT_JMP_BUF, MSG_JMP_BUF,
  * SLMDB_JMP_BUF, VSTREAM_JMP_BUF.
  */
#ifdef NO_SIGSETJMP
#define TEST_JMP_BUF jmp_buf
#define ptest_longjmp(bufp, val)	longjmp((bufp)[0], (val))
#else
#define TEST_JMP_BUF sigjmp_buf
#define ptest_longjmp(bufp, val)	siglongjmp((bufp)[0], (val))
#endif

 /*
  * All run-time test info in one place.
  */
typedef void (*PTEST_DEFER_FN) (void *);

#define PTEST_CTX_FLAG_SKIP	(1<<0)	/* This test is skipped */
#define PTEST_CTX_FLAG_FAIL	(1<<1)	/* This test has failed */

typedef struct PTEST_CTX {
    /* ptest_ctx.c */
    char   *name;			/* Null, name, or name/name/... */
    TEST_JMP_BUF *jbuf;			/* Used by ptest_fatal(), msg(3) */
    struct PTEST_CTX *parent;		/* In case tests are nested */
    int     flags;			/* See above */
    /* ptest_run.c */
    int     sub_pass;			/* Subtests that passed */
    int     sub_fail;			/* Subtests that failed */
    int     sub_skip;			/* Subtests that were skipped */
    PTEST_DEFER_FN defer_fn;		/* To be called after test... */
    void   *defer_ctx;			/* ...with this argument */
    /* ptest_error.c */
    VSTREAM *err_stream;		/* Output stream */
    VSTRING *err_buf;			/* Formatting buffer */
    ARGV   *allow_errors;		/* Allowed errors */
    /* ptest_log.c */
    VSTRING *log_buf;			/* Formatting buffer */
    ARGV   *allow_logs;			/* Allowed logs */
} PTEST_CTX;

 /*
  * ptest_ctx.c
  */
extern PTEST_CTX *ptest_ctx_create(const char *, TEST_JMP_BUF *);
extern PTEST_CTX *ptest_ctx_current(void);
extern void ptest_ctx_free(PTEST_CTX *);

 /*
  * ptest_error.c
  */
extern void ptest_error_setup(PTEST_CTX *, VSTREAM *);
extern void expect_ptest_error(PTEST_CTX *, const char *);
extern void PRINTFLIKE(2, 3) ptest_info(PTEST_CTX *, const char *,...);
extern void PRINTFLIKE(2, 3) ptest_error(PTEST_CTX *, const char *,...);
extern NORETURN PRINTFLIKE(2, 3) ptest_fatal(PTEST_CTX *, const char *,...);
extern int ptest_error_wrapup(PTEST_CTX *);

 /*
  * ptest_log.c
  */
extern void ptest_log_setup(PTEST_CTX *);
extern void expect_ptest_log_event(PTEST_CTX *, const char *);
extern void ptest_log_wrapup(PTEST_CTX *);

 /*
  * ptest_run.c
  */
extern void ptest_run_prolog(PTEST_CTX *);
extern void ptest_run_epilog(PTEST_CTX *, PTEST_CTX *);
extern NORETURN ptest_skip(PTEST_CTX *);
extern NORETURN ptest_return(PTEST_CTX *);
extern void ptest_defer(PTEST_CTX *, PTEST_DEFER_FN, void *);

#define PTEST_RUN(t, test_name, body_in_braces) do { \
    MSG_JMP_BUF new_buf; \
    PTEST_CTX *parent = t; \
    t = ptest_ctx_create((test_name), &new_buf); \
    ptest_run_prolog(t); \
    if (msg_setjmp(&new_buf) == 0) { \
        body_in_braces \
    } \
    msg_resetjmp(parent->jbuf); \
    ptest_run_epilog(t, parent); \
    ptest_ctx_free(t); \
    t = parent; \
} while (0)

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
