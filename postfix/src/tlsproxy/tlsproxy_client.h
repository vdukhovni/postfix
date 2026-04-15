#ifndef _TLSPROXY_CLIENT_H_
#define _TLSPROXY_CLIENT_H_

/*++
/* NAME
/*	tlsproxy_client 3h
/* SUMMARY
/*	tlsproxy client role support
/* SYNOPSIS
/*	#include <tlsproxy_client.h>
/* DESCRIPTION
/* .nf

 /*
  * TLS library.
  */
#include <tls.h>
#include <tls_proxy.h>

 /*
  * Internal API.
  */
#include <tlsproxy.h>

extern void tlsp_pre_jail_client_init(void);
extern TLS_APPL_STATE *tlsp_client_init(TLS_CLIENT_PARAMS *, TLS_CLIENT_INIT_PROPS *);
extern int tlsp_client_start_pre_handshake(TLSP_STATE *);

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
/*
/*	Wietse Venema
/*	porcupine.org
/*--*/

#endif
