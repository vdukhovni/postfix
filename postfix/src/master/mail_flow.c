/*++
/* NAME
/*	mail_flow 3
/* SUMMARY
/*	global mail flow control
/* SYNOPSIS
/*	#include <mail_flow.h>
/*
/*	int	mail_flow_get(count)
/*	int	count;
/*
/*	void	mail_flow_put(count)
/*	int	count;
/* DESCRIPTION
/*	This module implements a simple flow control mechanism that
/*	is based on tokens that are consumed by mail receiving processes
/*	and that are produced by mail sending processes.
/*
/*	mail_flow_get() attempts to read specified number of tokens. The
/*	result is > 0 for success, < 0 for failure. In the latter case,
/*	the process is expected to slow down a little.
/*
/*	mail_flow_put() produces the specified number of tokens. The
/*	token producing process is expected to produce new tokens
/*	whenever it falls idle and no more tokens are available.
/* BUGS
/*	The producer needs to wake up periodically to ensure that
/*	tokens are not lost due to leakage.
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

/* Utility library. */

#include <msg.h>

/* Global library. */

#include <mail_flow.h>

/* Master library. */

#include <master_proto.h>

#define BUFFER_SIZE	1024

/* mail_flow_get - read N tokens */

int     mail_flow_get(int len)
{
    char   *myname = "mail_flow_get";
    char    buf[BUFFER_SIZE];
    int     count;
    int     n = 0;

    /*
     * Sanity check.
     */
    if (len <= 0)
	msg_panic("%s: bad length %d", myname, len);

    /*
     * Read and discard N bytes.
     */
    for (count = len; count > 0; count -= n)
	if ((n = read(MASTER_FLOW_READ, buf, count > BUFFER_SIZE ?
		      BUFFER_SIZE : count)) < 0)
	    return (-1);
    if (msg_verbose)
	msg_info("%s: %d %d", myname, len, len - count);
    return (len - count);
}

/* mail_flow_put - put N tokens */

int     mail_flow_put(int len)
{
    char   *myname = "mail_flow_put";
    char    buf[BUFFER_SIZE];
    int     count;
    int     n = 0;

    /*
     * Sanity check.
     */
    if (len <= 0)
	msg_panic("%s: bad length %d", myname, len);

    /*
     * Write or discard N bytes.
     */
    for (count = len; count > 0; count -= n)
	if ((n = write(MASTER_FLOW_WRITE, buf, count > BUFFER_SIZE ?
		      BUFFER_SIZE : count)) < 0)
	    return (-1);
    if (msg_verbose)
	msg_info("%s: %d %d", myname, len, len - count);
    return (len - count);
}
