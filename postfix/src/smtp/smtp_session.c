/*++
/* NAME
/*	smtp_session 3
/* SUMMARY
/*	SMTP_SESSION structure management
/* SYNOPSIS
/*	#include "smtp.h"
/*
/*	SMTP_SESSION *smtp_session_alloc(stream, dest, host, addr, port, flags)
/*	VSTREAM *stream;
/*	char	*dest;
/*	char	*host;
/*	char	*addr;
/*	unsigned port;
/*	int	flags;
/*
/*	void	smtp_session_free(session)
/*	SMTP_SESSION *session;
/*
/*	int	smtp_session_passivate(session, dest_prop, endp_prop)
/*	SMTP_SESSION *session;
/*	VSTRING	*dest_prop;
/*	VSTRING	*endp_prop;
/*
/*	SMTP_SESSION *smtp_session_activate(fd, dest_prop, endp_prop)
/*	int	fd;
/*	VSTRING	*dest_prop;
/*	VSTRING	*endp_prop;
/* DESCRIPTION
/*	smtp_session_alloc() allocates memory for an SMTP_SESSION structure
/*	and initializes it with the given stream and destination, host name
/*	and address information.  The host name and address strings are
/*	copied. The port is in network byte order.
/*
/*	smtp_session_free() destroys an SMTP_SESSION structure and its
/*	members, making memory available for reuse. It will handle the
/*	case of a null stream and will assume it was given a different
/*	purpose.
/*
/*	smtp_session_passivate() flattens an SMTP session so that
/*	it can be cached. The SMTP_SESSION structure is destroyed.
/*
/*	smtp_session_activate() inflates a flattened SMTP session
/*	so that it can be used. The input is modified.
/*
/*	Arguments:
/* .IP stream
/*	A full-duplex stream.
/* .IP dest
/*	The unmodified next-hop or fall-back destination including
/*	the optional [] and including the optional port or service.
/* .IP host
/*	The name of the host that we are connected to.
/* .IP addr
/*	The address of the host that we are connected to.
/* .IP port
/*	The remote port, network byte order.
/* .IP flags
/*	Zero or more of the following:
/* .RS
/* .IP SMTP_SESS_FLAG_CACHE
/*	Enable session caching.
/* .RE
/* .IP
/*	The manifest constant SMTP_SESS_FLAG_NONE requests no options.
/* .IP dest_prop
/*	Destination specific session properties: the server is the
/*	best MX host for the current logical destination.
/* .IP endp_prop
/*	Endpoint specific session properties: all the features
/*	advertised by the remote server.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include <sys_defs.h>
#include <stdlib.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <vstream.h>
#include <stringops.h>

/* Global library. */

#include <mime_state.h>
#include <debug_peer.h>
#include <mail_params.h>

/* Application-specific. */

#include "smtp.h"

#define STR(x) vstring_str(x)

/*#define msg_verbose 1*/

/* smtp_session_alloc - allocate and initialize SMTP_SESSION structure */

SMTP_SESSION *smtp_session_alloc(VSTREAM *stream, const char *dest,
				         const char *host, const char *addr,
				         unsigned port, int flags)
{
    SMTP_SESSION *session;

    session = (SMTP_SESSION *) mymalloc(sizeof(*session));
    session->stream = stream;
    session->dest = mystrdup(dest);
    session->host = mystrdup(host);
    session->addr = mystrdup(addr);
    session->namaddr = concatenate(host, "[", addr, "]", (char *) 0);
    session->port = port;
    session->features = 0;

    session->size_limit = 0;
    session->error_mask = 0;
    session->buffer = vstring_alloc(100);
    session->scratch = vstring_alloc(100);
    session->scratch2 = vstring_alloc(100);
    smtp_chat_init(session);
    session->features = 0;
    session->mime_state = 0;

    session->sndbufsize = 0;
    session->send_proto_helo = 0;

    if (flags & SMTP_SESS_FLAG_CACHE)
	session->reuse_count = var_smtp_reuse_limit;
    else
	session->reuse_count = 0;

#ifdef USE_SASL_AUTH
    smtp_sasl_connect(session);
#endif

    debug_peer_check(host, addr);
    return (session);
}

/* smtp_session_free - destroy SMTP_SESSION structure and contents */

void    smtp_session_free(SMTP_SESSION *session)
{
    if (session->stream)
	vstream_fclose(session->stream);
    myfree(session->dest);
    myfree(session->host);
    myfree(session->addr);
    myfree(session->namaddr);

    vstring_free(session->buffer);
    vstring_free(session->scratch);
    vstring_free(session->scratch2);

    if (session->history)
	smtp_chat_reset(session);
    if (session->mime_state)
	mime_state_free(session->mime_state);

#ifdef USE_SASL_AUTH
    smtp_sasl_cleanup(session);
#endif

    debug_peer_restore();
    myfree((char *) session);
}

/* smtp_session_passivate - passivate an SMTP_SESSION object */

int     smtp_session_passivate(SMTP_SESSION *session, VSTRING *dest_prop,
			               VSTRING *endp_prop)
{
    int     fd;

    /*
     * Encode the local-to-physical binding properties: whether or not this
     * server is best MX host for the next-hop or fall-back logical
     * destination (this information is needed for loop handling in
     * smtp_proto()).
     * 
     * XXX It would be nice to have a VSTRING to VSTREAM adapter so that we can
     * serialize the properties with attr_print() instead of using ad-hoc,
     * non-reusable, code and hard-coded format strings.
     */
    vstring_sprintf(dest_prop, "%u",
		    session->features & SMTP_FEATURE_DESTINATION_MASK);

    /*
     * Encode the physical endpoint properties: all the session properties
     * except for "session from cache", "best MX", or "RSET failure".
     * 
     * XXX It would be nice to have a VSTRING to VSTREAM adapter so that we can
     * serialize the properties with attr_print() instead of using obscure
     * hard-coded format strings.
     * 
     * XXX Should also record an absolute time when a session must be closed,
     * how many non-delivering mail transactions there were during this
     * session, and perhaps other statistics, so that we don't reuse a
     * session too much.
     */
    vstring_sprintf(endp_prop, "%s\n%s\n%s\n%u\n%u\n%u\n%u",
		    session->dest, session->host,
		    session->addr, session->port,
		    session->features & SMTP_FEATURE_ENDPOINT_MASK,
		    session->reuse_count,
		    session->sndbufsize);

    /*
     * Append the passivated SASL attributes.
     */
#ifdef USE_SASL
    if (smtp_sasl_enable)
	smtp_sasl_passivate(endp_prop, session);
#endif

    /*
     * Salvage the underlying file descriptor, and destroy the session
     * object.
     */
    fd = vstream_fileno(session->stream);
    vstream_fdclose(session->stream);
    session->stream = 0;
    smtp_session_free(session);

    return (fd);
}

/* smtp_session_activate - re-activate a passivated SMTP_SESSION object */

SMTP_SESSION *smtp_session_activate(int fd, VSTRING *dest_prop,
				            VSTRING *endp_prop)
{
    const char *myname = "smtp_session_activate";
    SMTP_SESSION *session;
    char   *dest_props;
    char   *endp_props;
    const char *prop;
    const char *dest;
    const char *host;
    const char *addr;
    unsigned port;
    unsigned features;			/* server features */
    unsigned reuse_count;		/* how reuses left */
    unsigned sndbufsize;		/* PIPELINING buffer size */

    /*
     * XXX it would be nice to have a VSTRING to VSTREAM adapter so that we
     * can de-serialize the properties with attr_scan(), instead of using
     * ad-hoc, non-reusable code.
     * 
     * XXX As a preliminary solution we use mystrtok(), but that function is not
     * suitable for zero-length fields.
     */
    endp_props = STR(endp_prop);
    if ((dest = mystrtok(&endp_props, "\n")) == 0) {
	msg_warn("%s: missing cached session destination property", myname);
	return (0);
    }
    if ((host = mystrtok(&endp_props, "\n")) == 0) {
	msg_warn("%s: missing cached session hostname property", myname);
	return (0);
    }
    if ((addr = mystrtok(&endp_props, "\n")) == 0) {
	msg_warn("%s: missing cached session address property", myname);
	return (0);
    }
    if ((prop = mystrtok(&endp_props, "\n")) == 0 || !alldig(prop)) {
	msg_warn("%s: bad cached session port property", myname);
	return (0);
    }
    port = atoi(prop);

    if ((prop = mystrtok(&endp_props, "\n")) == 0 || !alldig(prop)) {
	msg_warn("%s: bad cached session features property", myname);
	return (0);
    }
    features = atoi(prop);

    if ((prop = mystrtok(&endp_props, "\n")) == 0 || !alldig(prop)) {
	msg_warn("%s: bad cached session reuse_count property", myname);
	return (0);
    }
    reuse_count = atoi(prop);

    if ((prop = mystrtok(&endp_props, "\n")) == 0 || !alldig(prop)) {
	msg_warn("%s: bad cached session sndbufsize property", myname);
	return (0);
    }
    sndbufsize = atoi(prop);

    if (dest_prop && VSTRING_LEN(dest_prop)) {
	dest_props = STR(dest_prop);
	if ((prop = mystrtok(&dest_props, "\n")) == 0 || !alldig(prop)) {
	    msg_warn("%s: bad cached destination features property", myname);
	    return (0);
	}
	features |= atoi(prop);
    }

    /*
     * Allright, bundle up what we have sofar.
     */
    session = smtp_session_alloc(vstream_fdopen(fd, O_RDWR),
			       dest, host, addr, port, SMTP_SESS_FLAG_NONE);
    session->features = (features | SMTP_FEATURE_FROM_CACHE);
    session->reuse_count = reuse_count - 1;
    session->sndbufsize = sndbufsize;

    if (msg_verbose)
	msg_info("%s: dest=%s host=%s addr=%s port=%u features=0x%x, reuse=%u, sndbuf=%u",
		 myname, dest, host, addr, ntohs(port), features, reuse_count, sndbufsize);

    /*
     * Re-activate the SASL attributes.
     */
#ifdef USE_SASL
    if (smtp_sasl_enable && smtp_sasl_activate(session, endp_props) < 0) {
	vstream_fdclose(session->stream);
	session->stream = 0;
	smtp_session_free(session);
	return (0);
    }
#endif

    return (session);
}
