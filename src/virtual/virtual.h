/*++
/* NAME
/*	virtual 3h
/* SUMMARY
/*	virtual mail delivery
/* SYNOPSIS
/*	#include "virtual.h"
/* DESCRIPTION
/* .nf

 /*
  * System library.
  */
#include <unistd.h>

 /*
  * Utility library.
  */
#include <vstream.h>
#include <vstring.h>

 /*
  * Global library.
  */
#include <deliver_request.h>
#include <maps.h>
#include <mbox_conf.h>
#include <dsn_buf.h>
#include <dsn.h>

 /*
  * Mappings.
  */
extern MAPS *virtual_mailbox_maps;
extern MAPS *virtual_uid_maps;
extern MAPS *virtual_gid_maps;

 /*
  * User attributes: these control the privileges for delivery to external
  * commands, external files, or mailboxes, and the initial environment of
  * external commands.
  */
typedef struct USER_ATTR {
    uid_t   uid;			/* file/command access */
    gid_t   gid;			/* file/command access */
    char   *mailbox;			/* mailbox file or directory */
} USER_ATTR;

 /*
  * Critical macros. Not for obscurity, but to ensure consistency.
  */
#define RESET_USER_ATTR(usr_attr, level) { \
	usr_attr.uid = 0; usr_attr.gid = 0; usr_attr.mailbox = 0; \
	if (msg_verbose) \
	    msg_info("%s[%d]: reset user_attr", myname, level); \
    }

 /*
  * The delivery attributes are inherited from files, from aliases, and from
  * whatnot. Some of the information is changed on the fly. DELIVER_ATTR
  * structures are therefore passed by value, so there is no need to undo
  * changes.
  */
typedef struct DELIVER_ATTR {
    int     level;			/* recursion level */
    VSTREAM *fp;			/* open queue file */
    char   *queue_name;			/* mail queue id */
    char   *queue_id;			/* mail queue id */
    long    offset;			/* data offset */
    const char *sender;			/* taken from envelope */
    char   *dsn_envid;			/* DSN envelope ID */
    int     dsn_ret;			/* DSN headers/full */
    RECIPIENT rcpt;			/* from delivery request */
    char   *user;			/* recipient lookup handle */
    const char *delivered;		/* for loop detection */
    char   *relay;			/* relay host */
    MSG_STATS msg_stats;		/* time profile */
    DSN_BUF *why;			/* delivery status */
} DELIVER_ATTR;

extern void deliver_attr_init(DELIVER_ATTR *);
extern void deliver_attr_dump(DELIVER_ATTR *);
extern void deliver_attr_free(DELIVER_ATTR *);

#define FEATURE_NODELIVERED	(1<<0)	/* no delivered-to */

 /*
  * Rather than schlepping around dozens of arguments, here is one that has
  * all. Well, almost. The user attributes are just a bit too sensitive, so
  * they are passed around separately.
  */
typedef struct LOCAL_STATE {
    int     level;			/* nesting level, for logging */
    DELIVER_ATTR msg_attr;		/* message/recipient attributes */
    DELIVER_REQUEST *request;		/* as from queue manager */
} LOCAL_STATE;

 /*
  * Bundle up some often-user attributes.
  */
#define BOUNCE_FLAGS(request)	DEL_REQ_TRACE_FLAGS((request)->flags)

#define BOUNCE_ATTR(attr) \
	attr.queue_id, &attr.msg_stats, &attr.rcpt, attr.relay, \
	DSN_FROM_DSN_BUF(attr.why)
#define SENT_ATTR(attr) \
	attr.queue_id, &attr.msg_stats, &attr.rcpt, attr.relay, \
	DSN_FROM_DSN_BUF(attr.why)
#define COPY_ATTR(attr) \
	attr.sender, attr.rcpt.orig_addr, attr.delivered, attr.fp

#define MSG_LOG_STATE(m, p) \
	msg_info("%s[%d]: recip %s deliver %s", m, \
                p.level, \
		p.msg_attr.rcpt.address ? p.msg_attr.rcpt.address : "", \
		p.msg_attr.delivered ? p.msg_attr.delivered : "")

 /*
  * "inner" nodes of the delivery graph.
  */
extern int deliver_recipient(LOCAL_STATE, USER_ATTR);

 /*
  * "leaf" nodes of the delivery graph.
  */
extern int deliver_mailbox(LOCAL_STATE, USER_ATTR, int *);
extern int deliver_file(LOCAL_STATE, USER_ATTR, char *);
extern int deliver_maildir(LOCAL_STATE, USER_ATTR);
extern int deliver_unknown(LOCAL_STATE);

 /*
  * Mailbox lock protocol.
  */
extern int virtual_mbox_lock_mask;

 /*
  * Silly little macros.
  */
#define STR(s)	vstring_str(s)

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
