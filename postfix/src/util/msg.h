#ifndef _MSG_H_INCLUDED_
#define _MSG_H_INCLUDED_

/*++
/* NAME
/*	msg 3h
/* SUMMARY
/*	diagnostics interface
/* SYNOPSIS
/*	#include "msg.h"
/* DESCRIPTION
/*	.nf

/*
 * System library.
 */
#include <stdarg.h>
#include <time.h>
#include <setjmp.h>

/*
 * External interface.
 */
typedef void (*MSG_CLEANUP_FN) (void);

extern int msg_verbose;

extern void PRINTFLIKE(1, 2) msg_info(const char *,...);
extern void PRINTFLIKE(1, 2) msg_warn(const char *,...);
extern void PRINTFLIKE(1, 2) msg_error(const char *,...);
extern NORETURN PRINTFLIKE(1, 2) msg_fatal(const char *,...);
extern NORETURN PRINTFLIKE(2, 3) msg_fatal_status(int, const char *,...);
extern NORETURN PRINTFLIKE(1, 2) msg_panic(const char *,...);

extern void vmsg_info(const char *, va_list);
extern void vmsg_warn(const char *, va_list);
extern void vmsg_error(const char *, va_list);
extern NORETURN vmsg_fatal(const char *, va_list);
extern NORETURN vmsg_fatal_status(int, const char *, va_list);
extern NORETURN vmsg_panic(const char *, va_list);

extern int msg_error_limit(int);
extern void msg_error_clear(void);
extern MSG_CLEANUP_FN msg_cleanup(MSG_CLEANUP_FN);

extern void PRINTFLIKE(4, 5) msg_rate_delay(time_t *, int,
	              void PRINTFPTRLIKE(1, 2) (*log_fn) (const char *,...),
					            const char *,...);

 /*
  * Only for tests: make a long jump instead of terminating.
  */
#ifdef NO_SIGSETJMP
#define MSG_JMP_BUF jmp_buf
#define msg_setjmp(bufp)	setjmp((msg_jmp_bufp = (bufp))[0])
#define msg_longjmp(val)	longjmp(msg_jmp_bufp[0], (val))
#else
#define MSG_JMP_BUF sigjmp_buf
#define msg_setjmp(bufp)	sigsetjmp((msg_jmp_bufp = (bufp))[0], 1)
#define msg_longjmp(val)	siglongjmp(msg_jmp_bufp[0], (val))
#endif
#define msg_resetjmp(bufp)	do { msg_jmp_bufp = (bufp); } while (0)
#define msg_clearjmp()		do { msg_jmp_bufp = 0; } while (0)

extern MSG_JMP_BUF *msg_jmp_bufp;

#define MSG_LONGJMP_FATAL	2	/* msg_fatal longjmp code */
#define MSG_LONGJMP_PANIC	3	/* msg_panic longjmp code */

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
/*	Wietse Venema
/*	Google, Inc.
/*	111 8th Avenue
/*	New York, NY 10011, USA
/*--*/

#endif
