#ifndef _GET_UNIX_PEER_CREDS_H_INCLUDED_
#define _GET_UNIX_PEER_CREDS_H_INCLUDED_

/*++
/* NAME
/*	get_unix_peer_creds 3h
/* SUMMARY
/*	local IPC peer lookup
/* SYNOPSIS
/*	#include <get_unix_peer_creds.h>
/* DESCRIPTION
/* .nf

 /*
  * External interface.
  */
extern int get_unix_peer_creds(int sock, uid_t *uid, gid_t *gid);

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

#endif
