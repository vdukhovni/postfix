/*++
/* NAME
/*	msg_jmp 3
/* SUMMARY
/*	msg plugin for tests
/* SYNOPSIS
/*	#include <msg_jmp.h>
/*
/*	int     msg_setjmp(MSG_JMP_BUF *bufp)
/*
/*	void    msg_resetjmp(MSG_JMP_BUF *bufp)
/*
/*	void    msg_clearjmp(void)
/* DESCRIPTION
/*	The default action for msg_fatal*() and msg_panic() is to terminate
/*	the program. To support non-production tests that must verify
/*	that these calls are actually made, the following functions
/*	implement support for long jumps instead of process termination.
/*	This code uses the msg_bailout_action() call-back feature.
/*
/*	msg_setjmp() specifies a caller-specified buffer and saves state
/*	for a future long jump. The buffer lifetime must extend to the
/*	next msg_resetjmp() or msg_clearjmp() call.
/*
/*	In-between the msg_setjmp() and msg_clearjmp() calls, msg_fatal*()
/*	and msg_panic() will perform a long jump instead of terminating
/*	the program.
/*
/*	msg_resetjmp() restores state that was previously saved with
/*	msg_setjmp(). The buffer lifetime must extend to the next
/*	msg_resetjmp() or msg_clearjmp() call.
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

 /*
  * System libraries.
  */
#include <sys_defs.h>
#include <signal.h>

 /*
  * Utility library.
  */
#include <msg.h>

 /*
  * Ptest library.
  */
#include <msg_jmp.h>

 /*
  * Private state. But it has to be caller-visible
because msg_setjmp() cannot be a function.
  */
MSG_JMP_BUF *msg_jmp_bufp;

/* msg_longjmp - restore setjmp() previously-saved run-time state */

NORETURN msg_longjmp(int value)
{
#ifdef NO_SIGSETJMP
    longjmp(msg_jmp_bufp[0], value);
#else
    siglongjmp(msg_jmp_bufp[0], value);
#endif
}
