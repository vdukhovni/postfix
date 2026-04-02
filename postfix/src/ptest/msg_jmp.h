#ifndef _MSG_JMP_H_INCLUDED_
#define _MSG_JMP_H_INCLUDED_

/*++
/* NAME
/*	msg_jmp 3h
/* SUMMARY
/*	msg plugin for tests
/* SYNOPSIS
/*	#include <msg_jmp.h>
/* DESCRIPTION
/*	.nf

/*
 * System library.
 */
#include <setjmp.h>

/*
 * Utility library.
 */
#include <msg.h>

/*
 * External interface.
 */

 /*
  * Only for tests: make a long jump instead of terminating.
  */
#ifdef NO_SIGSETJMP
#define MSG_JMP_BUF jmp_buf
#define msg_setjmp(bufp)        (msg_set_longjmp_action(msg_longjmp), \
				    setjmp((msg_jmp_bufp = (bufp))[0]))
#else
#define MSG_JMP_BUF sigjmp_buf
#define msg_setjmp(bufp)        (msg_set_longjmp_action(msg_longjmp), \
				    sigsetjmp((msg_jmp_bufp = (bufp))[0], 1))
#endif
#define msg_resetjmp(bufp)      do { msg_jmp_bufp = (bufp); } while (0)
#define msg_clearjmp()          do { \
				    msg_set_longjmp_action(0); \
				    msg_jmp_bufp = 0; \
				} while (0)

extern MSG_JMP_BUF *msg_jmp_bufp;
extern NORETURN msg_longjmp(int);

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
