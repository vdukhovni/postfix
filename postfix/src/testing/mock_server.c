/*++
/* NAME
/*	mock_server 3
/* SUMMARY
/*	Mock server for hermetic tests
/* SYNOPSIS
/*	#include <mock_server.h>
/*
/*	MOCK_SERVER *mock_unix_server_create(
/*	const char *destination)
/*
/*	void	mock_server_interact(
/*	MOCK_SERVER *server,
/*	const VSTRING *request,
/*	const VSTRING *response)
/*
/*	void	mock_server_free(MOCK_SERVER *server)
/*
/*	int	unix_connect(
/*	const char *destination,
/*	int	block_mode,
/*	int	timeout)
/* AUXILIARY FUNCTIONS
/*	void	mock_server_free_void_ptr(void *ptr)
/* DESCRIPTION
/*	The purpose of this code is to make tests hermetic, i.e.
/*	independent from a real server.
/*
/*	This module overrides the client function unix_connect()
/*	with a function that connects to a mock server instance.
/*	The mock server must be instantiated in advance with
/*	mock_unix_server_create(). The connection destination name
/*	is not associated with out-of-process resources.
/*
/*	mock_unix_server_create() creates a mock in-process server
/*	that will "accept" one unix_connect() request with the
/*	specified destination. To accept multiple connections, use
/*	multiple mock_unix_server_create() calls.
/*
/*	mock_server_interact() sets up one expected request and/or
/*	prepared response. Specify a null request to configure an
/*	unconditional server response such as an initial handshake,
/*	and specify a null response to specify a final request. If
/*	an expected request is configured, the client should send
/*	a request and call event_loop() once to deliver the request
/*	to the mock server. If a prepared response is configured,
/*	the mock server will respond immediately, and the client
/*	should call event_loop() once to receive the response from
/*	the server. To simulate multiple request/response interactions,
/*	use a sequence of mock_server_interact() calls.
/*
/*	mock_server_free() destroys the specified server instance.
/*	mock_server_free_void_ptr() provides an alternative API to
/*	allow for systems where (struct *) and (void *) differ.
/* BUGS
/*	Each connection supports no more than one request and one
/*	response at a time; each request and each response must fit
/*	in a VSTREAM buffer (4096 bytes as of Postfix version
/*	3.8), and must not be larger than SO_SNDBUF for AF_LOCAL
/*	stream sockets (8192 bytes or more on contemporaneous
/*	systems).
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	Google, Inc.
/*	111 8th Avenue
/*	New York, NY 10011, USA
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>

 /*
  * Utility library.
  */
#include <events.h>
#include <msg.h>
#include <mymalloc.h>
#include <vstring.h>

 /*
  * Test libraries
  */
#include <match_attr.h>
#include <mock_server.h>
#include <ptest.h>

 /*
  * Macros to make obscure code more readable.
  */
#define COPY_OR_NULL(dst, src) do { \
	if ((src) != 0) { \
	    if ((dst) == 0) \
		(dst) = vstring_alloc(LEN(src)); \
	    vstring_memcpy((dst), STR(src), LEN(src)); \
	} else if ((dst) != 0) { \
	    vstring_free(dst); \
	    (dst) = 0; \
	} \
    } while (0)

#define MOCK_SERVER_REQUEST_READ_EVENT(fd, action, context, timeout) do { \
        if (msg_verbose > 1) \
            msg_info("%s: read-request fd=%d", myname, (fd)); \
        event_enable_read((fd), (action), (context)); \
        event_request_timer((action), (context), (timeout)); \
    } while (0)

#define MOCK_SERVER_CLEAR_EVENT_REQUEST(fd, time_act, context) do { \
        if (msg_verbose > 1) \
            msg_info("%s: clear-request fd=%d", myname, (fd)); \
        event_disable_readwrite(fd); \
        event_cancel_timer((time_act), (context)); \
    } while (0)


#define MOCK_SERVER_TIMEOUT	10

#define MOCK_SERVER_SIDE	(0)
#define MOCK_CLIENT_SIDE	(1)

 /*
  * Other macros.
  */
#define STR	vstring_str
#define LEN	VSTRING_LEN

 /*
  * List head. We could use a RING object and save a few bits in data space,
  * at the cost of more complex code.
  */
static MOCK_SERVER mock_unix_server_head;

/* mock_unix_server_create - instantiate an unconnected mock server */

MOCK_SERVER *mock_unix_server_create(const char *dest)
{
    MOCK_SERVER *mp;

    mp = (MOCK_SERVER *) mymalloc(sizeof(*mp));
    if (socketpair(AF_LOCAL, SOCK_STREAM, 0, mp->fds) < 0) {
	myfree(mp);
	ptest_error(ptest_ctx_current(), "mock_unix_server_create(%s): "
		    "socketpair(AF_LOCAL, SOCK_STREAM, 0, fds): %m", dest);
	return (0);
    }
    mp->flags = 0;
    mp->want_dest = mystrdup(dest);
    mp->want_req = 0;
    mp->resp = 0;
    mp->iobuf = 0;

    /*
     * Link the mock server into the waiting list.
     */
    mp->next = mock_unix_server_head.next;
    mock_unix_server_head.next = mp;
    mp->prev = &mock_unix_server_head;
    if (mp->next)
	mp->next->prev = mp;
    return (mp);
}

/* mock_unix_server_respond - send a prepared response */

static void mock_unix_server_respond(MOCK_SERVER *mp)
{
    const char myname[] = "mock_unix_server_respond";
    ssize_t got_len;

    got_len = write(mp->fds[MOCK_SERVER_SIDE], STR(mp->resp), LEN(mp->resp));
    if (got_len < 0)
	ptest_fatal(ptest_ctx_current(), "%s: write: %m", myname);
    if (got_len != LEN(mp->resp))
	ptest_fatal(ptest_ctx_current(), "%s: wrote %ld of %ld bytes",
		    myname, (long) got_len, (long) LEN(mp->resp));
}

/* mock_unix_server_read_event - receive request and respond */

static void mock_unix_server_read_event(int event, void *context)
{
    const char myname[] = "mock_unix_server_read_event";
    MOCK_SERVER *mp = (MOCK_SERVER *) context;
    ssize_t peek_len;
    ssize_t got_len;

    /*
     * Disarm this file descriptor.
     */
    MOCK_SERVER_CLEAR_EVENT_REQUEST(mp->fds[MOCK_SERVER_SIDE],
				    mock_unix_server_read_event,
				    context);

    /*
     * Handle the event.
     */
    switch (event) {
    case EVENT_READ:
	break;
    case EVENT_TIME:
	ptest_error(ptest_ctx_current(), "%s: timeout", myname);
	return;
    default:
	ptest_fatal(ptest_ctx_current(), "%s: unexpected event: %d",
		    myname, event);
    }

    /*
     * Receive the request.
     */
    switch (peek_len = peekfd(mp->fds[MOCK_SERVER_SIDE])) {
    case -1:
	ptest_error(ptest_ctx_current(), "%s: read: %m", myname);
	return;
    case 0:
	ptest_error(ptest_ctx_current(), "%s: read EOF", myname);
	return;
    default:
	break;
    }
    if (mp->iobuf == 0)
	mp->iobuf = vstring_alloc(1000);
    VSTRING_SPACE(mp->iobuf, peek_len);
    got_len = read(mp->fds[MOCK_SERVER_SIDE], STR(mp->iobuf), peek_len);
    if (got_len != peek_len) {
	ptest_fatal(ptest_ctx_current(), "%s: read %ld of %ld bytes",
		    myname, (long) got_len, (long) peek_len);
	return;
    }
    vstring_set_payload_size(mp->iobuf, got_len);
    if (!eq_attr(ptest_ctx_current(), "request", mp->iobuf, mp->want_req))
	return;

    /*
     * Send the response, if available.
     */
    else if (mp->resp)
	mock_unix_server_respond(mp);
}

/* mock_server_interact - set up request and/or response */

void    mock_server_interact(MOCK_SERVER *mp,
				          const VSTRING *req,
				          const VSTRING *resp)
{
    const char myname[] = "mock_server_interact";

    if (req == 0 && resp == 0)
	ptest_fatal(ptest_ctx_current(), "%s: null request and null response",
		    myname);
    COPY_OR_NULL(mp->want_req, req);
    COPY_OR_NULL(mp->resp, resp);
    if (req != 0) {
	MOCK_SERVER_REQUEST_READ_EVENT(mp->fds[MOCK_SERVER_SIDE],
				       mock_unix_server_read_event,
				       (void *) mp, MOCK_SERVER_TIMEOUT);
    } else {
	mock_unix_server_respond(mp);
    }
}

/* mock_unix_server_unlink - detach one instance from the waiting list */

static void mock_unix_server_unlink(MOCK_SERVER *mp)
{
    if (mp->next)
	mp->next->prev = mp->prev;
    if (mp->prev)
	mp->prev->next = mp->next;
    mp->prev = mp->next = 0;
}

/* mock_server_free - destroy mock unix-domain server */

void    mock_server_free(MOCK_SERVER *mp)
{
    const char myname[] = "mock_server_free";

    myfree(mp->want_dest);
    if ((mp->flags & MOCK_SERVER_FLAG_CONNECTED) == 0)
	(void) close(mp->fds[MOCK_CLIENT_SIDE]);
    MOCK_SERVER_CLEAR_EVENT_REQUEST(mp->fds[MOCK_SERVER_SIDE],
				    mock_unix_server_read_event, mp);
    (void) close(mp->fds[MOCK_SERVER_SIDE]);
    if (mp->want_req)
	vstring_free(mp->want_req);
    if (mp->resp)
	vstring_free(mp->resp);
    if (mp->iobuf)
	vstring_free(mp->iobuf);
    mock_unix_server_unlink(mp);
    myfree(mp);
}

/* mock_server_free_void_ptr - destroy mock unix-domain server */

void    mock_server_free_void_ptr(void *ptr)
{
    mock_server_free(ptr);
}

/* unix_connect - mock helper */

int     unix_connect(const char *dest, int block_mode, int unused_timeout)
{
    MOCK_SERVER *mp;

    for (mp = mock_unix_server_head.next; /* see below */ ; mp = mp->next) {
	if (mp == 0) {
	    errno = ENOENT;
	    return (-1);
	}
	if (strcmp(dest, mp->want_dest) == 0) {
	    mock_unix_server_unlink(mp);
	    if (block_mode == NON_BLOCKING)
		non_blocking(mp->fds[MOCK_CLIENT_SIDE], block_mode);
	    mp->flags |= MOCK_SERVER_FLAG_CONNECTED;
	    return (mp->fds[MOCK_CLIENT_SIDE]);
	}
    }
}
