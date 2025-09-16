/*++
/* NAME
/*	deliver_attr 3
/* SUMMARY
/*	initialize message delivery attributes
/* SYNOPSIS
/*	#include "virtual.h"
/*
/*	void	deliver_attr_init(attrp)
/*	DELIVER_ATTR *attrp;
/*
/*	void	deliver_attr_dump(attrp)
/*	DELIVER_ATTR *attrp;
/*
/*	void	deliver_attr_free(attrp)
/*	DELIVER_ATTR *attrp;
/* DESCRIPTION
/*	deliver_attr_init() initializes a structure with message delivery
/*	attributes to a known initial state (all zeros).
/*
/*	deliver_attr_dump() logs the contents of the given attribute list.
/*
/*	deliver_attr_free() releases memory that was allocated by
/*	deliver_attr_init().
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

/* Utility library. */

#include <msg.h>
#include <vstream.h>

/* Application-specific. */

#include "virtual.h"

/* deliver_attr_init - set message delivery attributes to all-zero state */

void    deliver_attr_init(DELIVER_ATTR *attrp)
{
    attrp->level = 0;
    attrp->fp = 0;
    attrp->queue_name = 0;
    attrp->queue_id = 0;
    attrp->offset = 0;
    attrp->sender = 0;
    RECIPIENT_ASSIGN(&(attrp->rcpt), 0, 0, 0, 0, 0);
    attrp->user = 0;
    attrp->delivered = 0;
    attrp->relay = 0;
    attrp->why = dsb_create();
}

/* deliver_attr_dump - log message delivery attributes */

void    deliver_attr_dump(DELIVER_ATTR *attrp)
{
    msg_info("level: %d", attrp->level);
    msg_info("path: %s", VSTREAM_PATH(attrp->fp));
    msg_info("fp: 0x%lx", (long) attrp->fp);
    msg_info("queue_name: %s", attrp->queue_name ? attrp->queue_name : "null");
    msg_info("queue_id: %s", attrp->queue_id ? attrp->queue_id : "null");
    msg_info("offset: %ld", attrp->offset);
    msg_info("sender: %s", attrp->sender ? attrp->sender : "null");
    msg_info("recipient: %s", attrp->rcpt.address ? attrp->rcpt.address : "null");
    msg_info("user: %s", attrp->user ? attrp->user : "null");
    msg_info("delivered: %s", attrp->delivered ? attrp->delivered : "null");
    msg_info("relay: %s", attrp->relay ? attrp->relay : "null");
    msg_info("why: %s", attrp->why ? "buffer" : "null");
}


/* deliver_attr_free - release storage */

void    deliver_attr_free(DELIVER_ATTR *attrp)
{
    dsb_free(attrp->why);
}
