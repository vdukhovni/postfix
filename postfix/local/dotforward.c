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
/*	listed in a recipient's .forward file(s) as specified through
/*	the forward_path configuration parameter.  The result is
/*	zero when no acceptable .forward file was found, or when
/*	a recipient is listed in her own .forward file.
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
#include <mac_parse.h>

/* Global library. */

#include <mypwd.h>
#include <bounce.h>
#include <been_here.h>
#include <mail_params.h>
#include <config.h>

/* Application-specific. */

#include "local.h"

#define NO	0
#define YES	1

 /*
  * A little helper structure for message-specific context.
  */
typedef struct {
    int     failures;			/* $name not available */
    struct mypasswd *pwd;		/* recipient */
    char   *extension;			/* address extension */
    VSTRING *path;			/* result */
} FW_CONTEXT;

/* dotforward_parse_callback - callback for mac_parse */

static void dotforward_parse_callback(int type, VSTRING *buf, char *context)
{
    char   *myname = "dotforward_parse_callback";
    FW_CONTEXT *fw_context = (FW_CONTEXT *) context;
    char   *ptr;

    /*
     * Find out what data to substitute.
     */
    if (type == MAC_PARSE_VARNAME) {
	if (strcmp(vstring_str(buf), "home") == 0)
	    ptr = fw_context->pwd->pw_dir;
	else if (strcmp(vstring_str(buf), "user") == 0)
	    ptr = fw_context->pwd->pw_name;
	else if (strcmp(vstring_str(buf), "extension") == 0)
	    ptr = fw_context->extension;
	else if (strcmp(vstring_str(buf), "recipient_delimiter") == 0)
	    ptr = var_rcpt_delim;
	else
	    msg_fatal("unknown macro $%s in %s", vstring_str(buf),
		      VAR_FORWARD_PATH);
    } else {
	ptr = vstring_str(buf);
    }

    /*
     * Append the data, or record that the data was not available.
     */
    if (msg_verbose)
	msg_info("%s: %s = %s", myname, vstring_str(buf),
		 ptr ? ptr : "(unavailable)");
    if (ptr == 0) {
	fw_context->failures++;
    } else {
	vstring_strcat(fw_context->path, ptr);
    }
}

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
    char   *saved_forward_path;
    char   *lhs;
    char   *next;
    const char *forward_path;
    FW_CONTEXT fw_context;

    /*
     * Make verbose logging easier to understand.
     */
    state.level++;
    if (msg_verbose)
	MSG_LOG_STATE(myname, state);

    /*
     * Skip this module if per-user forwarding is disabled. XXX We need to
     * extend the config_XXX() interface to request no expansion of $names in
     * the given value or in the default value.
     */
    if ((forward_path = config_lookup(VAR_FORWARD_PATH)) == 0)
	forward_path = DEF_FORWARD_PATH;
    if (*forward_path == 0)
	return (NO);

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
     * permission enabled.
     */
#define STR(x)	vstring_str(x)

    status = 0;
    path = vstring_alloc(100);
    saved_forward_path = mystrdup(forward_path);
    next = saved_forward_path;

    fw_context.pwd = mypwd;
    fw_context.extension = state.msg_attr.extension;
    fw_context.path = path;

    lookup_status = -1;

    while ((lhs = mystrtok(&next, ", \t\r\n")) != 0) {
	fw_context.failures = 0;
	VSTRING_RESET(path);
	mac_parse(lhs, dotforward_parse_callback, (char *) &fw_context);
	if (fw_context.failures == 0) {
	    lookup_status =
		lstat_as(STR(path), &st, usr_attr.uid, usr_attr.gid);
	    if (msg_verbose)
		msg_info("%s: path %s status %d", myname,
			 STR(path), lookup_status);
	    if (lookup_status >= 0)
		break;
	}
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
    myfree(saved_forward_path);
    mypwfree(mypwd);

    *statusp = status;
    return (forward_found);
}
