/*++
/* NAME
/*	mock_fcntl 3
/* SUMMARY
/*	mock fcntl/flock
/* SYNOPSIS
/*	#include <mock_fcntl.h>
/*
/*	typedef struct MOCK_FCNTL_REQ {
/* .in +4
/*	int	want_fd;
/*	int	want_cmd;
/*	union {
/* .in +4
/*	    struct flock fl;
/*	    int flags;
/* .in -4
/*	} want_arg;
/*	int	out_ret;
/*	int	out_errno;
/* .in -4
/*	} MOCK_FCNTL_REQ;
/*
/*	void	setup_mock_fcntl(const MOCK_FCNTL_REQ *request)
/*
/*	void	teardown_mock_fcntl(void)
/*
/*	typedef struct MOCK_FLOCK_REQ {
/* .in +4
/*	int	want_fd;
/*	int	want_cmd;
/* .in -4
/*	int	out_ret;
/*	int	out_errno;
/*	} MOCK_FLOCK_REQ;
/*
/*	void	setup_mock_flock(const MOCK_FLOCK_REQ *request)
/*
/*	void	teardown_mock_flock(void)
/* DESCRIPTION
/*	This module provides mock fcntl() and flock() implementations
/*	that return static results for expected calls.
/*
/*	setup_mock_fcntl() adds a mapping from fcntl() requests to
/*	fcntl() results. It is an error to enter multiple mappings for
/*	the same request. setup_mock_fcntl() makes no copy of its input:
/* .IP want_fd
/* .IP want_cmd
/* .IP want_arg
/*	These form the lookup key for the setup_mock_fcntl() mapping.
/*	The caller must zero-fill unused space in and around the want_xxx
/*	request members.
/* .IP out_ret
/*	The mock fcntl() result value. If -1, then out_errno must be
/*	non-zero.
/* .IP out_errno
/*	The global out_errno upon return from mock fcntl(). If this is
/*	non-zero, mock fcntl() must return -1.
/* .PP
/*	teardown_mock_fcntl() destroys all mappings entered with
/*	setup_mock_fcntl().
/*
/*	The mock fcntl() function looks up the MOCK_FCNTL_REQ information
/*	for an fcntl() request, and terminates the test if the request
/*	is not expected.
/*
/*	setup_mock_flock() and teardown_mock_flock() implement similar
/*	functionality for flock() calls.
/* SEE ALSO
/*	fcntl(2), flock(2), the real things
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>

 /*
  * Utility library.
  */
#include <binhash.h>
#include <wrap_fcntl.h>
#include <msg.h>
#include <string.h>

 /*
  * Test library.
  */
#include <ptest.h>
#include <mock_fcntl.h>

 /*
  * Application-specific.
  */
static BINHASH *mock_fcntl_table;
static BINHASH *mock_flock_table;

#ifdef HAS_FCNTL_LOCK

/* setup_mock_fcntl - add request-to-result mapping */

void    setup_mock_fcntl(const MOCK_FCNTL_REQ *request)
{
    if (mock_fcntl_table == 0)
	mock_fcntl_table = binhash_create(10);
    if (request->out_ret == -1 && request->out_errno == 0)
	ptest_fatal(ptest_ctx_current(),
		    "setup_mock_fcntl: result -1 requires non-zero errno");
    if (request->out_ret != -1 && request->out_errno != 0)
	ptest_fatal(ptest_ctx_current(),
		    "setup_mock_fcntl: result %d requires zero errno",
		    request->out_ret);
    binhash_enter(mock_fcntl_table, (void *) request,
		  MOCK_FCNTL_REQ_WANT_SIZE, (void *) request);
}

/* teardown_mock_fcntl - destroy all request-to-result mappings */

void    teardown_mock_fcntl(void)
{
    if (mock_fcntl_table) {
	binhash_free(mock_fcntl_table, (void (*) (void *)) 0);
	mock_fcntl_table = 0;
    }
}

/* fcntl - mock fcntl() implementation */

int     fcntl(int fd, int cmd,...)
{
    MOCK_FCNTL_REQ mock_key;
    MOCK_FCNTL_REQ *mock_info;
    va_list ap;

    if (mock_fcntl_table != 0) {
	va_start(ap, cmd);
	memset((void *) &mock_key, 0, MOCK_FCNTL_REQ_WANT_SIZE);
	mock_key.want_fd = fd;
	mock_key.want_cmd = cmd;
	switch (cmd) {
	case F_GETFD:
	case F_GETFL:
	    break;
	case F_DUPFD:
	case F_SETFD:
	case F_SETFL:
	    mock_key.want_arg.fd = va_arg(ap, int);
	    break;
	case F_SETLK:
	case F_SETLKW:
	case F_GETLK:
	    mock_key.want_arg.fl = *va_arg(ap, struct flock *);
	    break;
	}
	va_end(ap);
    }
    if (mock_fcntl_table == 0
	|| (mock_info = (MOCK_FCNTL_REQ *)
	    binhash_find(mock_fcntl_table, &mock_key,
			 MOCK_FCNTL_REQ_WANT_SIZE)) == 0)
	ptest_fatal(ptest_ctx_current(),
		    "unexpected request: fcntl(%d, %d, ...)", fd, cmd);
    errno = mock_info->out_errno;
    return (mock_info->out_ret);
}

#endif					/* HAS_FCNTL_LOCK */

#ifdef  HAS_FLOCK_LOCK

/* setup_mock_flock - add request-to-result mapping */

void    setup_mock_flock(const MOCK_FLOCK_REQ *request)
{
    if (mock_flock_table == 0)
	mock_flock_table = binhash_create(10);
    if (request->out_ret == -1 && request->out_errno == 0)
	ptest_fatal(ptest_ctx_current(),
		    "setup_mock_flock: result -1 requires non-zero errno");
    if (request->out_ret != -1 && request->out_errno != 0)
	ptest_fatal(ptest_ctx_current(),
		    "setup_mock_flock: result %d requires zero errno",
		    request->out_ret);
    binhash_enter(mock_flock_table, (void *) request,
		  MOCK_FLOCK_REQ_WANT_SIZE, (void *) request);
}

/* teardown_mock_flock - destroy all request-to-result mappings */

void    teardown_mock_flock(void)
{
    if (mock_flock_table) {
	binhash_free(mock_flock_table, (void (*) (void *)) 0);
	mock_flock_table = 0;
    }
}

/* flock - mock flock() implementation */

int     flock(int fd, int cmd)
{
    MOCK_FLOCK_REQ mock_key;
    MOCK_FLOCK_REQ *mock_info;

    mock_key.want_fd = fd;
    mock_key.want_cmd = cmd;
    if (mock_flock_table == 0
	|| (mock_info = (MOCK_FLOCK_REQ *)
	    binhash_find(mock_flock_table, &mock_key,
			 MOCK_FLOCK_REQ_WANT_SIZE)) == 0)
	ptest_fatal(ptest_ctx_current(),
		    "unexpected request: flock(%d, %d)", fd, cmd);
    errno = mock_info->out_errno;
    return (mock_info->out_ret);
}

#endif					/* HAS_FLOCK_LOCK */
