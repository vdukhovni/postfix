/*++
/* NAME
/*	anvil 8
/* SUMMARY
/*	Postfix connection count and rate management
/* SYNOPSIS
/*	\fBanvil\fR [generic Postfix daemon options]
/* DESCRIPTION
/*	The Postfix \fBanvil\fR server maintains short-term statistics
/*	to defend against clients that hammer a server with either too
/*	many parallel connections or with too many successive connection
/*	attempts within a configurable time interval.
/*	This server is designed to run under control by the Postfix
/*	master server.
/* PROTOCOL
/* .ad
/* .fi
/*	When a remote client connects, a connection count (or rate) limited
/*	server should send the following request to the \fBanvil\fR server:
/* .PP
/* .in +4
/*	\fBrequest=connect\fR
/* .br
/*	\fBident=\fIstring\fR
/* .in
/* .PP
/*	This registers a new connection for the (service, client)
/*	combination specified with \fBident\fR. The \fBanvil\fR server
/*	answers with the number of simultaneous connections and the
/*	number of connections per unit time for that (service, client)
/*	combination:
/* .PP
/* .in +4
/*	\fBstatus=0\fR
/* .br
/*	\fBcount=\fInumber\fR
/* .br
/*	\fBrate=\fInumber\fR
/* .in
/* .PP
/*	The \fBrate\fR is computed as the number of connections
/*	that were registered in the current "time unit" interval.
/*	It is left up to the server to decide if the remote client
/*	exceeds the connection count (or rate) limit.
/* .PP
/*	When a remote client disconnects, a connection count (or rate) limited
/*	server should send the following request to the \fBanvil\fR server:
/* .PP
/* .in +4
/*	\fBrequest=disconnect\fR
/* .br
/*	\fBident=\fIstring\fR
/* .in
/* .PP
/*	This registers a disconnect event for the (service, client)
/*	combination specified with \fBident\fR. The \fBanvil\fR
/*	server replies with:
/* .PP
/* .ti +4
/*	\fBstatus=0\fR
/* .PP
/* SECURITY
/* .ad
/* .fi
/*	The \fBanvil\fR server does not talk to the network or to local
/*	users, and can run chrooted at fixed low privilege.
/*
/*	The \fBanvil\fR server maintains an in-memory table with information
/*	about recent clients of a connection count (or rate) limited service.
/*	Although state is kept only temporarily, this may require a lot of
/*	memory on systems that handle connections from many remote clients.
/*	To reduce memory usage, reduce the time unit over which state
/*	is kept.
/* DIAGNOSTICS
/*	Problems and transactions are logged to \fBsyslogd\fR(8).
/*
/*	Upon exit, and every \fBclient_connection_status_update_time\fR
/*	seconds, the server logs the maximal count and rate values measured,
/*	together with (service, client) information and the time of day
/*	associated with those events.
/* BUGS
/*	Systems behind network address translating routers or proxies
/*	appear to have the same client address and can run into connection
/*	count and/or rate limits falsely.
/*
/*	In this preliminary implementation, a count (or rate) limited server
/*	can have only one remote client at a time. If a server reports
/*	multiple simultaneous clients, all but the last reported client
/*	are ignored.
/* CONFIGURATION PARAMETERS
/* .ad
/* .fi
/*	The following \fBmain.cf\fR parameters are especially relevant to
/*	this program. Use the \fBpostfix reload\fR command after
/*	a configuration change.
/* .IP \fBclient_connection_rate_time_unit\fR
/*	The unit of time over which connection rates are calculated.
/* .IP \fBclient_connection_status_update_time\fR
/*	Time interval for logging the maximal connection count
/*	and connection rate information.
/* SEE ALSO
/*	smtpd(8) Postfix SMTP server
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
#include <sys/time.h>
#include <limits.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <htable.h>
#include <binhash.h>
#include <stringops.h>
#include <events.h>

/* Global library. */

#include <mail_conf.h>
#include <mail_params.h>
#include <mail_proto.h>
#include <anvil_clnt.h>

/* Server skeleton. */

#include <mail_server.h>

/* Application-specific. */

int     var_anvil_time_unit;
int     var_anvil_stat_time;

 /*
  * State.
  */
static HTABLE *anvil_remote_map;	/* indexed by service+ remote client */
static BINHASH *anvil_local_map;	/* indexed by local client handle */

 /*
  * Absent a real-time query interface, these are logged at process exit time
  * and at regular intervals.
  */
static int max_count;
static char *max_count_user;
static time_t max_count_time;

static int max_rate;
static char *max_rate_user;
static time_t max_rate_time;

 /*
  * Remote connection state, one instance for each (service, client) pair.
  */
typedef struct {
    char   *ident;			/* lookup key */
    int     count;			/* connection count */
    int     rate;			/* connection rate */
    time_t  start;			/* time of first rate sample */
} ANVIL_REMOTE;

 /*
  * Local server state, one per server instance. This allows us to clean up
  * connection state when a local server goes away without cleaning up.
  */
typedef struct {
    ANVIL_REMOTE *anvil_remote;		/* XXX should be list */
} ANVIL_LOCAL;

 /*
  * Silly little macros.
  */
#define STR(x)			vstring_str(x)
#define STREQ(x,y)		(strcmp((x), (y)) == 0)

 /*
  * The following operations are implemented as macros with recognizable
  * names so that we don't lose sight of what the code is trying to do.
  * 
  * Related operations are defined side by side so that the code implementing
  * them isn't pages apart.
  */

/* Create new (service, client) state. */

#define ANVIL_REMOTE_FIRST(remote, id) \
    do { \
	(remote)->ident = mystrdup(id); \
	(remote)->count = 1; \
	(remote)->rate = 1; \
	(remote)->start = event_time(); \
    } while(0)

/* Destroy unused (service, client) state. */

#define ANVIL_REMOTE_FREE(remote) \
    do { \
	myfree((remote)->ident); \
	myfree((char *) (remote)); \
    } while(0)

/* Add connection to (service, client) state. */

#define ANVIL_REMOTE_NEXT(remote) \
    do { \
	time_t _now = event_time(); \
	if ((remote)->start + var_anvil_time_unit < _now) { \
	    (remote)->rate = 1; \
	    (remote)->start = _now; \
	} else if ((remote)->rate < INT_MAX) { \
	    (remote)->rate += 1; \
	} \
	if ((remote)->count == 0) \
	    event_cancel_timer(anvil_remote_expire, (char *) remote); \
	(remote)->count++; \
    } while(0)

/* Drop connection from (service, client) state. */

#define ANVIL_REMOTE_DROP_ONE(remote) \
    do { \
	if ((remote) && (remote)->count > 0) { \
	    if (--(remote)->count == 0) \
		event_request_timer(anvil_remote_expire, (char *) remote, \
			var_anvil_time_unit); \
	} \
    } while(0)

/* Create local server state. */

#define ANVIL_LOCAL_INIT(local) \
    do { \
	(local)->anvil_remote = 0; \
    } while(0)

/* Add connection to local server. */

#define ANVIL_LOCAL_ADD_ONE(local, remote) \
    do { \
	/* XXX allow multiple remote clients per local server. */ \
	if ((local)->anvil_remote) \
	    ANVIL_REMOTE_DROP_ONE((local)->anvil_remote); \
	(local)->anvil_remote = (remote); \
    } while(0)

/* Drop connection from local server. */

#define ANVIL_LOCAL_DROP_ONE(local, remote) \
    do { \
	/* XXX allow multiple remote clients per local server. */ \
	if ((local)->anvil_remote == (remote)) \
	    (local)->anvil_remote = 0; \
    } while(0)

/* Drop all connections from local server. */

#define ANVIL_LOCAL_DROP_ALL(stream, local) \
    do { \
	 /* XXX allow multiple remote clients per local server. */ \
	if ((local)->anvil_remote) \
	    anvil_remote_disconnect((stream), (local)->anvil_remote->ident); \
    } while (0)

/* anvil_remote_expire - purge expired connection state */

static void anvil_remote_expire(int unused_event, char *context)
{
    ANVIL_REMOTE *anvil_remote = (ANVIL_REMOTE *) context;
    char   *myname = "anvil_remote_expire";

    if (msg_verbose)
	msg_info("%s %s", myname, anvil_remote->ident);

    if (anvil_remote->count != 0)
	msg_panic("%s: bad connection count: %d",
		  myname, anvil_remote->count);

    htable_delete(anvil_remote_map, anvil_remote->ident,
		  (void (*) (char *)) 0);
    ANVIL_REMOTE_FREE(anvil_remote);
}

/* anvil_remote_lookup - dump address status */

static void anvil_remote_lookup(VSTREAM *client_stream, const char *ident)
{
    ANVIL_REMOTE *anvil_remote;
    char   *myname = "anvil_remote_lookup";
    HTABLE_INFO **ht_info;
    HTABLE_INFO **ht;

    if (msg_verbose)
	msg_info("%s fd=%d stream=0x%lx ident=%s",
		 myname, vstream_fileno(client_stream),
		 (unsigned long) client_stream, ident);

    /*
     * Look up remote client information.
     */
    if (STREQ(ident, "*")) {
	attr_print_plain(client_stream, ATTR_FLAG_MORE,
			 ATTR_TYPE_NUM, ANVIL_ATTR_STATUS, ANVIL_STAT_OK,
			 ATTR_TYPE_END);
	ht_info = htable_list(anvil_remote_map);
	for (ht = ht_info; *ht; ht++) {
	    anvil_remote = (ANVIL_REMOTE *) ht[0]->value;
	    attr_print_plain(client_stream, ATTR_FLAG_MORE,
			     ATTR_TYPE_STR, ANVIL_ATTR_IDENT, ht[0]->key,
		       ATTR_TYPE_NUM, ANVIL_ATTR_COUNT, anvil_remote->count,
			 ATTR_TYPE_NUM, ANVIL_ATTR_RATE, anvil_remote->rate,
			     ATTR_TYPE_END);
	}
	attr_print_plain(client_stream, ATTR_FLAG_NONE, ATTR_TYPE_END);
	myfree((char *) ht_info);
    } else if ((anvil_remote =
	      (ANVIL_REMOTE *) htable_find(anvil_remote_map, ident)) == 0) {
	attr_print_plain(client_stream, ATTR_FLAG_NONE,
			 ATTR_TYPE_NUM, ANVIL_ATTR_STATUS, ANVIL_STAT_FAIL,
			 ATTR_TYPE_NUM, ANVIL_ATTR_COUNT, 0,
			 ATTR_TYPE_NUM, ANVIL_ATTR_RATE, 0,
			 ATTR_TYPE_END);
    } else {
	attr_print_plain(client_stream, ATTR_FLAG_NONE,
			 ATTR_TYPE_NUM, ANVIL_ATTR_STATUS, ANVIL_STAT_OK,
		       ATTR_TYPE_NUM, ANVIL_ATTR_COUNT, anvil_remote->count,
			 ATTR_TYPE_NUM, ANVIL_ATTR_RATE, anvil_remote->rate,
			 ATTR_TYPE_END);
    }
}

/* anvil_remote_connect - report connection event, query address status */

static void anvil_remote_connect(VSTREAM *client_stream, const char *ident)
{
    ANVIL_REMOTE *anvil_remote;
    ANVIL_LOCAL *anvil_local;
    char   *myname = "anvil_remote_connect";

    if (msg_verbose)
	msg_info("%s fd=%d stream=0x%lx ident=%s",
		 myname, vstream_fileno(client_stream),
		 (unsigned long) client_stream, ident);

    /*
     * Look up remote connection count information. Update remote connection
     * rate information. Simply reset the counter every var_anvil_time_unit
     * seconds. This is easier than maintaining a moving average and it gives
     * a quicker response to tresspassers.
     */
    if ((anvil_remote =
	 (ANVIL_REMOTE *) htable_find(anvil_remote_map, ident)) == 0) {
	anvil_remote = (ANVIL_REMOTE *) mymalloc(sizeof(*anvil_remote));
	ANVIL_REMOTE_FIRST(anvil_remote, ident);
	htable_enter(anvil_remote_map, ident, (char *) anvil_remote);
    } else {
	ANVIL_REMOTE_NEXT(anvil_remote);
    }

    /*
     * Record this connection under the local client information, so that we
     * can clean up all its connection state when the local client goes away.
     */
    if ((anvil_local =
	 (ANVIL_LOCAL *) binhash_find(anvil_local_map,
				      (char *) &client_stream,
				      sizeof(client_stream))) == 0) {
	anvil_local = (ANVIL_LOCAL *) mymalloc(sizeof(*anvil_local));
	ANVIL_LOCAL_INIT(anvil_local);
	binhash_enter(anvil_local_map, (char *) &client_stream,
		      sizeof(client_stream), (char *) anvil_local);
    }
    ANVIL_LOCAL_ADD_ONE(anvil_local, anvil_remote);
    if (msg_verbose)
	msg_info("%s: anvil_local 0x%lx",
		 myname, (unsigned long) anvil_local);

    /*
     * Respond to the local client.
     */
    attr_print_plain(client_stream, ATTR_FLAG_NONE,
		     ATTR_TYPE_NUM, ANVIL_ATTR_STATUS, ANVIL_STAT_OK,
		     ATTR_TYPE_NUM, ANVIL_ATTR_COUNT, anvil_remote->count,
		     ATTR_TYPE_NUM, ANVIL_ATTR_RATE, anvil_remote->rate,
		     ATTR_TYPE_END);

    /*
     * Update local statistics.
     */
    if (anvil_remote->rate > max_rate) {
	max_rate = anvil_remote->rate;
	if (max_rate_user == 0) {
	    max_rate_user = mystrdup(anvil_remote->ident);
	} else if (!STREQ(max_rate_user, anvil_remote->ident)) {
	    myfree(max_rate_user);
	    max_rate_user = mystrdup(anvil_remote->ident);
	}
	max_rate_time = event_time();
    }
    if (anvil_remote->count > max_count) {
	max_count = anvil_remote->count;
	if (max_count_user == 0) {
	    max_count_user = mystrdup(anvil_remote->ident);
	} else if (!STREQ(max_count_user, anvil_remote->ident)) {
	    myfree(max_count_user);
	    max_count_user = mystrdup(anvil_remote->ident);
	}
	max_count_time = event_time();
    }
}

/* anvil_remote_disconnect - report disconnect event */

static void anvil_remote_disconnect(VSTREAM *client_stream, const char *ident)
{
    ANVIL_REMOTE *anvil_remote;
    ANVIL_LOCAL *anvil_local;
    char   *myname = "anvil_remote_disconnect";

    if (msg_verbose)
	msg_info("%s fd=%d stream=0x%lx ident=%s",
		 myname, vstream_fileno(client_stream),
		 (unsigned long) client_stream, ident);

    /*
     * Look up the remote client.
     */
    if ((anvil_remote =
	 (ANVIL_REMOTE *) htable_find(anvil_remote_map, ident)) != 0)
	ANVIL_REMOTE_DROP_ONE(anvil_remote);

    /*
     * Update the local client information.
     */
    if ((anvil_local =
	 (ANVIL_LOCAL *) binhash_find(anvil_local_map,
				      (char *) &client_stream,
				      sizeof(client_stream))) != 0)
	ANVIL_LOCAL_DROP_ONE(anvil_local, anvil_remote);
    if (msg_verbose)
	msg_info("%s: anvil_local 0x%lx",
		 myname, (unsigned long) anvil_local);

    /*
     * Respond to the local client.
     */
    attr_print_plain(client_stream, ATTR_FLAG_NONE,
		     ATTR_TYPE_NUM, ANVIL_ATTR_STATUS, ANVIL_STAT_OK,
		     ATTR_TYPE_END);
}

/* anvil_service_done - clean up */

static void anvil_service_done(VSTREAM *client_stream, char *unused_service,
			               char **unused_argv)
{
    ANVIL_LOCAL *anvil_local;
    char   *myname = "anvil_service_done";

    if (msg_verbose)
	msg_info("%s fd=%d stream=0x%lx",
		 myname, vstream_fileno(client_stream),
		 (unsigned long) client_stream);

    /*
     * Look up the local client, and get rid of open remote connection state
     * that we still have for this local client. Do not destroy remote client
     * status information before it expires.
     */
    if ((anvil_local =
	 (ANVIL_LOCAL *) binhash_find(anvil_local_map,
				      (char *) &client_stream,
				      sizeof(client_stream))) != 0) {
	if (msg_verbose)
	    msg_info("%s: anvil_local 0x%lx",
		     myname, (unsigned long) anvil_local);
	ANVIL_LOCAL_DROP_ALL(client_stream, anvil_local);
	binhash_delete(anvil_local_map,
		       (char *) &client_stream,
		       sizeof(client_stream), myfree);
    } else if (msg_verbose)
	msg_info("client socket not found for fd=%d",
		 vstream_fileno(client_stream));
}

/* anvil_service - perform service for client */

static void anvil_service(VSTREAM *client_stream, char *unused_service, char **argv)
{
    VSTRING *request = vstring_alloc(10);
    VSTRING *ident = vstring_alloc(10);

    /*
     * Sanity check. This service takes no command-line arguments.
     */
    if (argv[0])
	msg_fatal("unexpected command-line argument: %s", argv[0]);

    /*
     * This routine runs whenever a client connects to the socket dedicated
     * to the client connection rate management service. All
     * connection-management stuff is handled by the common code in
     * multi_server.c.
     */
    if (attr_scan_plain(client_stream,
			ATTR_FLAG_MISSING | ATTR_FLAG_STRICT,
			ATTR_TYPE_STR, ANVIL_ATTR_REQ, request,
			ATTR_TYPE_STR, ANVIL_ATTR_IDENT, ident,
			ATTR_TYPE_END) == 2) {
	if (STREQ(STR(request), ANVIL_REQ_CONN)) {
	    anvil_remote_connect(client_stream, STR(ident));
	} else if (STREQ(STR(request), ANVIL_REQ_DISC)) {
	    anvil_remote_disconnect(client_stream, STR(ident));
	} else if (STREQ(STR(request), ANVIL_REQ_LOOKUP)) {
	    anvil_remote_lookup(client_stream, STR(ident));
	} else {
	    msg_warn("unrecognized request: \"%s\", ignored", STR(request));
	    attr_print_plain(client_stream, ATTR_FLAG_NONE,
			  ATTR_TYPE_NUM, ANVIL_ATTR_STATUS, ANVIL_STAT_FAIL,
			     ATTR_TYPE_END);
	}
	vstream_fflush(client_stream);
    } else {
	/* Note: invokes anvil_service_done() */
	multi_server_disconnect(client_stream);
    }
    vstring_free(ident);
    vstring_free(request);
}

/* post_jail_init - post-jail initialization */

static void post_jail_init(char *unused_name, char **unused_argv)
{
    static void anvil_status_update(int, char *);

    /*
     * Dump and reset extreme usage every so often.
     */
    event_request_timer(anvil_status_update, (char *) 0, var_anvil_stat_time);

    /*
     * Initial client state tables.
     */
    anvil_remote_map = htable_create(1000);
    anvil_local_map = binhash_create(100);

    /*
     * Do not limit the number of client requests.
     */
    var_use_limit = 0;
}

/* anvil_status_dump - log and reset extreme usage */

static void anvil_status_dump(char *unused_name, char **unused_argv)
{
    if (max_rate > 1) {
	msg_info("statistics: max connection rate %d/%ds for (%s) at %.15s",
		 max_rate, var_anvil_time_unit,
		 max_rate_user, ctime(&max_rate_time) + 4);
	max_rate = 0;
    }
    if (max_count > 1) {
	msg_info("statistics: max connection count %d for (%s) at %.15s",
		 max_count, max_count_user, ctime(&max_count_time) + 4);
	max_count = 0;
    }
}

/* anvil_status_update - log and reset extreme usage periodically */

static void anvil_status_update(int unused_event, char *context)
{
    anvil_status_dump((char *) 0, (char **) 0);
    event_request_timer(anvil_status_update, context, var_anvil_stat_time);
}

/* main - pass control to the multi-threaded skeleton */

int     main(int argc, char **argv)
{
    static CONFIG_TIME_TABLE time_table[] = {
	VAR_ANVIL_TIME_UNIT, DEF_ANVIL_TIME_UNIT, &var_anvil_time_unit, 1, 0,
	VAR_ANVIL_STAT_TIME, DEF_ANVIL_STAT_TIME, &var_anvil_stat_time, 1, 0,
	0,
    };

    multi_server_main(argc, argv, anvil_service,
		      MAIL_SERVER_TIME_TABLE, time_table,
		      MAIL_SERVER_POST_INIT, post_jail_init,
		      MAIL_SERVER_SOLITARY,
		      MAIL_SERVER_PRE_DISCONN, anvil_service_done,
		      MAIL_SERVER_EXIT, anvil_status_dump,
		      0);
}
