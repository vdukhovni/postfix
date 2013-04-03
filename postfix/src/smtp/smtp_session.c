/*++
/* NAME
/*	smtp_session 3
/* SUMMARY
/*	SMTP_SESSION structure management
/* SYNOPSIS
/*	#include "smtp.h"
/*
/*	SMTP_SESSION *smtp_session_alloc(why, dest, host, addr,
/*					port, flags)
/*	DSN_BUF *why;
/*	char	*dest;
/*	char	*host;
/*	char	*addr;
/*	unsigned port;
/*	int	flags;
/*
/*	void	smtp_session_new_stream(session, stream, start, flags)
/*	SMTP_SESSION *session;
/*	VSTREAM	*stream;
/*	time_t	start;
/*	int	flags;
/*
/*	int	smtp_sess_tls_check(dest, host, port, valid)
/*	char	*dest;
/*	char	*host;
/*	unsigned port;
/*	int	valid;
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
/*	and initializes it with the given destination, host name and address
/*	information.  The host name and address strings are copied. The port
/*	is in network byte order.  When TLS is enabled, smtp_session_alloc()
/*	looks up the per-site TLS policies for TLS enforcement and certificate
/*	verification.  The resulting policy is stored into the SMTP_SESSION
/*	object.  Table and DNS lookups can fail during TLS policy creation,
/*	when this happens, "why" is updated with the error reason and a null
/*	session pointer is returned.
/*
/*	smtp_session_new_stream() updates an SMTP_SESSION structure
/*	with a newly-created stream that was created at the specified
/*	start time, and makes it subject to the specified connection
/*	caching policy.
/*
/*	smtp_sess_tls_check() returns true if TLS use is mandatory, invalid
/*	or indeterminate. The return value is false only if TLS is optional.
/*	This is not yet used anywhere, it can be used to safely enable TLS
/*	policy with smtp_reuse_addr().
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
/* .IP valid
/*	The DNSSEC validation status of the host name.
/* .IP start
/*	The time when this connection was opened.
/* .IP flags
/*	Zero or more of the following:
/* .RS
/* .IP SMTP_MISC_FLAG_TLSA_HOST
/*	The hostname is DNSSEC-validated.
/* .IP SMTP_MISC_FLAG_CONN_LOAD
/*	Enable re-use of cached SMTP or LMTP connections.
/* .IP SMTP_MISC_FLAG_CONN_STORE
/*	Enable saving of cached SMTP or LMTP connections.
/* .IP SMTP_MISC_FLAG_NO_TLS
/*	Used only internally in smtp_session.c
/* .RE
/*	SMTP_MISC_FLAG_CONN_MASK corresponds with both _LOAD and _STORE.
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
/*
/*	TLS support originally by:
/*	Lutz Jaenicke
/*	BTU Cottbus
/*	Allgemeine Elektrotechnik
/*	Universitaetsplatz 3-4
/*	D-03044 Cottbus, Germany
/*--*/

/* System library. */

#include <sys_defs.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <vstring.h>
#include <vstream.h>
#include <stringops.h>
#include <valid_hostname.h>

/* Global library. */

#include <mime_state.h>
#include <debug_peer.h>
#include <mail_params.h>
#include <maps.h>
#include <smtp_stream.h>

/* Application-specific. */

#include "smtp.h"
#include "smtp_sasl.h"

/* smtp_session_alloc - allocate and initialize SMTP_SESSION structure */

SMTP_SESSION *smtp_session_alloc(DSN_BUF *why, const char *dest,
				         const char *host, const char *addr,
				         unsigned port, int flags)
{
    const char *myname = "smtp_session_alloc";
    SMTP_SESSION *session;
    int     valid = (flags & SMTP_MISC_FLAG_TLSA_HOST);

    session = (SMTP_SESSION *) mymalloc(sizeof(*session));
    session->stream = 0;
    session->dest = mystrdup(dest);
    session->host = mystrdup(host);
    session->addr = mystrdup(addr);
    session->namaddr = concatenate(host, "[", addr, "]", (char *) 0);
    session->helo = 0;
    session->port = port;
    session->features = 0;

    session->size_limit = 0;
    session->error_mask = 0;
    session->buffer = vstring_alloc(100);
    session->scratch = vstring_alloc(100);
    session->scratch2 = vstring_alloc(100);
    smtp_chat_init(session);
    session->mime_state = 0;

    if (session->port) {
	vstring_sprintf(session->buffer, "%s:%d",
			session->namaddr, ntohs(session->port));
	session->namaddrport = mystrdup(STR(session->buffer));
    } else
	session->namaddrport = mystrdup(session->namaddr);

    session->send_proto_helo = 0;

    USE_NEWBORN_SESSION;			/* He's not dead Jim! */

#ifdef USE_SASL_AUTH
    smtp_sasl_connect(session);
#endif

    /*
     * When the destination argument of smtp_tls_sess_alloc() is null, a
     * trivial TLS policy (level = "none") is returned unconditionally and
     * the other arguments are not used.  Soon the DSN_BUF "why" argument
     * will be optional when the caller is not interested in the error
     * status.
     */
#define NO_DSN_BUF	(DSN_BUF *) 0
#define NO_DEST		(char *) 0
#define NO_HOST		(char *) 0
#define NO_PORT		0
#define NO_FLAGS	0

#ifdef USE_TLS
    session->tls_context = 0;
    session->tls_retry_plain = 0;
    session->tls_nexthop = 0;
    if (flags & SMTP_MISC_FLAG_NO_TLS)
	session->tls = smtp_tls_sess_alloc(NO_DSN_BUF, NO_DEST, NO_HOST,
					   NO_PORT, NO_FLAGS);
    else {
	if (why == 0)
	    msg_panic("%s: null error buffer", myname);
	session->tls = smtp_tls_sess_alloc(why, dest, host, port, valid);
    }
    if (!session->tls) {
	smtp_session_free(session);
	return (0);
    }
#endif
    session->state = 0;
    debug_peer_check(host, addr);
    return (session);
}

/* smtp_session_new_stream - finalize session with newly-created connection */

void    smtp_session_new_stream(SMTP_SESSION *session, VSTREAM *stream,
				        time_t start, int flags)
{
    const char *myname = "smtp_session_new_stream";

    /*
     * Sanity check.
     */
    if (session->stream != 0)
	msg_panic("%s: session exists", myname);

    session->stream = stream;

    /*
     * Make the session subject to the new-stream connection caching policy,
     * as opposed to the reused-stream connection caching policy at the
     * bottom of this module. Both policies are enforced in this file, not
     * one policy here and the other at some random place in smtp_connect.c.
     */
    if (flags & SMTP_MISC_FLAG_CONN_STORE)
	CACHE_THIS_SESSION_UNTIL(start + var_smtp_reuse_time);
    else
	DONT_CACHE_THIS_SESSION;
    session->reuse_count = 0;
}

/* smtp_session_free - destroy SMTP_SESSION structure and contents */

void    smtp_session_free(SMTP_SESSION *session)
{
#ifdef USE_TLS
    if (session->stream) {
	vstream_fflush(session->stream);
	if (session->tls_context)
	    tls_client_stop(smtp_tls_ctx, session->stream,
			  var_smtp_starttls_tmout, 0, session->tls_context);
    }
    if (session->tls)
	(void) smtp_tls_sess_free(session->tls);
#endif
    if (session->stream)
	vstream_fclose(session->stream);
    myfree(session->dest);
    myfree(session->host);
    myfree(session->addr);
    myfree(session->namaddr);
    myfree(session->namaddrport);
    if (session->helo)
	myfree(session->helo);

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

/* smtp_sess_tls_check - does session require tls */

int     smtp_sess_tls_check(const char *dest, const char *host, unsigned port,
			            int valid)
{
#ifdef USE_TLS
    static DSN_BUF *why;
    SMTP_TLS_SESS *tls;

    if (!why)
	why = dsb_create();

    tls = smtp_tls_sess_alloc(why, dest, host, ntohs(port), valid);
    dsb_reset(why);

    if (tls && tls->level >= TLS_LEV_NONE && tls->level <= TLS_LEV_MAY)
	return (0);
    if (tls)
	smtp_tls_sess_free(tls);
    return (1);
#else
    return (0);
#endif
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
     * 
     * XXX Be sure to use unsigned types in the format string. Sign characters
     * would be rejected by the alldig() test on the reading end.
     */
    vstring_sprintf(endp_prop, "%u\n%s\n%s\n%s\n%u\n%u\n%lu",
		    session->reuse_count,
		    session->dest, session->host,
		    session->addr, session->port,
		    session->features & SMTP_FEATURE_ENDPOINT_MASK,
		    (long) session->expire_time);

    /*
     * Append the passivated SASL attributes.
     */
#ifdef notdef
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
    time_t  expire_time;		/* session re-use expiration time */
    unsigned reuse_count;		/* # times reused */

    /*
     * XXX it would be nice to have a VSTRING to VSTREAM adapter so that we
     * can de-serialize the properties with attr_scan(), instead of using
     * ad-hoc, non-reusable code.
     * 
     * XXX As a preliminary solution we use mystrtok(), but that function is not
     * suitable for zero-length fields.
     */
    endp_props = STR(endp_prop);
    if ((prop = mystrtok(&endp_props, "\n")) == 0 || !alldig(prop)) {
	msg_warn("%s: bad cached session reuse count property", myname);
	return (0);
    }
    reuse_count = atoi(prop);
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
	msg_warn("%s: bad cached session expiration time property", myname);
	return (0);
    }
#ifdef MISSING_STRTOUL
    expire_time = strtol(prop, 0, 10);
#else
    expire_time = strtoul(prop, 0, 10);
#endif

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
     * 
     * Caller is responsible for not reusing plain-text connections when TLS is
     * required.  With TLS disabled, a trivial TLS policy (level "none") is
     * returned without fail, with no other ways for session setup to fail,
     * we assume it must succeed.
     */
    session = smtp_session_alloc(NO_DSN_BUF, dest, host, addr, port,
				 SMTP_MISC_FLAG_NO_TLS);
    session->stream = vstream_fdopen(fd, O_RDWR);
    session->features = (features | SMTP_FEATURE_FROM_CACHE);
    CACHE_THIS_SESSION_UNTIL(expire_time);
    session->reuse_count = ++reuse_count;

    if (msg_verbose)
	msg_info("%s: dest=%s host=%s addr=%s port=%u features=0x%x, "
		 "ttl=%ld, reuse=%d",
		 myname, dest, host, addr, ntohs(port), features,
		 (long) (expire_time - time((time_t *) 0)), reuse_count);

    /*
     * Re-activate the SASL attributes.
     */
#ifdef notdef
    if (smtp_sasl_enable && smtp_sasl_activate(session, endp_props) < 0) {
	vstream_fdclose(session->stream);
	session->stream = 0;
	smtp_session_free(session);
	return (0);
    }
#endif

    return (session);
}
