#ifndef _TLSPROXY_SERVER_H_
#define _TLSPROXY_SERVER_H_

/*++
/* NAME
/*	tlsproxy_server 3h
/* SUMMARY
/*	tlsproxy server role support
/* SYNOPSIS
/*	#include <tlsproxy_server.h>
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

extern bool pre_jail_init_server(void);
extern TLS_APPL_STATE *tlsp_server_init(TLS_SERVER_PARAMS *, TLS_SERVER_INIT_PROPS *);
extern int tlsp_server_start_pre_handshake(TLSP_STATE *);

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
