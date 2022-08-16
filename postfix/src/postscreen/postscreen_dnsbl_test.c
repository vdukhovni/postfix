 /*
  * Test program to exercise postscreen_dnsbl.c. See comments in
  * mock_server.c, and PTEST_README for documented examples of unit tests.
  */

 /*
  * System library.
  */
#include <sys_defs.h>
#include <limits.h>

 /*
  * Utility library.
  */
#include <attr.h>
#include <events.h>
#include <vstream.h>
#include <vstring.h>

 /*
  * Global library.
  */
#include <mail_proto.h>
#include <mail_params.h>

 /*
  * Test library.
  */
#include <ptest.h>
#include <make_attr.h>
#include <mock_server.h>

 /*
  * Application-specific.
  */
#include <postscreen.h>

 /*
  * Generic case structure.
  */
typedef struct PTEST_CASE {
    const char *testname;		/* Human-readable description */
    void    (*action) (PTEST_CTX *t, const struct PTEST_CASE *);
} PTEST_CASE;

 /*
  * Structure to capture postscreen_dnsbl_retrieve() inputs and outputs.
  */
struct session_state {
    /* postscreen_dnsbl_retrieve() inputs. */
    const char *req_addr;		/* Client IP address */
    int     req_idx;			/* Request index */
    /* postscreen_dnsbl_retrieve()  outputs. */
    const char *got_dnsbl;		/* Null, or biggest contributor */
    int     got_ttl;			/* TTL from A or SOA record */
    int     got_score;			/* Combined score */
};

 /*
  * Surrogates for global variables used, but not defined, by
  * postscreen_dnsbl.c.
  */
int     var_psc_dnsbl_min_ttl;		/* postscreen_dnsbl_min_ttl */
int     var_psc_dnsbl_max_ttl;		/* postscreen_dnsbl_max_ttl */
int     var_psc_dnsbl_tmout;		/* postscreen_dnsbl_timeout */
char   *var_psc_dnsbl_sites;		/* postscreen_dnsbl_sites */
char   *var_dnsblog_service;		/* dnsblog_service_name */
DICT   *psc_dnsbl_reply;		/* postscreen_dnsbl_reply_map */

/* deinit_psc_globals - best effort reset */

static void deinit_psc_globals(void)
{

    /*
     * deinit_psc_globals() must be idempotent, so that it can be called
     * safely at the start and end of each test.
     */
    if (var_psc_dnsbl_sites) {
	myfree(var_psc_dnsbl_sites);
	var_psc_dnsbl_sites = 0;
    }
    if (psc_dnsbl_reply) {
	dict_close(psc_dnsbl_reply);
	psc_dnsbl_reply = 0;
    }

    /*
     * Reset postscreen_dnsbl.c internals.
     */
    psc_dnsbl_deinit();
}

/* init_psc_globals - initialize globals */

static void init_psc_globals(const char *dnsbl_sites)
{

    /*
     * We call deinit_psc_globals() first, because it may not be called at
     * the end of a failed test. A test failure should not affect later
     * tests.
     */
    deinit_psc_globals();

    /*
     * Set parameters that postscreen_dnsbl.c depends on.
     */
    var_psc_dnsbl_min_ttl = 60;
    var_psc_dnsbl_max_ttl = 3600;
    var_psc_dnsbl_tmout = atoi(DEF_PSC_DNSBL_TMOUT);
    var_psc_dnsbl_sites = mystrdup(dnsbl_sites);
    var_dnsblog_service = DEF_DNSBLOG_SERVICE;

    /*
     * postscreen_dnsbl.c mandatory initialization.
     */
    psc_dnsbl_init();
}

/* psc_dnsbl_callback - event handler to retrieve score and ttl */

static void psc_dnsbl_callback(int event, void *context)
{
    struct session_state *sp = (struct session_state *) context;

    sp->got_score = psc_dnsbl_retrieve(sp->req_addr, &sp->got_dnsbl,
				       sp->req_idx, &sp->got_ttl);
}

 /*
  * Test data and tests for a single reputation provider.
  */
struct single_dnsbl_data {
    const char *label;			/* test label */
    const char *dnsbl_sites;		/* postscreen_dnsbl_sites */
    const char *req_dnsbl;		/* in dnsblog request */
    const char *req_addr;		/* in dnsblog request */
    const char *res_addr;		/* in dnsblog response */
    int     res_ttl;			/* in dnsblog response */
    int     want_score;			/* sum of weights */
};

static const struct single_dnsbl_data single_dnsbl_tests[] = {
    {
	"single site listed address",
	 /* dnsbl_sites */ "zen.spamhaus.org",
	 /* req_dnsbl */ "zen.spamhaus.org",
	 /* req_addr */ "127.0.0.2",
	 /* res_addr */ "127.0.0.2 127.0.0.4 127.0.0.10",
	 /* res_ttl */ 60,
	 /* want_score */ 1,
    },
    {
	"repeated site 1x rpc 2x score",
	 /* dnsbl_sites */ "zen.spamhaus.org, zen.spamhaus.org",
	 /* req_dnsbl */ "zen.spamhaus.org",
	 /* req_addr */ "127.0.0.2",
	 /* res_addr */ "127.0.0.2 127.0.0.4 127.0.0.10",
	 /* res_ttl */ 60,
	 /* want_score */ 2,
    },
    {
	"unlisted address zero score",
	 /* dnsbl_sites */ "zen.spamhaus.org",
	 /* req_dnsbl */ "zen.spamhaus.org",
	 /* req_addr */ "127.0.0.1",
	 /* res_addr */ "",
	 /* res_ttl */ 60,
	 /* want_score */ 0,
    },
    {
	"site with weight first",
	 /* dnsbl_sites */ "zen.spamhaus.org*3, zen.spamhaus.org",
	 /* req_dnsbl */ "zen.spamhaus.org",
	 /* req_addr */ "127.0.0.2",
	 /* res_addr */ "127.0.0.2 127.0.0.4 127.0.0.10",
	 /* res_ttl */ 60,
	 /* want_score */ 4,
    },
    {
	"site with weight last",
	 /* dnsbl_sites */ "zen.spamhaus.org, zen.spamhaus.org*3",
	 /* req_dnsbl */ "zen.spamhaus.org",
	 /* req_addr */ "127.0.0.2",
	 /* res_addr */ "127.0.0.2 127.0.0.4 127.0.0.10",
	 /* res_ttl */ 60,
	 /* want_score */ 4,
    },
    {
	"site with filter+weight first",
	 /* dnsbl_sites */ "zen.spamhaus.org=127.0.0.10*3, zen.spamhaus.org",
	 /* req_dnsbl */ "zen.spamhaus.org",
	 /* req_addr */ "127.0.0.2",
	 /* res_addr */ "127.0.0.2 127.0.0.4 127.0.0.10",
	 /* res_ttl */ 60,
	 /* want_score */ 4,
    },
    {
	"site with filter+weight last",
	 /* dnsbl_sites */ "zen.spamhaus.org, zen.spamhaus.org=127.0.0.10*3",
	 /* req_dnsbl */ "zen.spamhaus.org",
	 /* req_addr */ "127.0.0.2",
	 /* res_addr */ "127.0.0.2 127.0.0.4 127.0.0.10",
	 /* res_ttl */ 60,
	 /* want_score */ 4,
    },
    {
	"filter+weight add and subtract",
	 /* dnsbl_sites */ "zen.spamhaus.org=127.0.0.[1..255]*3, zen.spamhaus.org=127.0.0.3*-1",
	 /* req_dnsbl */ "zen.spamhaus.org",
	 /* req_addr */ "10.2.3.4",
	 /* res_addr */ "127.0.0.3 127.0.0.10",
	 /* res_ttl */ 60,
	 /* want_score */ 2,
    },
    {
	"filter+weight add and not subtract",
	 /* dnsbl_sites */ "zen.spamhaus.org=127.0.0.[1..255]*3, zen.spamhaus.org=127.0.0.3*-1",
	 /* req_dnsbl */ "zen.spamhaus.org",
	 /* req_addr */ "10.2.3.4",
	 /* res_addr */ "127.0.0.10",
	 /* res_ttl */ 60,
	 /* want_score */ 3,
    },
};

static void test_single_dnsbl(PTEST_CTX *t, const PTEST_CASE *tp)
{
    MOCK_SERVER *mp;
    struct session_state session_state;
    const char *dnsblog_path = "private/dnsblog";
    VSTRING *serialized_req;
    VSTRING *serialized_resp;
    const int request_id = 0;
    const struct single_dnsbl_data *tt;

    for (tt = single_dnsbl_tests; tt < single_dnsbl_tests
	 + PTEST_NROF(single_dnsbl_tests); tt++) {
	if (tt->label == 0)
	    ptest_fatal(t, "Null test label in single_dnsbl_tests array!");
	PTEST_RUN(t, tt->label, {

	    /*
	     * Reset global state and parameters used by postscreen_dnsbl.c.
	     */
	    init_psc_globals(tt->dnsbl_sites);

	    /*
	     * Instantiate a mock server.
	     */
	    mp = mock_unix_server_create(dnsblog_path);

	    /*
	     * Set up the expected dnsblog request, and the corresponding
	     * response. The mock dnsblog server immediately generates a read
	     * event request, so we should send something soon.
	     */
	    serialized_req =
		make_attr(ATTR_FLAG_NONE,
			  SEND_ATTR_STR(MAIL_ATTR_RBL_DOMAIN, tt->req_dnsbl),
			  SEND_ATTR_STR(MAIL_ATTR_ACT_CLIENT_ADDR,
					tt->req_addr),
			  SEND_ATTR_INT(MAIL_ATTR_LABEL, request_id),
			  ATTR_TYPE_END);
	    serialized_resp =
		make_attr(ATTR_FLAG_NONE,
			  SEND_ATTR_STR(MAIL_ATTR_RBL_DOMAIN, tt->req_dnsbl),
			  SEND_ATTR_STR(MAIL_ATTR_ACT_CLIENT_ADDR,
					tt->req_addr),
			  SEND_ATTR_INT(MAIL_ATTR_LABEL, request_id),
			  SEND_ATTR_STR(MAIL_ATTR_RBL_ADDR, tt->res_addr),
			  SEND_ATTR_INT(MAIL_ATTR_TTL, tt->res_ttl),
			  ATTR_TYPE_END);
	    mock_server_interact(mp, serialized_req, serialized_resp);

	    /*
	     * Send a request by calling psc_dnsbl_request(), and run the
	     * event loop once to notify the mock dnsblog server that a
	     * request is pending. The mock dnsblog server will receive the
	     * request, and if it matches the expected request, the mock
	     * dnsblog server will immediately send the prepared response.
	     */
	    session_state.req_addr = tt->req_addr;
	    session_state.got_dnsbl = 0;
	    session_state.got_ttl = INT_MAX;
	    session_state.got_score = INT_MAX;
	    session_state.req_idx = psc_dnsbl_request(tt->req_addr,
						      psc_dnsbl_callback,
						      &session_state);
	    event_loop(2);

	    /*
	     * Run the event loop another time to wake up
	     * psc_dnsbl_receive(). That function will deserialize the mock
	     * dnsblog server's response, and will immediately call our
	     * psc_dnsbl_callback() function to store the result into the
	     * session_state object.
	     */
	    event_loop(2);

	    /*
	     * Validate the response.
	     */
	    if (session_state.got_ttl == INT_MAX) {
		ptest_error(t, "psc_dnsbl_callback() was not called, "
			    "or did not update the session_state");
	    } else {
		if (session_state.got_ttl != tt->res_ttl)
		    ptest_error(t, "unexpected ttl: got %d, want %d",
				session_state.got_ttl, tt->res_ttl);
		if (session_state.got_score != tt->want_score)
		    ptest_error(t, "unexpected score: got %d, want %d",
				session_state.got_score, tt->want_score);
	    }

	    /*
	     * Clean up.
	     */
	    vstring_free(serialized_req);
	    vstring_free(serialized_resp);
	    mock_server_free(mp);
	    deinit_psc_globals();
	});
    }
}

 /*
  * Test data and tests for multiple reputation providers.
  */
struct dnsbl_data {
    const char *req_dnsbl;		/* in dnsblog request */
    const char *res_addr;		/* in dnsblog response */
    int     res_ttl;			/* in dnsblog response */
};

#define MAX_DNSBL_SITES	3

struct multi_dnsbl_data {
    const char *label;			/* test label */
    const char *dnsbl_sites;		/* postscreen_dnsbl_sites */
    const char *req_addr;		/* in dnsblog request */
    struct dnsbl_data dnsbl_data[MAX_DNSBL_SITES];
    int	    want_ttl;			/* effective TTL */
    int     want_score;			/* sum of weights */
};

static const struct multi_dnsbl_data multi_dnsbl_tests[] = {
    {
	"dual dnsbl, listed by both",
	 /* dnsbl_sites */ "zen.spamhaus.org, foo.example.org",
	 /* req_addr */ "10.2.3.4", {
	    {
		 /* req_dnsbl */ "foo.example.org",
		 /* res_addr */ "127.0.0.10",
		 /* res_ttl */ 60,
	    },
	    {
		 /* req_dnsbl */ "zen.spamhaus.org",
		 /* res_addr */ "127.0.0.10",
		 /* res_ttl */ 60,
	    },
	},
	 /* want_ttl */ 60,
	 /* want_score */ 2,
    }, {
	"dual dnsbl, listed by first",
	 /* dnsbl_sites */ "zen.spamhaus.org, foo.example.org",
	 /* req_addr */ "10.2.3.4", {
	    {
		 /* req_dnsbl */ "foo.example.org",
		 /* res_addr */ "",
		 /* res_ttl */ 62,
	    },
	    {
		 /* req_dnsbl */ "zen.spamhaus.org",
		 /* res_addr */ "127.0.0.10",
		 /* res_ttl */ 61,
	    },
	},
	 /* want_ttl */ 61,
	 /* want_score */ 1,
    }, {
	"dual dnsbl, listed by last",
	 /* dnsbl_sites */ "zen.spamhaus.org, foo.example.org",
	 /* req_addr */ "10.2.3.4", {
	    {
		 /* req_dnsbl */ "foo.example.org",
		 /* res_addr */ "127.0.0.10",
		 /* res_ttl */ 62,
	    },
	    {
		 /* req_dnsbl */ "zen.spamhaus.org",
		 /* res_addr */ "",
		 /* res_ttl */ 61,
	    },
	},
	 /* want_ttl */ 62,
	 /* want_score */ 1,
    }, {
	"dual dnsbl, unlisted address zero score",
	 /* dnsbl_sites */ "zen.spamhaus.org, foo.example.org",
	 /* req_addr */ "10.2.3.4", {
	    {
		 /* req_dnsbl */ "foo.example.org",
		 /* res_addr */ "",
		 /* res_ttl */ 62,
	    },
	    {
		 /* req_dnsbl */ "zen.spamhaus.org",
		 /* res_addr */ "",
		 /* res_ttl */ 61,
	    },
	},
	 /* want_ttl */ 61,
	 /* want_score */ 0,
    }, {
	"dual dnsbl, allowlist wins",
	 /* dnsbl_sites */ "list.dnswl.org=127.0.[0..255].[1..3]*-2, foo.example.org",
	 /* req_addr */ "10.2.3.4", {
	    {
		 /* req_dnsbl */ "foo.example.org",
		 /* res_addr */ "127.0.0.10",
		 /* res_ttl */ 62,
	    },
	    {
		 /* req_dnsbl */ "list.dnswl.org",
		 /* res_addr */ "127.0.5.2",
		 /* res_ttl */ 61,
	    },
	},
	 /* want_ttl */ 61,
	 /* want_score */ -1,
    }
};

static void test_multi_dnsbl(PTEST_CTX *t, const PTEST_CASE *tp)
{
    MOCK_SERVER *mp[MAX_DNSBL_SITES];
    struct session_state session_state;
    const char *dnsblog_path = "private/dnsblog";
    const int request_id = 0;
    const struct multi_dnsbl_data *tt;
    const struct dnsbl_data *dp;
    int     n;

    for (tt = multi_dnsbl_tests; tt < multi_dnsbl_tests
	 + PTEST_NROF(multi_dnsbl_tests); tt++) {
	if (tt->label == 0)
	    ptest_fatal(t, "Null test label in multi_dnsbl_tests array!");
	PTEST_RUN(t, tt->label, {

	    /*
	     * Reset global state and parameters used by postscreen_dnsbl.c.
	     */
	    init_psc_globals(tt->dnsbl_sites);

	    for (n = 0, dp = tt->dnsbl_data; n < MAX_DNSBL_SITES
		 && dp[n].req_dnsbl != 0; n++) {
		VSTRING *serialized_req;
		VSTRING *serialized_resp;

		/*
		 * Instantiate a mock server.
		 */
		if ((mp[n] = mock_unix_server_create(dnsblog_path)) == 0)
		    ptest_fatal(t, "mock_unix_server_create: %m");

		/*
		 * Set up the expected dnsblog requests, and the
		 * corresponding responses. The mock dnsblog server
		 * immediately generates read event requests, so we should
		 * send something soon.
		 */
		serialized_req =
		    make_attr(ATTR_FLAG_NONE,
			      SEND_ATTR_STR(MAIL_ATTR_RBL_DOMAIN,
					    dp[n].req_dnsbl),
			      SEND_ATTR_STR(MAIL_ATTR_ACT_CLIENT_ADDR,
					    tt->req_addr),
			      SEND_ATTR_INT(MAIL_ATTR_LABEL, request_id),
			      ATTR_TYPE_END);
		serialized_resp =
		    make_attr(ATTR_FLAG_NONE,
			      SEND_ATTR_STR(MAIL_ATTR_RBL_DOMAIN,
					    dp[n].req_dnsbl),
			      SEND_ATTR_STR(MAIL_ATTR_ACT_CLIENT_ADDR,
					    tt->req_addr),
			      SEND_ATTR_INT(MAIL_ATTR_LABEL, request_id),
			      SEND_ATTR_STR(MAIL_ATTR_RBL_ADDR,
					    dp[n].res_addr),
			      SEND_ATTR_INT(MAIL_ATTR_TTL,
					    dp[n].res_ttl),
			      ATTR_TYPE_END);
		mock_server_interact(mp[n], serialized_req,
					  serialized_resp);
		vstring_free(serialized_req);
		vstring_free(serialized_resp);
	    }

	    /*
	     * Send a request by calling psc_dnsbl_request(), and run the
	     * event loop once to notify the mock dnsblog servers that a
	     * request is pending. Each mock dnsblog server will receive a
	     * request, and if it matches the expected request, the mock
	     * dnsblog server will immediately send the prepared response.
	     */
	    session_state.req_addr = tt->req_addr;
	    session_state.got_dnsbl = 0;
	    session_state.got_ttl = INT_MAX;
	    session_state.got_score = INT_MAX;
	    session_state.req_idx = psc_dnsbl_request(tt->req_addr,
						      psc_dnsbl_callback,
						      &session_state);
	    event_loop(2);

	    /*
	     * Run the event loop again, to wake up psc_dnsbl_receive(). That
	     * function will deserialize the mock dnsblog server's response,
	     * and will immediately call our psc_dnsbl_callback() function to
	     * store the result into the session_state object.
	     */
	    event_loop(2);

	    /*
	     * Validate the response.
	     */
	    if (session_state.got_ttl == INT_MAX) {
		ptest_error(t, "psc_dnsbl_callback() was not called, "
			    "or did not update the session_state");
	    } else {
		if (session_state.got_ttl != tt->want_ttl)
		    ptest_error(t, "unexpected ttl: got %d, want %d",
				session_state.got_ttl, tt->want_ttl);
		if (session_state.got_score != tt->want_score)
		    ptest_error(t, "unexpected score: got %d, want %d",
				session_state.got_score, tt->want_score);
	    }

	    /*
	     * Clean up.
	     */
	    for (n = 0, dp = tt->dnsbl_data; n < MAX_DNSBL_SITES
		 && dp[n].req_dnsbl != 0; n++)
		mock_server_free(mp[n]);
	    deinit_psc_globals();
	});
    }
}

 /*
  * Test cases.
  */
const PTEST_CASE ptestcases[] = {
    {
	"single dnsbl", test_single_dnsbl,
    },
    {
	"multi dnsbl", test_multi_dnsbl,
    },
};

#include <ptest_main.h>
