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
/*	int	transport_lookup(address, channel, nexthop)
/*	const char *address;
/*	VSTRING *channel;
/*	VSTRING *nexthop;
/* DESCRIPTION
/*	This module implements access to the table that maps transport
/*	user@domain addresses to (channel, nexthop) tuples.
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
/*	strip_addr(3), strip extension from address
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

#include <strip_addr.h>
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

/* check_maps_find - map lookup with extreme prejudice */

static const char *check_maps_find(MAPS *maps, const char *key, int flags)
{
    const char *value;

    if ((value = maps_find(maps, key, flags)) == 0 && dict_errno != 0)
	msg_fatal("transport table lookup problem.");
    return (value);
}

/* transport_lookup - map a transport domain */

int     transport_lookup(const char *addr, VSTRING *channel, VSTRING *nexthop)
{
    char   *full_addr = lowercase(mystrdup(*addr ? addr : var_xport_null_key));
    char   *stripped_addr = 0;
    char   *ratsign = 0;
    const char *name;
    const char *next;
    const char *value;
    const char *host;
    char   *saved_value;
    char   *transport;
    int     found = 0;

#define FULL	0
#define PARTIAL		DICT_FLAG_FIXED
#define STRNE		strcmp
#define DISCARD_EXTENSION ((char **) 0)

    if (transport_path == 0)
	msg_panic("transport_lookup: missing initialization");

    if (STRNE(full_addr, var_xport_null_key) && STRNE(full_addr, "*"))
	if ((ratsign = strrchr(full_addr, '@')) == 0)
	    msg_panic("transport_lookup: bad address: \"%s\"", full_addr);

    /*
     * Look up the full address with the FULL flag to include regexp maps in
     * the query.
     */
    if ((value = check_maps_find(transport_path, full_addr, FULL)) != 0) {
	found = 1;
    }

    /*
     * If this is a special address such as <> or *, not user@domain, then we
     * are done now.
     */
    else if (ratsign == 0) {
	found = 0;
    }

    /*
     * If the full address did not match, and there is an address extension,
     * look up the stripped address with the PARTIAL flag to avoid matching
     * partial lookup keys with regular expressions.
     */
    else if ((stripped_addr = strip_addr(full_addr, DISCARD_EXTENSION,
					 *var_rcpt_delim)) != 0
	     && (value = check_maps_find(transport_path, stripped_addr,
					 PARTIAL)) != 0) {
	found = 1;
    }

    /*
     * If the full and stripped address lookup fails, try domain name lookup.
     * 
     * Keep stripping domain components until nothing is left or until a
     * matching entry is found.
     * 
     * After checking the full domain name, check for .upper.domain, to
     * distinguish between the parent domain and it's decendants, a la
     * sendmail and tcp wrappers.
     * 
     * Before changing the DB lookup result, make a copy first, in order to
     * avoid DB cache corruption.
     * 
     * Specify that the lookup key is partial, to avoid matching partial keys
     * with regular expressions.
     */
    else if (found == 0) {
	for (name = ratsign + 1; /* void */ ; name = next) {
	    if ((value = maps_find(transport_path, name, PARTIAL)) != 0) {
		found = 1;
		break;
	    }
	    if ((next = strchr(name + 1, '.')) == 0)
		break;
	    if (transport_match_parent_style == MATCH_FLAG_PARENT)
		next++;
	}
    }

    /*
     * XXX user+ext@ and user@ lookups for domains that resolve locally?
     */

    /*
     * We found an answer in the transport table.  Use the results to
     * override transport and/or next-hop information.
     */
    if (found == 1) {
	saved_value = mystrdup(value);
	if ((host = split_at(saved_value, ':')) != 0 && *host != 0) {
#if 0
	    if (strchr(host, '@'))
		vstring_strcpy(recipient, host);
	    else
#endif
		vstring_strcpy(nexthop, host);
	}
	if (*(transport = saved_value) != 0)
	    vstring_strcpy(channel, transport);
	myfree(saved_value);
    }

    /*
     * Clean up.
     */
    myfree(full_addr);
    if (stripped_addr)
	myfree(stripped_addr);

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
