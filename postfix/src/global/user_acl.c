/*++
/* NAME
/*	user_acl 3
/* SUMMARY
/*	Convert uid to username and check against given ACL.
/* SYNOPSIS
/*	#include <user_acl.h>
/*
/*	char   *check_user_acl_byuid(acl, uid)
/*	const char *acl;
/*	uid_t	uid;
/* DESCRIPTION
/*	check_user_acl_byuid() checks the given uid against a
/*	user name matchlist. If the uid cannot be resolved to a user
/*	name, the numeric uid is used as the lookup key instead.
/*	The result is NULL on success, "User \fIusername\fR" or
/*	"UID \fIuid\fR" upon failure. The error result lives in
/*	static storage and must be saved if it is to be used to
/*	across calls.
/*
/*	Arguments:
/* .IP acl
/*	Authorized username list suitable for input to string_list_init(3).
/* .IP uid
/*	The uid to be checked against the access list.
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
/*	Victor Duchovni
/*	Morgan Stanley
/*--*/

/* System library. */

#include <sys_defs.h>

/* Utility library. */

#include <vstring.h>

/* Global library. */

#include <string_list.h>
#include <mypwd.h>

/* Application-specific. */

#include "user_acl.h"

/* check_user_acl_byuid - check user authorization */

char   *check_user_acl_byuid(char *acl, uid_t uid)
{
    struct mypasswd *mypwd;
    STRING_LIST *list;
    static VSTRING *why = 0;
    VSTRING *uidbuf = 0;
    int     matched;
    const char *name;

    /*
     * XXX: we must perform a lookup for unresolved uids, so that
     * static:anyone results in "permit" even when the uid is not found in
     * the password file and the resulting error message is clear.
     */
    if ((mypwd = mypwuid(uid)) == 0) {
	uidbuf = vstring_alloc(10);
	vstring_sprintf(uidbuf, "%ld", (long) uid);
	name = vstring_str(uidbuf);
    } else {
	name = mypwd->pw_name;
    }

    list = string_list_init(MATCH_FLAG_NONE, acl);
    if ((matched = string_list_match(list, name)) == 0) {
	if (!why)
	    why = vstring_alloc(100);
	vstring_sprintf(why, "%s %s", mypwd ? "User" : "UID", name);
    }
    string_list_free(list);
    if (mypwd)
	mypwfree(mypwd);
    else
	vstring_free(uidbuf);

    return (matched ? 0 : vstring_str(why));
}
