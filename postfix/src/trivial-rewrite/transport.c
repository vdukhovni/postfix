/*++
/* NAME
/*	transport 3
/* SUMMARY
/*	transport mapping
/* SYNOPSIS
/*	#include "transport.h"
/*
/*	void	transport_init()
/*
/*	int	transport_lookup(domain, channel, nexthop)
/*	const char *domain;
/*	VSTRING *channel;
/*	VSTRING *nexthop;
/* DESCRIPTION
/*	This module implements access to the table that maps transport
/*	domains to (channel, nexthop) tuples.
/*
/*	transport_init() performs initializations that should be
/*	done before the process enters the chroot jail, and
/*	before calling transport_lookup().
/*
/*	transport_lookup() finds the channel and nexthop for the given
/*	domain, and returns 1 if something was found.	Otherwise, 0
/*	is returned.
/* DIAGNOSTICS
/*	The global \fIdict_errno\fR is non-zero when the lookup
/*	should be tried again.
/* SEE ALSO
/*	maps(3), multi-dictionary search
/*	transport(5), format of transport map
/* FILES
/*	/etc/postfix/transport*
/* CONFIGURATION PARAMETERS
/*	transport_maps, names of maps to be searched.
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
#include <string.h>

/* Utility library. */

#include <msg.h>
#include <stringops.h>
#include <mymalloc.h>
#include <vstring.h>
#include <split_at.h>
#include <dict.h>

/* Global library. */

#include <mail_params.h>
#include <maps.h>
#include <match_parent_style.h>

/* Application-specific. */

#include "transport.h"

static MAPS *transport_path;
static int transport_match_parent_style;
static VSTRING *wildcard_channel;
static VSTRING *wildcard_nexthop;

/* transport_init - pre-jail initialization */

void    transport_init(void)
{
    if (transport_path)
	msg_panic("transport_init: repeated call");
    transport_path = maps_create("transport", var_transport_maps,
				 DICT_FLAG_LOCK);
    transport_match_parent_style = match_parent_style(VAR_TRANSPORT_MAPS);

}

/* transport_wildcard_init - post-jail initialization */

void    transport_wildcard_init(void)
{
    wildcard_channel = vstring_alloc(10);
    wildcard_nexthop = vstring_alloc(10);

    if (transport_lookup("*", wildcard_channel, wildcard_nexthop)) {
	if (msg_verbose)
	    msg_info("wildcard_{chan:hop}={%s:%s}",
	      vstring_str(wildcard_channel), vstring_str(wildcard_nexthop));
    } else {
	vstring_free(wildcard_channel);
	wildcard_channel = 0;
	vstring_free(wildcard_nexthop);
	wildcard_nexthop = 0;
    }
}

/* transport_lookup - map a transport domain */

int     transport_lookup(const char *domain, VSTRING *channel, VSTRING *nexthop)
{
    char   *low_domain = lowercase(mystrdup(domain));
    const char *name;
    const char *next;
    const char *value;
    const char *host;
    char   *saved_value;
    char   *transport;
    int     found = 0;

#define FULL	0
#define PARTIAL		DICT_FLAG_FIXED

    int     maps_flag = FULL;

    if (transport_path == 0)
	msg_panic("transport_lookup: missing initialization");

    /*
     * Keep stripping domain components until nothing is left or until a
     * matching entry is found.
     * 
     * If the entry specifies no nexthop host and/or delivery channel, do not
     * change caller-provided information.
     * 
     * If we find no match, then return the wildcard entry, if any.
     * 
     * After checking the full name, check for .upper.domain, to distinguish
     * between the upper domain and it's decendants, ala sendmail and tcp
     * wrappers.
     * 
     * Before changing the DB lookup result, make a copy first, in order to
     * avoid DB cache corruption.
     * 
     * Specify if a key is partial or full, to avoid matching partial keys with
     * regular expressions.
     */
    for (name = low_domain; /* void */ ; name = next) {
	if ((value = maps_find(transport_path, name, maps_flag)) != 0) {
	    saved_value = mystrdup(value);
	    if ((host = split_at(saved_value, ':')) != 0 && *host != 0)
		vstring_strcpy(nexthop, host);
	    if (*(transport = saved_value) != 0)
		vstring_strcpy(channel, transport);
	    myfree(saved_value);
	    found = 1;
	    break;
	} else if (dict_errno != 0) {
	    msg_fatal("transport table lookup problem");
	}
	if ((next = strchr(name + 1, '.')) == 0)
	    break;
	if (transport_match_parent_style == MATCH_FLAG_PARENT)
	    next++;
	maps_flag = PARTIAL;
    }
    myfree(low_domain);

    /*
     * Fall back to the wild-card entry.
     */
    if (found == 0 && wildcard_channel) {
	if (*vstring_str(wildcard_channel))
	    vstring_strcpy(channel, vstring_str(wildcard_channel));
	if (*vstring_str(wildcard_nexthop))
	    vstring_strcpy(nexthop, vstring_str(wildcard_nexthop));
	found = 1;
    }
    return (found);
}
