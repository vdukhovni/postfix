/*++
/* NAME
/*	dict_tcp 3
/* SUMMARY
/*	dictionary manager interface to tcp-based lookup tables
/* SYNOPSIS
/*	#include <dict_tcp.h>
/*
/*	DICT	*dict_tcp_open(map, dummy, dict_flags)
/*	const char *map;
/*	int	dummy;
/*	int	dict_flags;
/* DESCRIPTION
/*	dict_tcp_open() makes a TCP server accessible via the generic
/*	dictionary operations described in dict_open(3).
/*	The \fIdummy\fR argument is not used. The only implemented
/*	operation is dictionary lookup.
/*
/*	Map names have the form host:port.
/*
/*	The TCP map class implements a very simple protocol: a query is sent
/*	as one line of text, and a reply is sent back in the same format.
/*	% and non-printable characters are replaced by %xx, xx being the
/*	corresponding hexadecimal value.
/* SEE ALSO
/*	dict(3) generic dictionary manager
/*	hex_quote(3) http-style quoting
/* DIAGNOSTICS
/*	Fatal errors: out of memory, unknown host or service name,
/*	attempt to update or iterate over map.
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

#include "sys_defs.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>

/* Utility library. */

#include "msg.h"
#include "mymalloc.h"
#include "vstring.h"
#include "vstream.h"
#include "vstring_vstream.h"
#include "connect.h"
#include "hex_quote.h"
#include "dict.h"
#include "dict_tcp.h"

/* Application-specific. */

typedef struct {
    DICT    dict;			/* generic members */
    char   *map;			/* server host:port */
    VSTRING *raw_buf;			/* raw I/O buffer */
    VSTRING *hex_buf;			/* quoted I/O buffer */
    VSTREAM *fp;			/* I/O stream */
} DICT_TCP;

#define DICT_TCP_MAXTRY	10
#define DICT_TCP_TMOUT	100

#define STR(x)		vstring_str(x)

/* dict_tcp_connect - connect to TCP server */

static int dict_tcp_connect(DICT_TCP *dict_tcp)
{
    int     fd;

    /*
     * Connect to the server. Enforce a time limit on read/write operations
     * so that we do not get stuck.
     */
    if ((fd = inet_connect(dict_tcp->map, BLOCKING, 0)) < 0) {
	msg_warn("connect to TCP map %s: %m", dict_tcp->map);
	return (-1);
    }
    dict_tcp->fp = vstream_fdopen(fd, O_RDWR);
    vstream_control(dict_tcp->fp,
		    VSTREAM_CTL_TIMEOUT, DICT_TCP_TMOUT,
		    VSTREAM_CTL_END);

    /*
     * Allocate per-map I/O buffers on the fly.
     */
    if (dict_tcp->raw_buf == 0) {
	dict_tcp->raw_buf = vstring_alloc(10);
	dict_tcp->hex_buf = vstring_alloc(10);
    }
    return (0);
}

/* dict_tcp_disconnect - disconnect from TCP server */

static void dict_tcp_disconnect(DICT_TCP *dict_tcp)
{
    (void) vstream_fclose(dict_tcp->fp);
    dict_tcp->fp = 0;
}

/* dict_tcp_lookup - query TCP server */

static const char *dict_tcp_lookup(DICT *dict, const char *key)
{
    DICT_TCP *dict_tcp = (DICT_TCP *) dict;
    int     i;

    dict_errno = 0;

    for (i = 0; /* see below */ ; i++) {

	/*
	 * Try to connect a limited number of times before giving up.
	 */
	if (i >= DICT_TCP_MAXTRY) {
	    dict_errno = DICT_ERR_RETRY;
	    return (0);
	}

	/*
	 * Sleep between attempts, instead of hammering the server.
	 */
	if (i > 0)
	    sleep(1);

	/*
	 * Connect to the server.
	 */
	if (dict_tcp->fp == 0)
	    if (dict_tcp_connect(dict_tcp) < 0)
		continue;

	/*
	 * Send query and receive response. Both are %XX quoted and both are
	 * terminated by newline. This encoding is convenient for data that
	 * is mostly text.
	 */
	hex_quote(dict_tcp->hex_buf, key);
	vstream_fprintf(dict_tcp->fp, "%s\n", STR(dict_tcp->hex_buf));
	errno = 0;
	if (vstring_get_nonl(dict_tcp->hex_buf, dict_tcp->fp) == VSTREAM_EOF) {
	    msg_warn("read TCP map reply from %s: %m", dict_tcp->map);
	} else if (!hex_unquote(dict_tcp->raw_buf, STR(dict_tcp->hex_buf))) {
	    msg_warn("read TCP map reply from %s: malformed reply %.100s",
		     dict_tcp->map, STR(dict_tcp->hex_buf));
	} else {
	    return (STR(dict_tcp->raw_buf));
	}

	/*
	 * That did not work. Clean up and try again.
	 */
	dict_tcp_disconnect(dict_tcp);
    }
}

/* dict_tcp_update - add or update table entry */

static void dict_tcp_update(DICT *dict, const char *unused_name, const char *unused_value)
{
    DICT_TCP *dict_tcp = (DICT_TCP *) dict;

    msg_fatal("dict_tcp_update: attempt to update map %s", dict_tcp->map);
}

/* dict_tcp_delete - remove table entry */

static int dict_tcp_delete(DICT *dict, const char *unused_name)
{
    DICT_TCP *dict_tcp = (DICT_TCP *) dict;

    msg_fatal("dict_tcp_delete: attempt to update map %s", dict_tcp->map);
}

/* dict_tcp_sequence - iterate over table */

static int dict_tcp_sequence(DICT *dict, int unused_func,
		        const char **unused_name, const char **unused_value)
{
    DICT_TCP *dict_tcp = (DICT_TCP *) dict;

    msg_fatal("dict_tcp_sequence: attempt to iterate map %s", dict_tcp->map);
}

/* dict_tcp_close - close TCP map */

static void dict_tcp_close(DICT *dict)
{
    DICT_TCP *dict_tcp = (DICT_TCP *) dict;

    if (dict_tcp->fp)
	(void) vstream_fclose(dict_tcp->fp);
    if (dict_tcp->raw_buf)
	vstring_free(dict_tcp->raw_buf);
    if (dict_tcp->hex_buf)
	vstring_free(dict_tcp->hex_buf);
    myfree(dict_tcp->map);
    myfree((char *) dict_tcp);
}

/* dict_tcp_open - open TCP map */

DICT   *dict_tcp_open(const char *map, int unused_flags, int dict_flags)
{
    DICT_TCP *dict_tcp;

    dict_errno = 0;

    dict_tcp = (DICT_TCP *) mymalloc(sizeof(*dict_tcp));
    dict_tcp->fp = 0;
    dict_tcp->raw_buf = dict_tcp->hex_buf = 0;
    dict_tcp->dict.lookup = dict_tcp_lookup;
    dict_tcp->dict.update = dict_tcp_update;
    dict_tcp->dict.delete = dict_tcp_delete;
    dict_tcp->dict.sequence = dict_tcp_sequence;
    dict_tcp->dict.close = dict_tcp_close;
    dict_tcp->dict.fd = -1;
    dict_tcp->map = mystrdup(map);
    dict_tcp->dict.flags = dict_flags | DICT_FLAG_FIXED;
    return (&dict_tcp->dict);
}
