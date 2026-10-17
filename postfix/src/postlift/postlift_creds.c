/*++
/* NAME
/*	postlift_creds 8
/* SUMMARY
/*	postlift security zone hygiene
/* SYNOPSIS
/*	void	plift_valid_creds_or_die(uid_t my_uid, gid_t my_gid)
/* DESCRIPTION
/*	plift_valid_creds_or_die() enforces that the given real UID and
/*	GID belong respectively to the postdrop user and postdrop group,
/*	that they are the same as the effective UID and GID, that they
/*	differ from the default_privs or mail_owner UID or GID, and that
/*	they do not have the reserved value (-1) which disables owner
/*	or group changes in safe_open(). The process terminates with
/*	fatal run-time error if any condition is not met.
/* CONFIGURATION PARAMETERS
/* .IP "\fBdefault_privs (nobody)\fR"
/*	The default rights used by the \fBlocal\fR(8) delivery agent
/*	for delivery to an external file or command.
/* .IP "\fBmail_owner (postfix)\fR"
/*	The UNIX system account that owns the Postfix queue and most
/*	Postfix daemon processes.
/* .IP "\fBsetgid_group (postdrop)\fR"
/*	The group ownership of set-gid Postfix commands and of
/*	group-writable Postfix directories.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* HISTORY
/*	This service was introduced with Postfix version 3.12.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>
#include <unistd.h>

 /*
  * Utility library.
  */
#include <msg.h>
#include <mypwd.h>

 /*
  * Global library.
  */
#include <mail_params.h>

 /*
  * Application-specific.
  */
#include <postlift_creds.h>

/* plift_valid_creds_or_die - enforce security zone hygiene */

void    plift_valid_creds_or_die(uid_t my_uid, gid_t my_gid)
{
    struct mypasswd *postdrop_pwd;

    /*
     * Sanity checks: UID.
     */
    if (my_uid !=geteuid())
	msg_fatal("This program MUST NOT run with set-uid privileges");
    if ((postdrop_pwd = mypwnam(var_sgid_group)) == 0)
	msg_fatal("No '%s' account in the passwd database", var_sgid_group);
    if (my_uid != postdrop_pwd->pw_uid)
	msg_fatal("This program MUST run with the '%s' user ID (%lu)",
		  var_sgid_group, (unsigned long) postdrop_pwd->pw_uid);

    /*
     * Don't allow UID sharing with other accounts. Some of these checks may
     * overlap with similar checks in mail_params.c.
     */
    if (my_uid == 0)
	msg_fatal("This program MUST NOT run with the super-user ID (0)");
    if (my_uid == (uid_t) -1)
	msg_fatal("This program MUST NOT run with reserved user ID (-1)");
    if (my_uid == var_owner_uid)
	msg_fatal("This program MUST NOT run with the '%s' user ID (%lu)",
		  var_mail_owner, (unsigned long) var_owner_uid);
    if (my_uid == var_default_uid)
	msg_fatal("This program MUST NOT run with the '%s' user ID (%lu)",
		  var_default_privs, (unsigned long) var_default_uid);

    /*
     * Sanity checks: GID.
     */
    if (my_gid != getegid())
	msg_fatal("This program MUST NOT run with set-gid privileges");
    if (my_gid != var_sgid_gid)
	msg_fatal("This program MUST run with the '%s' group ID (%lu)",
		  var_sgid_group, (unsigned long) var_sgid_gid);

    /*
     * Don't allow GID sharing with other accounts. Some of these checks may
     * overlap with similar checks in mail_params.c.
     */
    if (my_gid == 0)
	msg_fatal("This program MUST NOT run with privileged group ID (0)");
    if (my_gid == (gid_t) -1)
	msg_fatal("This program MUST NOT run with reserved group ID (-1)");
    if (my_gid == var_owner_gid)
	msg_fatal("This program MUST NOT run with the '%s' group ID (%lu)",
		  var_mail_owner, (unsigned long) var_owner_gid);
    if (my_gid == var_default_uid)
	msg_fatal("This program MUST NOT run with the '%s' user ID (%lu)",
		  var_default_privs, (unsigned long) var_default_gid);
}
