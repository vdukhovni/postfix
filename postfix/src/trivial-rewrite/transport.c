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

#define STR(x)	vstring_str(x)

 /*
  * Macro for consistent updates of the transport and nexthop results.
  */
#define UPDATE_IF_SPECIFIED(dst, src) do { \
	if ((src) && *(src)) vstring_strcpy((dst), (src)); \
    } while (0)

/* transport_init - pre-jail initialization */

void    transport_init(void)
{
    if (transport_path)
	msg_panic("transport_init: repeated call");
    transport_path = maps_create("transport", var_transport_maps,
				 DICT_FLAG_LOCK);
    transport_match_parent_style = match_parent_style(VAR_TRANSPORT_MAPS);
}

/* find_transport_entry - look up and parse transport table entry */

static int find_transport_entry(const char *key, int flags,
				        VSTRING *channel, VSTRING *nexthop)
{
    char   *saved_value;
    const char *host;
    const char *value;

#define FOUND		1
#define NOTFOUND	0

    if (transport_path == 0)
	msg_panic("find_transport_entry: missing initialization");

    /*
     * Look up an entry with extreme prejedice.
     * 
     * XXX Should report lookup failure status to caller instead of aborting.
     */
    if ((value = maps_find(transport_path, key, flags)) == 0) {
	if (dict_errno != 0)
	    msg_fatal("transport table lookup problem.");
	return (NOTFOUND);
    }

    /*
     * Can't do transport:user@domain because the right-hand side can have
     * arbitrary content (especially in the case of the error mailer).
     */
    else {
	saved_value = mystrdup(value);
	host = split_at(saved_value, ':');
	UPDATE_IF_SPECIFIED(nexthop, host);
	UPDATE_IF_SPECIFIED(channel, saved_value);
	myfree(saved_value);
	return (FOUND);
    }
}

/* transport_wildcard_init - post-jail initialization */

void    transport_wildcard_init(void)
{
    VSTRING *channel = vstring_alloc(10);
    VSTRING *nexthop = vstring_alloc(10);

    /*
     * Technically, the wildcard lookup pattern is redundant. A static map
     * (keys always match, result is fixed string) could achieve the same:
     * 
     * transport_maps = hash:/etc/postfix/transport static:xxx:yyy
     * 
     * But the user interface of such an approach would be less intuitive. We
     * tolerate the continued existence of wildcard lookup patterns because
     * of human interface considerations.
     */
#define WILDCARD	"*"
#define FULL		0
#define PARTIAL		DICT_FLAG_FIXED

    if (find_transport_entry(WILDCARD, FULL, channel, nexthop)) {
	wildcard_channel = channel;
	wildcard_nexthop = nexthop;
	if (msg_verbose)
	    msg_info("wildcard_{chan:hop}={%s:%s}",
	      vstring_str(wildcard_channel), vstring_str(wildcard_nexthop));
    } else {
	vstring_free(channel);
	vstring_free(nexthop);
    }
}

/* transport_lookup - map a transport domain */

int     transport_lookup(const char *addr, VSTRING *channel, VSTRING *nexthop)
{
    char   *full_addr = lowercase(mystrdup(*addr ? addr : var_xport_null_key));
    char   *stripped_addr;
    char   *ratsign = 0;
    const char *name;
    const char *next;
    int     found;

#define STREQ(x,y)	(strcmp((x), (y)) == 0)
#define DISCARD_EXTENSION ((char **) 0)

    /*
     * The optimizer will replace multiple instances of this macro expansion
     * by gotos to a single instance that does the same thing.
     */
#define RETURN_FREE(x) { \
	myfree(full_addr); \
	return (x); \
    }

    /*
     * If this is a special address such as <> do only one lookup of the full
     * string. Specify the FULL flag to include regexp maps in the query.
     */
    if (STREQ(full_addr, var_xport_null_key)) {
	if (find_transport_entry(full_addr, FULL, channel, nexthop))
	    RETURN_FREE(FOUND);
	RETURN_FREE(NOTFOUND);
    }

    /*
     * Look up the full address with the FULL flag to include regexp maps in
     * the query.
     */
    if ((ratsign = strrchr(full_addr, '@')) == 0 || ratsign[1] == 0)
	msg_panic("transport_lookup: bad address: \"%s\"", full_addr);

    if (find_transport_entry(full_addr, FULL, channel, nexthop))
	RETURN_FREE(FOUND);

    /*
     * If the full address did not match, and there is an address extension,
     * look up the stripped address with the PARTIAL flag to avoid matching
     * partial lookup keys with regular expressions.
     */
    if ((stripped_addr = strip_addr(full_addr, DISCARD_EXTENSION,
				    *var_rcpt_delim)) != 0) {
	if (find_transport_entry(stripped_addr, PARTIAL,
				 channel, nexthop)) {
	    myfree(stripped_addr);
	    RETURN_FREE(FOUND);
	} else {
	    myfree(stripped_addr);
	}
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
    for (found = 0, name = ratsign + 1; /* void */ ; name = next) {
	if (find_transport_entry(name, PARTIAL, channel, nexthop))
	    RETURN_FREE(FOUND);
	if ((next = strchr(name + 1, '.')) == 0)
	    break;
	if (transport_match_parent_style == MATCH_FLAG_PARENT)
	    next++;
    }

    /*
     * Fall back to the wild-card entry.
     */
    if (wildcard_channel) {
	UPDATE_IF_SPECIFIED(channel, STR(wildcard_channel));
	UPDATE_IF_SPECIFIED(nexthop, STR(wildcard_nexthop));
	RETURN_FREE(FOUND);
    }

    /*
     * We really did not find it.
     */
    RETURN_FREE(NOTFOUND);
}
