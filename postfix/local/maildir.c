/*++
/* NAME
/*	maildir 3
/* SUMMARY
/*	delivery to maildir
/* SYNOPSIS
/*	#include "local.h"
/*
/*	int	deliver_maildir(state, usr_attr, path)
/*	LOCAL_STATE state;
/*	USER_ATTR usr_attr;
/*	char	*path;
/* DESCRIPTION
/*	deliver_maildir() delivers a message to a qmail maildir.
/*
/*	Arguments:
/* .IP state
/*	The attributes that specify the message, recipient and more.
/*	Attributes describing alias, include or forward expansion.
/*	A table with the results from expanding aliases or lists.
/* .IP usr_attr
/*	Attributes describing user rights and environment information.
/* .IP path
/*	The maildir to deliver to, including trailing slash.
/* DIAGNOSTICS
/*	deliver_maildir() always succeeds or it bounces the message.
/* SEE ALSO
/*	bounce(3)
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

#include "sys_defs.h"
#include <unistd.h>
#include <errno.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <stringops.h>
#include <vstream.h>
#include <vstring.h>
#include <make_dirs.h>
#include <set_eugid.h>
#include <get_hostname.h>

/* Global library. */

#include <mail_copy.h>
#include <bounce.h>
#include <sent.h>
#include <mail_params.h>

/* Application-specific. */

#include "local.h"

/* deliver_maildir - delivery to maildir-style mailbox */

int     deliver_maildir(LOCAL_STATE state, USER_ATTR usr_attr, char *path)
{
    char   *newdir;
    char   *tmpdir;
    char   *tmpfile;
    char   *newfile;
    VSTRING *why;
    VSTRING *buf;
    VSTREAM *dst;
    int     status;
    int     copy_flags;
    static int count;

    if (msg_verbose)
	msg_info("deliver_maildir: %s %s", state.msg_attr.recipient, path);

    /*
     * Initialize. Assume the operation will fail. Set the delivered
     * attribute to reflect the final recipient.
     */
    if (vstream_fseek(state.msg_attr.fp, state.msg_attr.offset, SEEK_SET) < 0)
	msg_fatal("seek message file %s: %m", VSTREAM_PATH(state.msg_attr.fp));
    state.msg_attr.delivered = state.msg_attr.recipient;
    status = -1;
    buf = vstring_alloc(100);
    why = vstring_alloc(100);

    copy_flags = MAIL_COPY_TOFILE;
    if ((state.msg_attr.features & FEATURE_NODELIVERED) == 0)
	copy_flags |= MAIL_COPY_DELIVERED;

    newdir = concatenate(path, "new/", (char *) 0);
    tmpdir = concatenate(path, "tmp/", (char *) 0);

    /*
     * Create and write the file as the recipient, so that file quota work.
     * Create any missing directories on the fly.
     */
#define STR vstring_str

    set_eugid(usr_attr.uid, usr_attr.gid);
    vstring_sprintf(buf, "%ld.%d.%s.%d", (long) time((time_t *) 0),
		    var_pid, get_hostname(), count++);
    tmpfile = concatenate(tmpdir, STR(buf), (char *) 0);
    newfile = concatenate(newdir, STR(buf), (char *) 0);
    if ((dst = vstream_fopen(tmpfile, O_WRONLY | O_CREAT | O_EXCL, 0600)) == 0
	&& (errno != ENOENT
	    || make_dirs(tmpdir, 0700) < 0
	    || (dst = vstream_fopen(tmpfile, O_WRONLY | O_CREAT | O_EXCL, 0600)) == 0)) {
	vstring_sprintf(why, "create %s: %m", tmpfile);
    } else {
	if (mail_copy(COPY_ATTR(state.msg_attr), dst, copy_flags, why) == 0) {
	    if (link(tmpfile, newfile) < 0
		&& (errno != ENOENT
		    || make_dirs(newdir, 0700) < 0
		    || link(tmpfile, newfile) < 0)) {
		vstring_sprintf(why, "link to %s: %m", newfile);
	    } else {
		if (unlink(tmpfile) < 0)
		    msg_warn("remove %s: %m", tmpfile);
		status = 0;
	    }
	}
    }
    set_eugid(var_owner_uid, var_owner_gid);

    if (status)
	bounce_append(BOUNCE_FLAG_KEEP, BOUNCE_ATTR(state.msg_attr),
		      "maildir delivery failed: %s", vstring_str(why));
    else
	sent(SENT_ATTR(state.msg_attr), "maildir");
    vstring_free(buf);
    vstring_free(why);
    myfree(newdir);
    myfree(tmpdir);
    myfree(tmpfile);
    myfree(newfile);
    return (0);
}
