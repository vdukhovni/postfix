/*++
/* NAME
/*	virtual 8
/* SUMMARY
/*	Postfix virtual domain mail delivery agent
/* SYNOPSIS
/*	\fBvirtual\fR [generic Postfix daemon options]
/* DESCRIPTION
/*	This daemon is designed for ISP's offering virtual mail hosting
/*	services. Originally based on the local delivery agent, this agent
/*	locates user mailboxes via map lookups of the full recipient
/*	address, rather than hard-coded unix password file searches of
/*	the local part only.
/*
/*	The \fBvirtual\fR daemon processes delivery requests from the
/*	Postfix queue manager to deliver mail to virtual local recipients.
/*	Each delivery request specifies a queue file, a sender address,
/*	a domain or host to deliver to, and one or more recipients.
/*	This program expects to be run from the \fBmaster\fR(8) process
/*	manager.
/*
/*	The \fBvirtual\fR daemon updates queue files and marks recipients
/*	as finished, or it informs the queue manager that delivery should
/*	be tried again at a later time. Delivery problem reports are sent
/*	to the \fBbounce\fR(8) or \fBdefer\fR(8) daemon as appropriate.
/* MAILBOX DELIVERY
/* .ad
/* .fi
/*	The \fBvirtual\fR delivery agent can deliver to UNIX-style mailbox
/*	file or to qmail-style maildir files. The pathname and delivery
/*	mailbox style are controlled by the $\fBvirtual_mailbox_base\fR
/*	and \fB$virtual_mailbox_maps\fR parameters (see below).
/*
/*	In the case of UNIX-style mailbox delivery,
/*	the \fBlocal\fR daemon prepends a "\fBFrom \fIsender time_stamp\fR"
/*	envelope header to each message, prepends a \fBDelivered-To:\fR header
/*	with the envelope recipient address, prepends a \fBReturn-Path:\fR
/*	header with the envelope sender address, prepends a \fB>\fR character
/*	to lines beginning with "\fBFrom \fR", and appends an empty line.
/*	The mailbox is locked for exclusive access while delivery is in
/*	progress. In case of problems, an attempt is made to truncate the
/*	mailbox to its original length.
/*
/*	In the case of \fBmaildir\fR delivery, the local daemon prepends
/*	a \fBDelivered-To:\fR header with the envelope recipient address
/*	and prepends a \fBReturn-Path:\fR header with the envelope sender
/*	address.
/* DELIVERY RIGHTS
/* .ad
/* .fi
/*	Deliveries to mailboxes are made with the UID and GID that are listed
/*	for the recipient in the tables listed in the \fB$virtual_uid_maps\fR
/*	and \fB$virtual_gid_maps\fR, respectively.
/*
/*	The \fBvirtual_minimum_uid\fR parameter specifies a lower bound on
/*	UID values that may be specified in \fB$virtual_uid_maps\fR. Mail
/*	will not be delivered when an invalid UID is found.
/* STANDARDS
/*	RFC 822 (ARPA Internet Text Messages)
/* DIAGNOSTICS
/*	Problems and transactions are logged to \fBsyslogd\fR(8).
/*	Corrupted message files are marked so that the queue
/*	manager can move them to the \fBcorrupt\fR queue afterwards.
/*
/*	Depending on the setting of the \fBnotify_classes\fR parameter,
/*	the postmaster is notified of bounces and of other trouble.
/* CONFIGURATION PARAMETERS
/* .ad
/* .fi
/*	The following \fBmain.cf\fR parameters are especially relevant to
/*	this program. See the Postfix \fBmain.cf\fR file for syntax details
/*	and for default values. Use the \fBpostfix reload\fR command after
/*	a configuration change.
/* .SH Mailbox delivery
/* .ad
/* .fi
/* .IP \fBvirtual_mailbox_base\fR
/*	Specifies a path that is prepended to all mailbox or maildir paths.
/*	This is a safety measure to ensure that an out of control map in
/*	\fBvirtual_mailbox_maps\fR doesn't litter the filesystem with mailboxes.
/*	While it could be set to "/", this setting isn't recommended.
/* .IP \fBvirtual_mailbox_maps\fR
/*	Recipients are looked up in this map to determine the path to
/*	their mailbox or maildir. If the returned path ends in a slash
/*	("/"), maildir-style delivery is carried out, otherwise the
/*	path is assumed to specify a mailbox file.
/*
/*	Note that \fBvirtual_mailbox_base\fR is unconditionally prepended
/*	to this path.
/* .IP \fBvirtual_minimum_uid\fR
/*	Specifies a minimum uid that will be accepted as a return from
/*	a \fBvirtual_uid_maps\fR lookup. Returned values less than this
/*	will be rejected, and the message will be deferred.
/* .IP \fBvirtual_uid_maps\fR
/*	Recipients are looked up in this map to determine the UID to be
/*	used when writing to the target mailbox.
/* .IP \fBvirtual_gid_maps\fR
/*	Recipients are looked up in this map to determine the GID to be
/*	used when writing to the target mailbox.
/* .SH "Locking controls"
/* .ad
/* .fi
/* .IP \fBmailbox_delivery_lock\fR
/*	How to lock UNIX-style mailboxes: one or more of \fBflock\fR,
/*	\fBfcntl\fR or \fBdotlock\fR.
/* .IP \fBdeliver_lock_attempts\fR
/*	Limit the number of attempts to acquire an exclusive lock
/*	on a mailbox file.
/* .IP \fBdeliver_lock_delay\fR
/*	Time in seconds between successive attempts to acquire
/*	an exclusive lock on a mailbox file.
/* .IP \fBstale_lock_time\fR
/*	Limit the time after which a stale lockfile is removed.
/* .SH "Resource controls"
/* .ad
/* .fi
/* .IP \fBvirtual_destination_concurrency_limit\fR
/*	Limit the number of parallel deliveries to the same domain.
/*	The default limit is taken from the
/*	\fBdefault_destination_concurrency_limit\fR parameter.
/* .IP \fBvirtual_destination_recipient_limit\fR
/*	Limit the number of recipients per message delivery.
/*	The default limit is taken from the
/*	\fBdefault_destination_recipient_limit\fR parameter.
/* HISTORY
/* .ad
/* .fi
/*	This agent was originally based on the local delivery
/*	agent. Modifications mainly consisted of removing code that wasn't
/*	applicable or wasn't safe in this context (aliases, .forwards,
/*	program aliases).
/*
/*	The \fBDelivered-To:\fR header appears in the \fBqmail\fR system
/*	by Daniel Bernstein.
/*
/*	The \fImaildir\fR structure appears in the \fBqmail\fR system
/*	by Daniel Bernstein.
/* SEE ALSO
/*	bounce(8) non-delivery status reports
/*	syslogd(8) system logging
/*	qmgr(8) queue manager
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
/*	Andrew McNamara
/*	andrewm@connect.com.au
/*	connect.com.au Pty. Ltd.
/*	Level 3, 213 Miller St
/*	North Sydney 2060, NSW, Australia
/*--*/

/* System library. */

#include <sys_defs.h>

/* Utility library. */

#include <msg.h>
#include <vstring.h>
#include <vstream.h>
#include <iostuff.h>
#include <set_eugid.h>
#include <dict.h>

/* Global library. */

#include <mail_queue.h>
#include <recipient_list.h>
#include <deliver_request.h>
#include <deliver_completed.h>
#include <mail_params.h>
#include <mail_conf.h>
#include <mail_params.h>

/* Single server skeleton. */

#include <mail_server.h>

/* Application-specific. */

#include "virtual.h"

 /*
  * Tunable parameters.
  */
char   *var_mailbox_maps;
char   *var_uid_maps;
char   *var_gid_maps;
int     var_virt_minimum_uid;
char   *var_virt_mailbox_base;
char   *var_mailbox_lock;

 /*
  * Mappings.
  */
MAPS   *virtual_mailbox_maps;
MAPS   *virtual_uid_maps;
MAPS   *virtual_gid_maps;

 /*
  * Bit masks.
  */
int     virtual_mbox_lock_mask;

/* local_deliver - deliver message with extreme prejudice */

static int local_deliver(DELIVER_REQUEST *rqst, char *service)
{
    char   *myname = "local_deliver";
    RECIPIENT *rcpt_end = rqst->rcpt_list.info + rqst->rcpt_list.len;
    RECIPIENT *rcpt;
    int     rcpt_stat;
    int     msg_stat;
    LOCAL_STATE state;
    USER_ATTR usr_attr;

    if (msg_verbose)
	msg_info("local_deliver: %s from %s", rqst->queue_id, rqst->sender);

    /*
     * Initialize the delivery attributes that are not recipient specific.
     * While messages are being delivered and while aliases or forward files
     * are being expanded, this attribute list is being changed constantly.
     * For this reason, the list is passed on by value (except when it is
     * being initialized :-), so that there is no need to undo attribute
     * changes made by lower-level routines. The alias/include/forward
     * expansion attribute list is part of a tree with self and parent
     * references (see the EXPAND_ATTR definitions). The user-specific
     * attributes are security sensitive, and are therefore kept separate.
     * All this results in a noticeable level of clumsiness, but passing
     * things around by value gives good protection against accidental change
     * by subroutines.
     */
    state.level = 0;
    deliver_attr_init(&state.msg_attr);
    state.msg_attr.queue_name = rqst->queue_name;
    state.msg_attr.queue_id = rqst->queue_id;
    state.msg_attr.fp = rqst->fp;
    state.msg_attr.offset = rqst->data_offset;
    state.msg_attr.sender = rqst->sender;
    state.msg_attr.relay = service;
    state.msg_attr.arrival_time = rqst->arrival_time;
    RESET_USER_ATTR(usr_attr, state.level);
    state.request = rqst;

    /*
     * Iterate over each recipient named in the delivery request. When the
     * mail delivery status for a given recipient is definite (i.e. bounced
     * or delivered), update the message queue file and cross off the
     * recipient. Update the per-message delivery status.
     */
    for (msg_stat = 0, rcpt = rqst->rcpt_list.info; rcpt < rcpt_end; rcpt++) {
	state.msg_attr.recipient = rcpt->address;
	rcpt_stat = deliver_recipient(state, usr_attr);
	if (rcpt_stat == 0)
	    deliver_completed(state.msg_attr.fp, rcpt->offset);
	msg_stat |= rcpt_stat;
    }

    return (msg_stat);
}

/* local_service - perform service for client */

static void local_service(VSTREAM *stream, char *service, char **argv)
{
    DELIVER_REQUEST *request;
    int     status;

    /*
     * Sanity check. This service takes no command-line arguments.
     */
    if (argv[0])
	msg_fatal("unexpected command-line argument: %s", argv[0]);

    /*
     * This routine runs whenever a client connects to the UNIX-domain socket
     * that is dedicated to local mail delivery service. What we see below is
     * a little protocol to (1) tell the client that we are ready, (2) read a
     * delivery request from the client, and (3) report the completion status
     * of that request.
     */
    if ((request = deliver_request_read(stream)) != 0) {
	status = local_deliver(request, service);
	deliver_request_done(stream, request, status);
    }
}

/* pre_accept - see if tables have changed */

static void pre_accept(char *unused_name, char **unused_argv)
{
    if (dict_changed()) {
	msg_info("table has changed -- exiting");
	exit(0);
    }
}

/* post_init - post-jail initialization */

static void post_init(char *unused_name, char **unused_argv)
{

    /*
     * Drop privileges most of the time.
     */
    set_eugid(var_owner_uid, var_owner_gid);

    virtual_mailbox_maps =
	maps_create(VAR_VIRT_MAILBOX_MAPS, var_mailbox_maps,
		    DICT_FLAG_LOCK);

    virtual_uid_maps =
	maps_create(VAR_VIRT_UID_MAPS, var_uid_maps, DICT_FLAG_LOCK);

    virtual_gid_maps =
	maps_create(VAR_VIRT_UID_MAPS, var_gid_maps, DICT_FLAG_LOCK);

    virtual_mbox_lock_mask = mbox_lock_mask(var_mailbox_lock);
}

/* main - pass control to the single-threaded skeleton */

int     main(int argc, char **argv)
{
    static CONFIG_INT_TABLE int_table[] = {
	VAR_VIRT_MINUID, DEF_VIRT_MINUID, &var_virt_minimum_uid, 1, 0,
	0,
    };
    static CONFIG_STR_TABLE str_table[] = {
	VAR_VIRT_MAILBOX_MAPS, DEF_VIRT_MAILBOX_MAPS, &var_mailbox_maps, 0, 0,
	VAR_VIRT_UID_MAPS, DEF_VIRT_UID_MAPS, &var_uid_maps, 0, 0,
	VAR_VIRT_GID_MAPS, DEF_VIRT_GID_MAPS, &var_gid_maps, 0, 0,
	VAR_VIRT_MAILBOX_BASE, DEF_VIRT_MAILBOX_BASE, &var_virt_mailbox_base, 0, 0,
	VAR_MAILBOX_LOCK, DEF_MAILBOX_LOCK, &var_mailbox_lock, 1, 0,
	0,
    };

    single_server_main(argc, argv, local_service,
		       MAIL_SERVER_INT_TABLE, int_table,
		       MAIL_SERVER_STR_TABLE, str_table,
		       MAIL_SERVER_POST_INIT, post_init,
		       MAIL_SERVER_PRE_ACCEPT, pre_accept,
		       0);
}
