/*++
/* NAME
/*	smtp_session 3
/* SUMMARY
/*	SMTP_SESSION structure management
/* SYNOPSIS
/*	#include "smtp.h"
/*
/*	SMTP_SESSION *smtp_session_alloc(stream, dest, host, addr)
/*	VSTREAM *stream;
/*	char	*dest;
/*	char	*host;
/*	char	*addr;
/*
/*	void	smtp_session_free(session)
/*	SMTP_SESSION *session;
/* DESCRIPTION
/*	smtp_session_alloc() allocates memory for an SMTP_SESSION structure
/*	and initializes it with the given stream and destination, host name
/*	and address information.  The host name and address strings are
/*	copied. The code assumes that the stream is connected to the "best"
/*	alternative.
/*
/*	smtp_session_free() destroys an SMTP_SESSION structure and its
/*	members, making memory available for reuse.
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

/* Utility library. */

#include <mymalloc.h>
#include <vstream.h>
#include <stringops.h>

/* Global library. */

#include <mime_state.h>

/* Application-specific. */

#include "smtp.h"

/* smtp_session_alloc - allocate and initialize SMTP_SESSION structure */

SMTP_SESSION *smtp_session_alloc(VSTREAM *stream, char *dest, char *host,
				         char *addr)
{
    SMTP_SESSION *session;

    session = (SMTP_SESSION *) mymalloc(sizeof(*session));
    session->stream = stream;
    session->dest = mystrdup(dest);
    session->host = mystrdup(host);
    session->addr = mystrdup(addr);
    session->namaddr = concatenate(host, "[", addr, "]", (char *) 0);
    session->best = 1;

    session->size_limit = 0;
    session->error_mask = 0;
    session->buffer = vstring_alloc(100);
    session->scratch = vstring_alloc(100);
    session->scratch2 = vstring_alloc(100);
    smtp_chat_init(session);
    session->features = SMTP_FEATURE_CACHE_SESSION;
    session->mime_state = 0;

#ifdef USE_SASL_AUTH
    smtp_sasl_connect(session);
#endif

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

    myfree((char *) session);
}
