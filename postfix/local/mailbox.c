/*++
/* NAME
/*	mailbox 3
/* SUMMARY
/*	mailbox delivery
/* SYNOPSIS
/*	#include "local.h"
/*
/*	int	deliver_mailbox(state, usr_attr)
/*	LOCAL_STATE state;
/*	USER_ATTR usr_attr;
/* DESCRIPTION
/*	deliver_mailbox() delivers to mailbox, with duplicate
/*	suppression. The default is direct mailbox delivery to
/*	/var/[spool/]mail/\fIuser\fR; when a \fIhome_mailbox\fR
/*	has been configured, mail is delivered to ~/$\fIhome_mailbox\fR;
/*	and when a \fImailbox_command\fR has been configured, the message
/*	is piped into the command instead.
/*
/*	Arguments:
/* .IP state
/*	The attributes that specify the message, recipient and more.
/*	Attributes describing alias, include or forward expansion.
/*	A table with the results from expanding aliases or lists.
/* .IP usr_attr
/*	Attributes describing user rights and environment.
/* DIAGNOSTICS
/*	deliver_mailbox() returns non-zero when delivery should be tried again,
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
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#ifdef USE_PATHS_H
#include <paths.h>
#endif

/* Utility library. */

#include <msg.h>
#include <htable.h>
#include <vstring.h>
#include <vstream.h>
#include <mymalloc.h>
#include <stringops.h>
#include <set_eugid.h>

/* Global library. */

#include <mail_copy.h>
#include <safe_open.h>
#include <deliver_flock.h>
#ifdef USE_DOT_LOCK
#include <dot_lockfile.h>
#endif
#include <bounce.h>
#include <defer.h>
#include <sent.h>
#include <mypwd.h>
#include <been_here.h>
#include <mail_params.h>

/* Application-specific. */

#include "local.h"
#include "biff_notify.h"

/* deliver_mailbox_file - deliver to recipient mailbox */

static int deliver_mailbox_file(LOCAL_STATE state, USER_ATTR usr_attr)
{
    char   *mailbox;
    VSTRING *why;
    VSTREAM *dst;
    int     status;
    int     copy_flags;
    VSTRING *biff;
    long    end;

    if (msg_verbose)
	msg_info("deliver_mailbox_file: %s", state.msg_attr.recipient);

    /*
     * Initialize. Assume the operation will fail. Set the delivered
     * attribute to reflect the final recipient.
     */
    if (vstream_fseek(state.msg_attr.fp, state.msg_attr.offset, SEEK_SET) < 0)
	msg_fatal("seek message file %s: %m", VSTREAM_PATH(state.msg_attr.fp));
    state.msg_attr.delivered = state.msg_attr.recipient;
    status = -1;
    why = vstring_alloc(100);
    if (*var_home_mailbox)
	mailbox = concatenate(usr_attr.home, "/", var_home_mailbox, (char *) 0);
    else
	mailbox = concatenate(_PATH_MAILDIR, "/", state.msg_attr.local, (char *) 0);

    /*
     * Lock the mailbox and open/create the mailbox file. Depending on the
     * type of locking used, we lock first or we open first.
     * 
     * Write the file as the recipient, so that file quota work.
     * 
     * Create lock files as root, for non-writable directories.
     */
    copy_flags = MAIL_COPY_MBOX;
    if (state.msg_attr.features & FEATURE_NODELIVERED)
	copy_flags &= ~MAIL_COPY_DELIVERED;

    set_eugid(0, 0);
#ifdef USE_DOT_LOCK
    if (dot_lockfile(mailbox, why) >= 0) {
#endif
	dst = safe_open(mailbox, O_APPEND | O_WRONLY | O_CREAT,
			S_IRUSR | S_IWUSR, usr_attr.uid, usr_attr.gid, why);
	set_eugid(usr_attr.uid, usr_attr.gid);
	if (dst != 0) {
	    end = vstream_fseek(dst, (off_t) 0, SEEK_END);
	    if (deliver_flock(vstream_fileno(dst), why) < 0)
		vstream_fclose(dst);
	    else if (mail_copy(COPY_ATTR(state.msg_attr), dst,
			       copy_flags, why) == 0) {
		status = 0;
		if (var_biff) {
		    biff = vstring_alloc(100);
		    vstring_sprintf(biff, "%s@%ld", usr_attr.logname,
				    (long) end);
		    biff_notify(vstring_str(biff), VSTRING_LEN(biff) + 1);
		    vstring_free(biff);
		}
	    }
	}
#ifdef USE_DOT_LOCK
	set_eugid(0, 0);
	dot_unlockfile(mailbox);
    }
#endif
    set_eugid(var_owner_uid, var_owner_gid);

    if (status)
	defer_append(BOUNCE_FLAG_KEEP, BOUNCE_ATTR(state.msg_attr),
		 "cannot append to file %s: %s", mailbox, vstring_str(why));
    else
	sent(SENT_ATTR(state.msg_attr), "mailbox");
    myfree(mailbox);
    vstring_free(why);
    return (status);
}

/* deliver_mailbox - deliver to recipient mailbox */

int     deliver_mailbox(LOCAL_STATE state, USER_ATTR usr_attr)
{
    char   *myname = "deliver_mailbox";
    int     status;
    struct mypasswd *mbox_pwd;
    char   *path;

    /*
     * Make verbose logging easier to understand.
     */
    state.level++;
    if (msg_verbose)
	msg_info("%s[%d]: %s", myname, state.level, state.msg_attr.recipient);

    /*
     * Strip quoting that was prepended to defeat alias/forward expansion.
     */
    if (state.msg_attr.recipient[0] == '\\')
	state.msg_attr.recipient++, state.msg_attr.local++;

    /*
     * DUPLICATE ELIMINATION
     * 
     * Don't deliver more than once to this mailbox.
     */
    if (been_here(state.dup_filter, "mailbox %s", state.msg_attr.local))
	return (0);

    /*
     * Bounce the message when this recipient does not exist. XXX Should
     * quote_822_local() the recipient.
     */
    if ((mbox_pwd = mypwnam(state.msg_attr.local)) == 0)
	return (bounce_append(BOUNCE_FLAG_KEEP, BOUNCE_ATTR(state.msg_attr),
			      "unknown user: \"%s\"", state.msg_attr.local));

    /*
     * No early returns or we have a memory leak.
     */

    /*
     * DELIVERY RIGHTS
     * 
     * Use the rights of the recipient user.
     */
    SET_USER_ATTR(usr_attr, mbox_pwd, state.level);

    /*
     * Deliver to mailbox or to external delivery agent.
     */
#define LAST_CHAR(s) (s[strlen(s) - 1])

    if (*var_mailbox_command)
	status = deliver_command(state, usr_attr, var_mailbox_command);
    else if (*var_home_mailbox && LAST_CHAR(var_home_mailbox) == '/') {
	path = concatenate(usr_attr.home, "/", var_home_mailbox, (char *) 0);
	status = deliver_maildir(state, usr_attr, path);
	myfree(path);
    } else
	status = deliver_mailbox_file(state, usr_attr);

    /*
     * Cleanup.
     */
    mypwfree(mbox_pwd);
    return (status);
}
