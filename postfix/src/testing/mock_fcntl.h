#ifndef _MOCK_FCNTL_H_INCLUDED_
#define _MOCK_FCNTL_H_INCLUDED_

/*++
/* NAME
/*	mock_fcntl 3h
/* SUMMARY
/*	mock fcntl/flock
/* SYNOPSIS
/*	#include <mock_fcntl.h>
/* DESCRIPTION
/* .nf

 /*
  * System library.
  */
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>			/* offsetof() */

 /*
  * External interface.
  */
typedef struct MOCK_FCNTL_REQ {
    /* Inputs. */
    int     want_fd;
    int     want_cmd;
    union {
	struct flock fl;
	int     fd;
	int     flags;
    }       want_arg;
    /* Outputs. */
    int     out_ret;
    int     out_errno;
} MOCK_FCNTL_REQ;

#define MOCK_FCNTL_REQ_WANT_SIZE offsetof(MOCK_FCNTL_REQ, out_ret)

extern void setup_mock_fcntl(const MOCK_FCNTL_REQ *);
extern void teardown_mock_fcntl(void);

typedef struct MOCK_FLOCK_REQ {
    /* Inputs. */
    int     want_fd;
    int     want_cmd;
    /* Outputs. */
    int     out_ret;
    int     out_errno;
} MOCK_FLOCK_REQ;

#define MOCK_FLOCK_REQ_WANT_SIZE offsetof(MOCK_FLOCK_REQ, out_ret)

extern void setup_mock_flock(const MOCK_FLOCK_REQ *);
extern void teardown_mock_flock(void);

/* LICENSE
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

#endif
