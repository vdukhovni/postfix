/*++
/* NAME
/*	feature 3
/* SUMMARY
/*	toggle features depending on address
/* SYNOPSIS
/*	#include "local.h"
/*
/*	int	feature_control(state)
/*	LOCAL_STATE state;
/* DESCRIPTION
/*	feature_control() breaks the localpart of the recipient
/*	address up into fields, according to the recipient feature
/*	delimiter, and turns on/off the features as encountered.
/*
/*	Arguments:
/* .IP state
/*	The attributes that specify the message, recipient and more.
/*	Attributes describing the alias, include or forward expansion.
/*	A table with the results from expanding aliases or lists.
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

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

/* Utility library. */

#include <msg.h>
#include <stringops.h>
#include <mymalloc.h>

/* Global library. */

#include <mail_params.h>

/* Application-specific. */

#include "local.h"

struct feature_map {
    char   *name;
    int     mask;
};

static struct feature_map feature_map[] = {
    "nodelivered", FEATURE_NODELIVERED,
    0,
};

/* feature_control - extract delivery options from recipient localpart */

int     feature_control(const char *localpart)
{
    struct feature_map *mp;
    char   *saved_localpart;
    char   *ptr;
    int     mask = 0;
    char   *cp;

    if (*var_rcpt_fdelim) {
	ptr = saved_localpart = mystrdup(localpart);
	while ((cp = mystrtok(&ptr, var_rcpt_fdelim)) != 0) {
	    for (mp = feature_map; mp->name; mp++)
		if (strcasecmp(mp->name, cp) == 0) {
		    if (msg_verbose)
			msg_info("feature: %s", mp->name);
		    mask |= mp->mask;
		    break;
		}
	}
	myfree(saved_localpart);
    }
    if (msg_verbose)
	msg_info("features: 0x%x", mask);
    return (mask);
}
