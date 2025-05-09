/*++
/* NAME
/*	postscreen_endpt 3
/* SUMMARY
/*	look up connection endpoint information
/* SYNOPSIS
/*	#include <postscreen.h>
/*
/*	void	psc_endpt_lookup(smtp_client_stream, lookup_done)
/*	VSTREAM	*smtp_client_stream;
/*	void	(*lookup_done)(status, smtp_client_stream,
/*				smtp_client_addr, smtp_client_port,
/*				smtp_server_addr, smtp_server_port)
/*	int	status;
/*	MAI_HOSTADDR_STR *smtp_client_addr;
/*	MAI_SERVPORT_STR *smtp_client_port;
/*	MAI_HOSTADDR_STR *smtp_server_addr;
/*	MAI_SERVPORT_STR *smtp_server_port;
/* AUXILIARY METHODS
/*	void	psc_endpt_local_lookup(smtp_client_stream, lookup_done)
/*	VSTREAM	*smtp_client_stream;
/*	void	(*lookup_done)(status, smtp_client_stream,
/*				smtp_client_addr, smtp_client_port,
/*				smtp_server_addr, smtp_server_port)
/*	int	status;
/*	MAI_HOSTADDR_STR *smtp_client_addr;
/*	MAI_SERVPORT_STR *smtp_client_port;
/*	MAI_HOSTADDR_STR *smtp_server_addr;
/*	MAI_SERVPORT_STR *smtp_server_port;
/* DESCRIPTION
/*	psc_endpt_lookup() looks up remote and local connection
/*	endpoint information, either through local system calls,
/*	or through an adapter for an up-stream proxy protocol.
/*
/*	The following summarizes what the postscreen(8) server
/*	expects from a proxy protocol adapter routine.
/* .IP \(bu
/*	Accept the same arguments as psc_endpt_lookup().
/* .IP \(bu
/*	Call psc_endpt_local_lookup() to look up connection info
/*	when the upstream proxy indicates that the connection is
/*	not proxied (e.g., health check probe).
/* .IP \(bu
/*	Validate protocol, address and port syntax. Permit only
/*	protocols that are configured with the main.cf:inet_protocols
/*	setting.
/* .IP \(bu
/*	Convert IPv4-in-IPv6 address syntax to IPv4 syntax when
/*	both IPv6 and IPv4 support are enabled with main.cf:inet_protocols.
/* .IP \(bu
/*	Log a clear warning message that explains why a request
/*	fails.
/* .IP \(bu
/*	Never talk to the remote SMTP client.
/* .PP
/*	Arguments:
/* .IP client_stream
/*	A brand-new stream that is connected to the remote client.
/*	This argument MUST be passed to psc_endpt_local_lookup()
/*	if the up-stream proxy indicates that a connection is not
/*	proxied.
/* .IP lookup
/*	Call-back routine that reports the result status, address
/*	and port information. The result status is -1 in case of
/*	error, 0 in case of success. This MUST NOT be called directly
/*	if the up-stream proxy indicates that a connection is not
/*	proxied; instead this MUST be called indirectly by
/*	psc_endpt_local_lookup().
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

/* System library. */

#include <sys_defs.h>
#include <string.h>

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

/* Utility library. */

#include <msg.h>
#include <myaddrinfo.h>
#include <vstream.h>
#include <inet_proto.h>

/* Global library. */

#include <mail_params.h>
#include <haproxy_srvr.h>

/* Application-specific. */

#include <postscreen.h>
#include <postscreen_haproxy.h>

static const INET_PROTO_INFO *proto_info;

/* psc_endpt_local_lookup - look up local system connection information */

void    psc_endpt_local_lookup(VSTREAM *smtp_client_stream,
			               PSC_ENDPT_LOOKUP_FN lookup_done)
{
    struct sockaddr_storage addr_storage;
    SOCKADDR_SIZE addr_storage_len;
    int     status;
    MAI_HOSTADDR_STR smtp_client_addr;
    MAI_SERVPORT_STR smtp_client_port;
    MAI_HOSTADDR_STR smtp_server_addr;
    MAI_SERVPORT_STR smtp_server_port;
    int     aierr;

#define RESET_ADDR_STORAGE_LEN() (addr_storage_len = sizeof(addr_storage))

    /*
     * Look up the remote SMTP client address and port.
     */
    if (RESET_ADDR_STORAGE_LEN(),
	getpeername(vstream_fileno(smtp_client_stream), (struct sockaddr *)
		    &addr_storage, &addr_storage_len) < 0) {
	msg_warn("getpeername: %m -- dropping this connection");
	status = -1;
    }

    /*
     * Convert the remote SMTP client address and port to printable form for
     * logging and access control. Note: this may change addr_storage and
     * addr_storage_len.
     */
    else if ((aierr = sane_sockaddr_to_hostaddr(
					  (struct sockaddr *) &addr_storage,
				       &addr_storage_len, &smtp_client_addr,
				    &smtp_client_port, SOCK_STREAM)) != 0) {
	msg_warn("cannot convert client address/port to string: %s"
		 " -- dropping this connection",
		 MAI_STRERROR(aierr));
	status = -1;
    }

    /*
     * Look up the local SMTP server address and port. Be sure to reset the
     * addr_storage_len value.
     */
    else if (RESET_ADDR_STORAGE_LEN(),
	     getsockname(vstream_fileno(smtp_client_stream),
			 (struct sockaddr *) &addr_storage,
			 &addr_storage_len) < 0) {
	msg_warn("getsockname: %m -- dropping this connection");
	status = -1;
    }

    /*
     * Convert the local SMTP server address and port to printable form for
     * logging. This may also change addr_storage and addr_storage_len, but
     * those variables are dead.
     */
    else if ((aierr = sane_sockaddr_to_hostaddr(
					  (struct sockaddr *) &addr_storage,
				       &addr_storage_len, &smtp_server_addr,
				    &smtp_server_port, SOCK_STREAM)) != 0) {
	msg_warn("cannot convert server address/port to string: %s"
		 " -- dropping this connection",
		 MAI_STRERROR(aierr));
	status = -1;
    } else {
	status = 0;
    }
    lookup_done(status, smtp_client_stream,
		&smtp_client_addr, &smtp_client_port,
		&smtp_server_addr, &smtp_server_port);
}

 /*
  * Lookup table for available proxy protocols.
  */
typedef struct {
    const char *name;
    void    (*endpt_lookup) (VSTREAM *, PSC_ENDPT_LOOKUP_FN);
} PSC_ENDPT_LOOKUP_INFO;

static const PSC_ENDPT_LOOKUP_INFO psc_endpt_lookup_info[] = {
    NOPROXY_PROTO_NAME, psc_endpt_local_lookup,
    HAPROXY_PROTO_NAME, psc_endpt_haproxy_lookup,
    0,
};

/* psc_endpt_lookup - look up connection endpoint information */

void    psc_endpt_lookup(VSTREAM *smtp_client_stream,
			         PSC_ENDPT_LOOKUP_FN notify)
{
    const PSC_ENDPT_LOOKUP_INFO *pp;

    if (proto_info == 0)
	proto_info = inet_proto_info();

    for (pp = psc_endpt_lookup_info; /* see below */ ; pp++) {
	if (pp->name == 0)
	    msg_fatal("unsupported %s value: %s",
		      VAR_PSC_UPROXY_PROTO, var_psc_uproxy_proto);
	if (strcmp(var_psc_uproxy_proto, pp->name) == 0) {
	    pp->endpt_lookup(smtp_client_stream, notify);
	    return;
	}
    }
}
