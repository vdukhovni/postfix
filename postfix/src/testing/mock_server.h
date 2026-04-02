#ifndef _MOCK_SERVER_H_INCLUDED_
#define _MOCK_SERVER_H_INCLUDED_

/*++
/* NAME
/*	mock_server 3h
/* SUMMARY
/*	Mock server support
/* SYNOPSIS
/*	#include <mock_server.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <vstring.h>
#include <connect.h>

 /*
  * External interface.
  */
typedef struct MOCK_SERVER {
    int     flags;
    int     fds[2];			/* pipe(2) result */
    char   *want_dest;
    VSTRING *want_req;			/* serialized request, may be null */
    VSTRING *resp;			/* serialized response, may be null */
    VSTRING *iobuf;			/* I/O buffer */
    struct MOCK_SERVER *next;	/* chain of unconnected servers */
    struct MOCK_SERVER *prev;	/* chain of unconnected servers */
} MOCK_SERVER;

#define MOCK_SERVER_FLAG_CONNECTED	(1<<0)

extern MOCK_SERVER *mock_unix_server_create(const char *);
extern void mock_server_interact(MOCK_SERVER *, const VSTRING *,
				              const VSTRING *);
extern void mock_server_free(MOCK_SERVER *);
extern void mock_server_free_void_ptr(void *);

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
