/*++
/* NAME
/*	dotforward 3
/* SUMMARY
/*	$HOME/.forward file expansion
/* SYNOPSIS
/*	#include "local.h"
/*
/*	int	deliver_dotforward(state, usr_attr, statusp)
/*	LOCAL_STATE state;
/*	USER_ATTR usr_attr;
/*	int	*statusp;
/* DESCRIPTION
/*	deliver_dotforward() delivers a message to the destinations
/*	listed in a recipient's $HOME/.forward file.  The result is
/*	zero when no acceptable $HOME/.forward file was found, or when
/*	a recipient is listed in her own .forward file.
/*
/*	When mail is sent to an extended address (e.g., user+foo),
/*	the address extension is appended to the .forward file name
/*	(e.g., .forward+foo). When that file does not exist, .forward
/*	is used instead.
/*
/*	Arguments:
/* .IP state
/*	Message delivery attributes (sender, recipient etc.).
/*	Attributes describing alias, include or forward expansion.
/*	A table with the results from expanding aliases or lists.
/*	A table with delivered-to: addresses taken from the message.
/* .IP usr_attr
/*	Attributes describing user rights and environment.
/* .IP statusp
/*	Message delivery status. See below.
/* DIAGNOSTICS
/*	Fatal errors: out of memory. Warnings: bad $HOME/.forward
/*	file type, permissions or ownership.  The message delivery
/*	status is non-zero when delivery should be tried again.
/* SEE ALSO
/*	include(3) include file processor.
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
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#ifdef USE_PATHS_H
#include <paths.h>
#endif
#include <string.h>

/* Utility library. */

#include <msg.h>
#include <vstring.h>
#include <vstream.h>
#include <htable.h>
#include <open_as.h>
#include <lstat_as.h>
#include <iostuff.h>
#include <stringops.h>
#include <mymalloc.h>

/* Global library. */

#include <mypwd.h>
#include <bounce.h>
#include <been_here.h>
#include <mail_params.h>

/* Application-specific. */

#include "local.h"

#define NO	0
#define YES	1

/* deliver_dotforward - expand contents of .forward file */

int     deliver_dotforward(LOCAL_STATE state, USER_ATTR usr_attr, int *statusp)
{
    char   *myname = "deliver_dotforward";
    struct stat st;
    VSTRING *path;
    struct mypasswd *mypwd;
    int     fd;
    VSTREAM *fp;
    int     status;
    int     forward_found = NO;
    int     lookup_status;
    int     addr_count;
    char   *extension;

    /*
     * Make verbose logging easier to understand.
     */
    state.level++;
    if (msg_verbose)
	MSG_LOG_STATE(myname, state);

    /*
     * DUPLICATE/LOOP ELIMINATION
     * 
     * If this user includes (an alias of) herself in her own .forward file,
     * deliver to the user instead.
     */
    if (been_here(state.dup_filter, "forward %s", state.msg_attr.local))
	return (NO);
    state.msg_attr.exp_from = state.msg_attr.local;

    /*
     * Skip non-existing users. The mailbox delivery routine will catch the
     * error.
     */
    if ((mypwd = mypwnam(state.msg_attr.local)) == 0)
	return (NO);

    /*
     * From here on no early returns or we have a memory leak.
     */

    /*
     * EXTERNAL LOOP CONTROL
     * 
     * Set the delivered message attribute to the recipient, so that this
     * message will list the correct forwarding address.
     */
    state.msg_attr.delivered = state.msg_attr.recipient;

    /*
     * DELIVERY RIGHTS
     * 
     * Do not inherit rights from the .forward file owner. Instead, use the
     * recipient's rights, and insist that the .forward file is owned by the
     * recipient. This is a small but significant difference. Use the
     * recipient's rights for all /file and |command deliveries, and pass on
     * these rights to command/file destinations in included files. When
     * these are the rights of root, the /file and |command delivery routines
     * will use unprivileged default rights instead. Better safe than sorry.
     */
    if (mypwd->pw_uid != 0)
	SET_USER_ATTR(usr_attr, mypwd, state.level);

    /*
     * DELIVERY POLICY
     * 
     * Update the expansion type attribute so that we can decide if deliveries
     * to |command and /file/name are allowed at all.
     */
    state.msg_attr.exp_type = EXPAND_TYPE_FWD;

    /*
     * WHERE TO REPORT DELIVERY PROBLEMS
     * 
     * Set the owner attribute so that 1) include files won't set the sender to
     * be this user and 2) mail forwarded to other local users will be
     * resubmitted as a new queue file.
     */
    state.msg_attr.owner = state.msg_attr.recipient;

    /*
     * Assume that usernames do not have file system meta characters. Open
     * the .forward file as the user. Ignore files that aren't regular files,
     * files that are owned by the wrong user, or files that have world write
     * permission enabled. We take no special precautions to deal with home
     * directories imported via NFS, because mailbox and .forward files
     * should always be local to the host running the delivery process.
     * Anything else is just asking for trouble when a server goes down
     * (either the mailbox server or the home directory server).
     * 
     * With mail to user+foo, try ~/.forward+foo before ~/.forward. Ignore foo
     * when it contains '/' or when forward+foo does not exist.
     */
#define STR(x)	vstring_str(x)

    status = 0;
    path = vstring_alloc(100);
    extension = state.msg_attr.extension;
    if (extension && strchr(extension, '/')) {
	msg_warn("%s: address with illegal extension: %s",
		 state.msg_attr.queue_id, state.msg_attr.recipient);
	extension = 0;
    }
    if (extension != 0) {
	vstring_sprintf(path, "%s/.forward%c%s", mypwd->pw_dir,
			var_rcpt_delim[0], extension);
	if ((lookup_status = lstat_as(STR(path), &st,
				      usr_attr.uid, usr_attr.gid)) < 0)
	    extension = 0;
    }
    if (extension == 0) {
	vstring_sprintf(path, "%s/.forward", mypwd->pw_dir);
	lookup_status = lstat_as(STR(path), &st, usr_attr.uid, usr_attr.gid);
    }
    if (lookup_status >= 0) {
	if (S_ISREG(st.st_mode) == 0) {
	    msg_warn("file %s is not a regular file", STR(path));
	} else if (st.st_uid != 0 && st.st_uid != usr_attr.uid) {
	    msg_warn("file %s has bad owner uid %d", STR(path), st.st_uid);
	} else if (st.st_mode & 002) {
	    msg_warn("file %s is world writable", STR(path));
	} else if ((fd = open_as(STR(path), O_RDONLY, 0, usr_attr.uid, usr_attr.gid)) < 0) {
	    msg_warn("cannot open file %s: %m", STR(path));
	} else {
	    close_on_exec(fd, CLOSE_ON_EXEC);
	    addr_count = 0;
	    fp = vstream_fdopen(fd, O_RDONLY);
	    status = deliver_token_stream(state, usr_attr, fp, &addr_count);
	    if (vstream_fclose(fp))
		msg_warn("close file %s: %m", STR(path));
	    if (addr_count > 0)
		forward_found = YES;
	}
    }

    /*
     * Clean up.
     */
    vstring_free(path);
    mypwfree(mypwd);

    *statusp = status;
    return (forward_found);
}
