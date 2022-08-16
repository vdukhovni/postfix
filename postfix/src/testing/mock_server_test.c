 /*
  * Test program to exercise mock_server.c. See PTEST_README for
  * documentation for how this file is structured.
  */

 /*
  * System library.
  */
#include <sys_defs.h>
#include <errno.h>
#include <stdarg.h>

 /*
  * Utility library.
  */
#include <attr.h>
#include <events.h>
#include <msg.h>
#include <vstream.h>
#include <vstring.h>

 /*
  * Global library.
  */
#include <mail_proto.h>

 /*
  * Test library.
  */
#include <make_attr.h>
#include <mock_server.h>
#include <ptest.h>

 /*
  * Generic case structure.
  */
typedef struct PTEST_CASE {
    const char *testname;		/* Human-readable description */
    void    (*action) (PTEST_CTX *t, const struct PTEST_CASE *);
} PTEST_CASE;

 /*
  * Structure to capture a client-server conversation state.
  */
struct session_state {
    VSTRING *resp_buf;			/* request echoed by server */
    int     resp_len;			/* request length from server */
    int     fd;
    VSTREAM *stream;
    int     error;
};

#define REQUEST_READ_EVENT(fd, action, context, timeout) do { \
        if (msg_verbose > 1) \
            msg_info("%s: read-request fd=%d", myname, (fd)); \
        event_enable_read((fd), (action), (context)); \
        event_request_timer((action), (context), (timeout)); \
    } while (0)

#define CLEAR_EVENT_REQUEST(fd, time_act, context) do { \
        if (msg_verbose > 1) \
            msg_info("%s: clear-request fd=%d", myname, (fd)); \
        event_disable_readwrite(fd); \
        event_cancel_timer((time_act), (context)); \
    } while (0)

/* read_event - event handler to receive server response */

static void read_event(int event, void *context)
{
    static const char myname[] = "read_event";
    struct session_state *session_state = (struct session_state *) context;

    CLEAR_EVENT_REQUEST(session_state->fd, read_event, context);

    switch (event) {
    case EVENT_READ:
	if (attr_scan(session_state->stream, ATTR_FLAG_NONE,
		      RECV_ATTR_STR(MAIL_ATTR_REQ, session_state->resp_buf),
		    RECV_ATTR_INT(MAIL_ATTR_SIZE, &session_state->resp_len),
		      ATTR_TYPE_END) != 2) {
	    ptest_error(ptest_ctx_current(), "%s failed: %m", myname);
	    session_state->error = EINVAL;
	}
	break;
    case EVENT_TIME:
	ptest_error(ptest_ctx_current(), "%s: timeout", myname);
	session_state->error = ETIMEDOUT;
	break;
    default:
	ptest_fatal(ptest_ctx_current(), "%s: unknown event: %d",
		    myname, event);
    }
}

static void test_single_server(PTEST_CTX *t, const PTEST_CASE *tp)
{
    static const char myname[] = "test_single_server";
    MOCK_SERVER *mp;
    struct session_state session_state;
    VSTRING *serialized_req;
    VSTRING *serialized_resp;

#define REQUEST_VAL	"abcdef"
#define SERVER_NAME	"testing..."

    /*
     * Instantiate a mock server, and connect to it.
     */
    mp = mock_unix_server_create(SERVER_NAME);
    session_state.resp_buf = vstring_alloc(100);
    session_state.resp_len = 0;
    session_state.error = 0;
    if ((session_state.fd = unix_connect(SERVER_NAME, 0, 0)) < 0)
	ptest_fatal(t, "unix_connect: %s: %m", SERVER_NAME);
    session_state.stream = vstream_fdopen(session_state.fd, O_RDWR);

    /*
     * Set up a server request expectation, and response.
     */
    serialized_req =
	make_attr(ATTR_FLAG_NONE,
		  SEND_ATTR_STR(MAIL_ATTR_REQ, REQUEST_VAL),
		  ATTR_TYPE_END);
    serialized_resp =
	make_attr(ATTR_FLAG_NONE,
		  SEND_ATTR_STR(MAIL_ATTR_REQ, REQUEST_VAL),
		  SEND_ATTR_INT(MAIL_ATTR_SIZE, strlen(REQUEST_VAL)),
		  ATTR_TYPE_END);
    mock_server_interact(mp, serialized_req, serialized_resp);

    /*
     * Send a request, and run the event loop once to notify the server side
     * that the request is pending.
     */
    if (attr_print(session_state.stream, ATTR_FLAG_NONE,
		   SEND_ATTR_STR(MAIL_ATTR_REQ, REQUEST_VAL),
		   ATTR_TYPE_END) != 0
	|| vstream_fflush(session_state.stream) != 0)
	ptest_fatal(t, "send request: %m");
    event_loop(1);

    /*
     * Receive the response, and validate.
     */
    REQUEST_READ_EVENT(session_state.fd, read_event, &session_state, 1);
    event_loop(1);
    if (session_state.error != 0) {
	/* already reported */
    } else if (VSTRING_LEN(session_state.resp_buf) != strlen(REQUEST_VAL)) {
	ptest_error(t, "got resp_buf length %ld, want %ld",
		    (long) VSTRING_LEN(session_state.resp_buf),
		    (long) strlen(REQUEST_VAL));
    } else if (session_state.resp_len != strlen(REQUEST_VAL)) {
	ptest_error(t, "got resp_len %d, want %ld",
		    session_state.resp_len, (long) strlen(REQUEST_VAL));
    } else if (strcmp(vstring_str(session_state.resp_buf), REQUEST_VAL) != 0) {
	ptest_error(t, "got resp_buf '%s', wamt '%s'",
		    vstring_str(session_state.resp_buf), REQUEST_VAL);
    }

    /*
     * Clean up.
     */
    if (vstream_fclose(session_state.stream) != 0)
	ptest_fatal(t, "close stream: %m");
    vstring_free(session_state.resp_buf);
    vstring_free(serialized_req);
    vstring_free(serialized_resp);
    mock_server_free(mp);
}

static void test_request_mismatch(PTEST_CTX *t, const PTEST_CASE *tp)
{
    static const char myname[] = "test_single_server";
    MOCK_SERVER *mp;
    struct session_state session_state;
    VSTRING *serialized_req;
    VSTRING *serialized_resp;

#define REQUEST_VAL	"abcdef"
#define SERVER_NAME	"testing..."

    /*
     * Instantiate a mock server, and connect to it.
     */
    mp = mock_unix_server_create(SERVER_NAME);
    session_state.resp_buf = vstring_alloc(100);
    session_state.resp_len = 0;
    if ((session_state.fd = unix_connect(SERVER_NAME, 0, 0)) < 0)
	ptest_fatal(t, "unix_connect: %s: %m", SERVER_NAME);
    session_state.stream = vstream_fdopen(session_state.fd, O_RDWR);

    /*
     * Set up a server request expectation, and response.
     */
    serialized_req =
	make_attr(ATTR_FLAG_NONE,
		  SEND_ATTR_STR(MAIL_ATTR_REQ, REQUEST_VAL "g"),
		  ATTR_TYPE_END);
    serialized_resp =
	make_attr(ATTR_FLAG_NONE,
		  SEND_ATTR_STR(MAIL_ATTR_REQ, REQUEST_VAL),
		  SEND_ATTR_INT(MAIL_ATTR_SIZE, strlen(REQUEST_VAL)),
		  ATTR_TYPE_END);
    mock_server_interact(mp, serialized_req, serialized_resp);

    /*
     * Send a request, and run the event loop once to notify the server side
     * that the request is pending.
     */
    if (attr_print(session_state.stream, ATTR_FLAG_NONE,
		   SEND_ATTR_STR(MAIL_ATTR_REQ, REQUEST_VAL),
		   ATTR_TYPE_END) != 0
	|| vstream_fflush(session_state.stream) != 0)
	ptest_fatal(t, "send request: %m");
    expect_ptest_error(t, "attributes differ");
    expect_ptest_error(t, "+request = abcdef");
    expect_ptest_error(t, "-request = abcdefg");
    expect_ptest_error(t, "timeout");
    event_loop(1);

    /*
     * Receive the response, and validate.
     */
    REQUEST_READ_EVENT(session_state.fd, read_event, &session_state, 1);
    event_loop(1);
    if (session_state.error != 0) {
        /* already reported */
    } else if (VSTRING_LEN(session_state.resp_buf) != strlen(REQUEST_VAL)) {
        ptest_error(t, "got resp_buf length %ld, want %ld",
                    (long) VSTRING_LEN(session_state.resp_buf),
                    (long) strlen(REQUEST_VAL));
    } else if (session_state.resp_len != strlen(REQUEST_VAL)) {
        ptest_error(t, "got resp_len %d, want %ld",
                    session_state.resp_len, (long) strlen(REQUEST_VAL));
    } else if (strcmp(vstring_str(session_state.resp_buf), REQUEST_VAL) != 0) {
        ptest_error(t, "got resp_buf '%s', wamt '%s'",
                    vstring_str(session_state.resp_buf), REQUEST_VAL);
    }

    /*
     * Clean up.
     */
    if (vstream_fclose(session_state.stream) != 0)
	ptest_fatal(t, "close stream: %m");
    vstring_free(session_state.resp_buf);
    vstring_free(serialized_req);
    vstring_free(serialized_resp);
    mock_server_free(mp);
}

static void test_missing_server(PTEST_CTX *t, const PTEST_CASE *tp)
{
    int     fd;

#define SERVER_NAME "testing..."

    /*
     * Connect to a non-existent server, and require a failure.
     */
    if ((fd = unix_connect(SERVER_NAME, 0, 0)) >= 0) {
	(void) close(fd);
	ptest_fatal(t,
		    "unix_connect(%s) did NOT fail", SERVER_NAME);
    }
}

static void test_unused_server(PTEST_CTX *t, const PTEST_CASE *tp)
{
    MOCK_SERVER *mp;

    /*
     * Instantiate a mock server, and destroy it without using it.
     */
    mp = mock_unix_server_create(SERVER_NAME);
    mock_server_free(mp);
}

static void test_server_speaks_only(PTEST_CTX *t, const PTEST_CASE *tp)
{
    static const char myname[] = "test_server_speaks_only";
    MOCK_SERVER *mp;
    struct session_state session_state;
    VSTRING *serialized_resp;

    /*
     * This is the same test as "test_single_server", but without sending a
     * request.
     */
#define REQUEST_VAL	"abcdef"
#define SERVER_NAME	"testing..."
#define NO_REQUEST	((VSTRING *) 0)

    /*
     * Instantiate a mock server, and connect to it.
     */
    mp = mock_unix_server_create(SERVER_NAME);
    session_state.resp_buf = vstring_alloc(100);
    session_state.resp_len = 0;
    if ((session_state.fd = unix_connect(SERVER_NAME, 0, 0)) < 0)
	ptest_fatal(t, "unix_connect: %s: %m", SERVER_NAME);
    session_state.stream = vstream_fdopen(session_state.fd, O_RDWR);

    /*
     * Set up a server response, without request expectation.
     */
    serialized_resp =
	make_attr(ATTR_FLAG_NONE,
		  SEND_ATTR_STR(MAIL_ATTR_REQ, REQUEST_VAL),
		  SEND_ATTR_INT(MAIL_ATTR_SIZE, strlen(REQUEST_VAL)),
		  ATTR_TYPE_END);
    mock_server_interact(mp, NO_REQUEST, serialized_resp);

    /*
     * Receive the response, and validate.
     */
    REQUEST_READ_EVENT(session_state.fd, read_event, &session_state, 1);
    event_loop(1);
    if (session_state.error != 0) {
        /* already reported */
    } else if (VSTRING_LEN(session_state.resp_buf) != strlen(REQUEST_VAL)) {
        ptest_error(t, "got resp_buf length %ld, want %ld",
                    (long) VSTRING_LEN(session_state.resp_buf),
                    (long) strlen(REQUEST_VAL));
    } else if (session_state.resp_len != strlen(REQUEST_VAL)) {
        ptest_error(t, "got resp_len %d, want %ld",
                    session_state.resp_len, (long) strlen(REQUEST_VAL));
    } else if (strcmp(vstring_str(session_state.resp_buf), REQUEST_VAL) != 0) {
        ptest_error(t, "got resp_buf '%s', wamt '%s'",
                    vstring_str(session_state.resp_buf), REQUEST_VAL);
    }

    /*
     * Clean up.
     */
    if (vstream_fclose(session_state.stream) != 0)
	ptest_fatal(t, "close stream: %m");
    vstring_free(session_state.resp_buf);
    vstring_free(serialized_resp);
    mock_server_free(mp);
}

static void test_client_speaks_only(PTEST_CTX *t, const PTEST_CASE *tp)
{
    MOCK_SERVER *mp;
    struct session_state session_state;
    VSTRING *serialized_req;

    /*
     * This is the same test as "test_single_server", but without receiving a
     * response.
     */
#define REQUEST_VAL	"abcdef"
#define SERVER_NAME	"testing..."
#define NO_RESPONSE	((VSTRING *) 0)

    /*
     * Instantiate a mock server, and connect to it.
     */
    mp = mock_unix_server_create(SERVER_NAME);
    if ((session_state.fd = unix_connect(SERVER_NAME, 0, 0)) < 0)
	ptest_fatal(t, "unix_connect: %s: %m", SERVER_NAME);
    session_state.stream = vstream_fdopen(session_state.fd, O_RDWR);

    /*
     * Set up a server request expectation, and response.
     */
    serialized_req =
	make_attr(ATTR_FLAG_NONE,
		  SEND_ATTR_STR(MAIL_ATTR_REQ, REQUEST_VAL),
		  ATTR_TYPE_END);
    mock_server_interact(mp, serialized_req, NO_RESPONSE);

    /*
     * Send a request, and run the event loop once to notify the server side
     * that the request is pending.
     */
    if (attr_print(session_state.stream, ATTR_FLAG_NONE,
		   SEND_ATTR_STR(MAIL_ATTR_REQ, REQUEST_VAL),
		   ATTR_TYPE_END) != 0
	|| vstream_fflush(session_state.stream) != 0)
	ptest_fatal(t, "send request: %m");
    event_loop(1);

    /*
     * Clean up.
     */
    if (vstream_fclose(session_state.stream) != 0)
	ptest_fatal(t, "close stream: %m");
    vstring_free(serialized_req);
    mock_server_free(mp);
}

 /*
  * Test cases.
  */
const PTEST_CASE ptestcases[] = {
    {
	"test single server", test_single_server,
    },
    {
	"test request mismatch", test_request_mismatch,
    },
    {
	"test missing server", test_missing_server,
    },
    {
	"test unused server", test_unused_server,
    },
    {
	"test server speaks only", test_server_speaks_only,
    },
    {
	"test client speaks only", test_client_speaks_only,
    },

    /*
     * TODO: test multiple servers with the same endpoint name but with
     * different expectations. See postscreen_dnsbl_test.c for an example.
     * This requires that the environment variable "NORAMDOMIZE" is set
     * before this program is run.
     */
};

#include <ptest_main.h>
