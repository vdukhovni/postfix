/*++
/* NAME
/*	hold_message 3
/* SUMMARY
/*	move message to hold queue
/* SYNOPSIS
/*	#include <hold_message.h>
/*
/*	void	hold_message(queue_name, queue_id)
/*	const char *queue_name;
/*	const char *queue_id;
/* DESCRIPTION
/*	The \fBhold_message\fR() routine moves the specified
/*	queue file to the \fBhold\fR queue, where it will sit
/*	until someone either destroys it or releases it.
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
#include <stdio.h>			/* rename() */
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

/* Utility library. */

#include <msg.h>
#include <set_eugid.h>

/* Global library. */

#include <mail_queue.h>
#include <mail_params.h>
#include <hold_message.h>

#define STR(x)	vstring_str(x)

/* hold_message - move message to hold queue */

void    hold_message(const char *queue_name, const char *queue_id)
{
    VSTRING *old_path = vstring_alloc(100);
    VSTRING *new_path = vstring_alloc(100);
    uid_t   saved_uid;
    gid_t   saved_gid;

    /*
     * If not running as the mail system, change privileges first.
     */
    if ((saved_uid = geteuid()) != var_owner_uid) {
	saved_gid = getegid();
	set_eugid(var_owner_uid, var_owner_gid);
    }

    /*
     * Grr. Don't do stupid things when this function is called multiple
     * times. sane_rename() would emit a bogus warning about spurious NFS
     * problems.
     */
    (void) mail_queue_path(old_path, queue_name, queue_id);
    (void) mail_queue_path(new_path, MAIL_QUEUE_HOLD, queue_id);
    if (access(STR(old_path), F_OK) == 0) {
	if (rename(STR(old_path), STR(new_path)) == 0
	    || access(STR(new_path), F_OK) == 0)
	    msg_info("%s: placed on hold", queue_id);
	else
	    msg_warn("%s: could not place message on hold: %m", queue_id);
    }

    /*
     * Restore privileges.
     */
    if (saved_uid != var_owner_uid)
	set_eugid(saved_uid, saved_gid);

    /*
     * Cleanup.
     */
    vstring_free(old_path);
    vstring_free(new_path);
}
