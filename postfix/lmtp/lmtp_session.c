/*++
/* NAME
/*	lmtp_session 3
/* SUMMARY
/*	LMTP_SESSION structure management
/* SYNOPSIS
/*	#include "lmtp.h"
/*
/*	LMTP_SESSION *lmtp_session_alloc(stream, host, addr)
/*	VSTREAM *stream;
/*	char	*host;
/*	char	*addr;
/*
/*	void	lmtp_session_free(session)
/*	LMTP_SESSION *session;
/*
/*	void	lmtp_session_reset(state)
/*	LMTP_STATE *state;
/* DESCRIPTION
/*	lmtp_session_alloc() allocates memory for an LMTP_SESSION structure
/*	and initializes it with the given stream and host name and address
/*	information.  The host name and address strings are copied. The code
/*	assumes that the stream is connected to the "best" alternative.
/*
/*	lmtp_session_free() destroys an LMTP_SESSION structure and its
/*	members, making memory available for reuse.
/*
/*      lmtp_session_reset() is just a little helper to make sure everything
/*      is set to zero after the session has been freed.  This means I don't
/*      have to keep repeating the same chunks of code for cached connections.
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
/*      Alterations for LMTP by:
/*      Philip A. Prindeville
/*      Mirapoint, Inc.
/*      USA.
/*
/*      Additional work on LMTP by:
/*      Amos Gouaux
/*      University of Texas at Dallas
/*      P.O. Box 830688, MC34
/*      Richardson, TX 75083, USA
/*--*/

/* System library. */

#include <sys_defs.h>

/* Utility library. */

#include <mymalloc.h>
#include <vstream.h>

/* Application-specific. */

#include "lmtp.h"

/* lmtp_session_alloc - allocate and initialize LMTP_SESSION structure */

LMTP_SESSION *lmtp_session_alloc(VSTREAM *stream, char *host, char *addr)
{
    LMTP_SESSION *session;

    session = (LMTP_SESSION *) mymalloc(sizeof(*session));
    session->stream = stream;
    session->host = mystrdup(host);
    session->addr = mystrdup(addr);
    session->destination = 0;
    return (session);
}

/* lmtp_session_free - destroy LMTP_SESSION structure and contents */

void    lmtp_session_free(LMTP_SESSION *session)
{
    if (vstream_ispipe(session->stream))
	vstream_pclose(session->stream);
    else
	vstream_fclose(session->stream);
    myfree(session->host);
    myfree(session->addr);
    if (session->destination)
	myfree(session->destination);
    myfree((char *) session);
}

/* lmtp_session_reset - clean things up so a new session can be created */

void    lmtp_session_reset(LMTP_STATE *state)
{
    if (state->session) {
        lmtp_session_free(state->session);
        state->session = 0;
    }
    state->reuse = 0;
}

