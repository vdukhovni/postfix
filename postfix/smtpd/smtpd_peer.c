/*++
/* NAME
/*	smtpd_peer 3
/* SUMMARY
/*	look up peer name/address information
/* SYNOPSIS
/*	#include "smtpd.h"
/*
/*	void	smtpd_peer_init(state)
/*	SMTPD_STATE *state;
/*
/*	void	smtpd_peer_reset(state)
/*	SMTPD_STATE *state;
/* DESCRIPTION
/*	The smtpd_peer_init() routine attempts to produce a printable
/*	version of the peer name and address of the specified socket.
/*	Where information is unavailable, the name and/or address
/*	are set to "unknown".
/*
/*	smtpd_peer_init() updates the following fields:
/* .IP name
/*	The client hostname. An unknown name is represented by the
/*	string "unknown".
/* .IP addr
/*	Printable representation of the client address.
/* .IP namaddr
/*	String of the form: "name[addr]".
/* .IP peer_code
/*	The peer_code result field specifies how the client name
/*	information should be interpreted:
/* .RS
/* .IP 2
/*	Both name lookup and name verification succeeded.
/* .IP 4
/*	The name lookup or name verification failed with a recoverable
/*	error (no address->name mapping or no name->address mapping).
/* .IP 5
/*	The name lookup or verification failed with an unrecoverable
/*	error (no address->name mapping, bad hostname syntax, no 
/*	name->address mapping, client address not listed for hostname).
/* .RE
/* .PP
/*	smtpd_peer_reset() releases memory allocate by smtpd_peer_init().
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>

#ifdef NO_HERRNO
static int h_errno = TRY_AGAIN;
define  hstrerror(x) "Host not found"
#endif

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <valid_hostname.h>
#include <stringops.h>

/* Global library. */


/* Application-specific. */

#include "smtpd.h"

/* smtpd_peer_init - initialize peer information */

void    smtpd_peer_init(SMTPD_STATE *state)
{
    struct sockaddr_in sin;
    SOCKADDR_SIZE len = sizeof(sin);
    struct hostent *hp;
    int     i;

    /*
     * If it's not networked assume local.
     */
    if (getpeername(vstream_fileno(state->client),
		    (struct sockaddr *) & sin, &len) < 0)
	sin.sin_family = AF_UNSPEC;

    switch (sin.sin_family) {

	/*
	 * If it's not Internet, assume the client is local, and avoid using
	 * the naming service because that can hang when the machine is
	 * disconnected.
	 */
    default:
	state->name = mystrdup("localhost");
	state->addr = mystrdup("127.0.0.1");	/* XXX bogus. */
	state->peer_code = 2;
	break;

	/*
	 * Look up and "verify" the client hostname.
	 */
    case AF_INET:
	state->addr = mystrdup(inet_ntoa(sin.sin_addr));
	hp = gethostbyaddr((char *) &(sin.sin_addr),
			   sizeof(sin.sin_addr), AF_INET);
	if (hp == 0) {
	    state->name = mystrdup("unknown");
	    state->peer_code = (h_errno == TRY_AGAIN ? 4 : 5);
	    break;
	}
	if (!valid_hostname(hp->h_name)) {
	    state->name = mystrdup("unknown");
	    state->peer_code = 5;
	    break;
	}
	state->name = mystrdup(hp->h_name);	/* hp->name is clobbered!! */
	state->peer_code = 2;

	/*
	 * Reject the hostname if it does not list the peer address.
	 */
	hp = gethostbyname(state->name);	/* clobbers hp->name!! */
	if (hp == 0) {
	    msg_warn("hostname %s verification failed: %s",
		     state->name, hstrerror(h_errno));
	    myfree(state->name);
	    state->name = mystrdup("unknown");
	    state->peer_code = (h_errno == TRY_AGAIN ? 4 : 5);
	    break;
	}
	if (hp->h_length != sizeof(sin.sin_addr)) {
	    msg_warn("hostname %s verification failed: bad address size %d",
		     state->name, hp->h_length);
	    myfree(state->name);
	    state->name = mystrdup("unknown");
	    state->peer_code = 5;
	    break;
	}
	for (i = 0; hp->h_addr_list[i]; i++) {
	    if (memcmp(hp->h_addr_list[i],
		       (char *) &sin.sin_addr,
		       sizeof(sin.sin_addr)) == 0)
		break;
	}
	if (hp->h_addr_list[i] == 0) {
	    msg_warn("address %s not listed for name %s",
		     state->addr, state->name);
	    myfree(state->name);
	    state->name = mystrdup("unknown");
	    state->peer_code = 5;
	    break;
	}
	break;
    }
    state->namaddr =
	concatenate(state->name, "[", state->addr, "]", (char *) 0);
}

/* smtpd_peer_reset - destroy peer information */

void    smtpd_peer_reset(SMTPD_STATE *state)
{
    if (state->name)
	myfree(state->name);
    if (state->addr)
	myfree(state->addr);
    if (state->namaddr)
	myfree(state->namaddr);
}
