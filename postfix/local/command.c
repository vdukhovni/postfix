/*++
/* NAME
/*	command 3
/* SUMMARY
/*	message delivery to shell command
/* SYNOPSIS
/*	#include "local.h"
/*
/*	int	deliver_command(state, usr_attr, command)
/*	LOCAL_STATE state;
/*	USER_ATTR exp_attr;
/*	char	*command;
/* DESCRIPTION
/*	deliver_command() runs a command with a message as standard
/*	input.  A limited amount of standard output and standard error
/*	output is captured for diagnostics purposes.
/*	Duplicate commands for the same recipient are suppressed.
/*
/*	Arguments:
/* .IP state
/*	The attributes that specify the message, recipient and more.
/*	Attributes describing the alias, include or forward expansion.
/*	A table with the results from expanding aliases or lists.
/* .IP usr_attr
/*	Attributes describing user rights and environment.
/* .IP command
/*	The shell command to be executed, after $name expansion of recipient
/*	attributes. If possible, the command is executed without actually
/*	invoking a shell.
/* DIAGNOSTICS
/*	deliver_command() returns non-zero when delivery should be
/*	tried again,
/* SEE ALSO
/*	mailbox(3) deliver to mailbox
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
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>

/* Utility library. */

#include <msg.h>
#include <htable.h>
#include <vstring.h>
#include <vstream.h>
#include <argv.h>

/* Global library. */

#include <defer.h>
#include <bounce.h>
#include <sent.h>
#include <been_here.h>
#include <mail_params.h>
#include <pipe_command.h>
#include <mail_copy.h>

/* Application-specific. */

#include "local.h"

/* deliver_command - deliver to shell command */

int     deliver_command(LOCAL_STATE state, USER_ATTR usr_attr, char *command)
{
    char   *myname = "deliver_command";
    VSTRING *why;
    int     cmd_status;
    int     deliver_status;
    ARGV   *env;
    int     copy_flags;
    static char *ok_chars = "1234567890!@%-_=+:,./\
abcdefghijklmnopqrstuvwxyz\
ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    VSTRING *expanded_cmd;

    /*
     * Make verbose logging easier to understand.
     */
    state.level++;
    if (msg_verbose)
	MSG_LOG_STATE(myname, state);

    /*
     * DUPLICATE ELIMINATION
     * 
     * Skip this command if it was already delivered to as this user.
     */
    if (been_here(state.dup_filter, "command %d %s", usr_attr.uid, command))
	return (0);

    /*
     * DELIVERY POLICY
     * 
     * Do we permit mail to shell commands? Allow delivery via mailbox_command.
     */
    if (command != var_mailbox_command
	&& (local_cmd_deliver_mask & state.msg_attr.exp_type) == 0)
	return (bounce_append(BOUNCE_FLAG_KEEP, BOUNCE_ATTR(state.msg_attr),
			      "mail to command is restricted"));

    /*
     * DELIVERY RIGHTS
     * 
     * Choose a default uid and gid when none have been selected (i.e. values
     * are still zero).
     */
    if (usr_attr.uid == 0 && (usr_attr.uid = var_default_uid) == 0)
	msg_panic("privileged default user id");
    if (usr_attr.gid == 0 && (usr_attr.gid = var_default_gid) == 0)
	msg_panic("privileged default group id");

    /*
     * Deliver.
     */
    copy_flags = MAIL_COPY_FROM | MAIL_COPY_RETURN_PATH;
    if ((state.msg_attr.features & FEATURE_NODELIVERED) == 0)
	copy_flags |= MAIL_COPY_DELIVERED;

    why = vstring_alloc(1);
    if (vstream_fseek(state.msg_attr.fp, state.msg_attr.offset, SEEK_SET) < 0)
	msg_fatal("%s: seek queue file %s: %m",
		  myname, VSTREAM_PATH(state.msg_attr.fp));

    /*
     * Pass additional environment information. XXX This should be
     * configurable. However, passing untrusted information via environment
     * parameters opens up a whole can of worms. Lesson from web servers:
     * don't let any network data even near a shell. It causes trouble.
     */
    env = argv_alloc(1);
    if (usr_attr.home)
	argv_add(env, "HOME", usr_attr.home, ARGV_END);
    if (usr_attr.logname)
	argv_add(env, "LOGNAME", usr_attr.logname, ARGV_END);
    if (usr_attr.shell)
	argv_add(env, "SHELL", usr_attr.shell, ARGV_END);
    argv_terminate(env);

    expanded_cmd = vstring_alloc(10);
    if (command == var_mailbox_command)
	local_expand(expanded_cmd, command, state, usr_attr, ok_chars);
    else
	vstring_strcpy(expanded_cmd, command);

    cmd_status = pipe_command(state.msg_attr.fp, why,
			      PIPE_CMD_UID, usr_attr.uid,
			      PIPE_CMD_GID, usr_attr.gid,
			      PIPE_CMD_COMMAND, vstring_str(expanded_cmd),
			      PIPE_CMD_COPY_FLAGS, copy_flags,
			      PIPE_CMD_SENDER, state.msg_attr.sender,
			      PIPE_CMD_DELIVERED, state.msg_attr.delivered,
			      PIPE_CMD_TIME_LIMIT, var_command_maxtime,
			      PIPE_CMD_ENV, env->argv,
			      PIPE_CMD_SHELL, var_local_cmd_shell,
			      PIPE_CMD_END);

    argv_free(env);
    vstring_free(expanded_cmd);

    /*
     * Depending on the result, bounce or defer the message.
     */
    switch (cmd_status) {
    case PIPE_STAT_OK:
	deliver_status = sent(SENT_ATTR(state.msg_attr), "\"|%s\"", command);
	break;
    case PIPE_STAT_BOUNCE:
	deliver_status = bounce_append(BOUNCE_FLAG_KEEP,
				       BOUNCE_ATTR(state.msg_attr),
				       "%s", vstring_str(why));
	break;
    case PIPE_STAT_DEFER:
	deliver_status = defer_append(BOUNCE_FLAG_KEEP,
				      BOUNCE_ATTR(state.msg_attr),
				      "%s", vstring_str(why));
	break;
    default:
	msg_panic("%s: bad status %d", myname, cmd_status);
	/* NOTREACHED */
    }

    /*
     * Cleanup.
     */
    vstring_free(why);

    return (deliver_status);
}
