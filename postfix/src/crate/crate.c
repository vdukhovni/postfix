/*++
/* NAME
/*	crate 8
/* SUMMARY
/*	Postfix connection count and rate management
/* SYNOPSIS
/*	\fBcrate\fR [generic Postfix daemon options]
/* DESCRIPTION
/*	The Postfix \fBcrate\fR server maintains statistics that other
/*	Postfix servers can use to limit the number of simultaneous
/*	connections as well as the frequency of connection attempts
/*	over a configurable unit of time.
/*	This server is designed to run under control by the Postfix
/*	master server.
/* PROTOCOL
/* .ad
/* .fi
/*	When a connection is established, a rate limited server
/*	sends the following request to the \fBcrate\fR server:
/* .PP
/* .in +4
/*	\fBrequest=connect\fR
/* .br
/*	\fBident=\fIstring\fR
/* .in
/* .PP
/*	This registers a new connection for the remote client and the rate
/*	limited service specified with \fIstring\fR. The \fBcrate\fR server
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
/*	It is left up to the rate limited service to decide if the
/*	remote client connection request is acceptable.
/* .PP
/*	When a remote client disconnects, a rate limited server
/*	sends the following request to the \fBcrate\fR server:
/* .PP
/* .in +4
/*	\fBrequest=disconnect\fR
/* .br
/*	\fBident=\fIstring\fR
/* .in
/* .PP
/*	This registers a disconnect event for the remote client and the rate
/*	limited service specified with \fIstring\fR. The rate limit management
/*	server replies with:
/* .PP
/* .ti +4
/*	\fBstatus=0\fR
/* .PP
/* SECURITY
/* .ad
/* .fi
/*	The connection count and rate management service is not security
/*	sensitive. It does not talk to the network or local users,
/*	and it can run chrooted at fixed low privilege.
/*
/*	This server maintains an in-memory table with information about
/*	past and current clients of a rate limited service. Although state
/*	is kept only temporarily, this may require a lot of memory when a
/*	system handles connections from many remote clients, or when a system
/*	comes under a distributed denial of service attack. In that case,
/*	reduce the time unit over which statistics are kept.
/*
/*	Systems behind network address translating routers or proxies
/*	appear to have the same client address and can run into connection
/*	count and/or rate limits falsely.
/* DIAGNOSTICS
/*	Problems and transactions are logged to \fBsyslogd\fR(8).
/* BUGS
/*	All state is lost when the service is restarted.
/*
/*	In this first implementation, a count or rate limited server
/*	can have only one client at a time.
/* CONFIGURATION PARAMETERS
/* .ad
/* .fi
/*	The following \fBmain.cf\fR parameters are especially relevant to
/*	this program. Use the \fBpostfix reload\fR command after
/*	a configuration change.
/* .IP \fBconnection_rate_purge_delay\fR
/*	How long remote client state is remembered after the remote client
/*	has disconnected completely. This should not be smaller than the
/*	unit of time over which connection rates are calculated.
/* .IP \fBconnection_rate_time_unit\fR
/*	The unit of time over which connection rates are calculated.
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
#include <crate_clnt.h>

/* Server skeleton. */

#include <mail_server.h>

/* Application-specific. */

int     var_crate_sample;
int     var_crate_purge;

 /*
  * State.
  */
static HTABLE *crate_remote_map;
static BINHASH *crate_local_map;

 /*
  * Remote client state.
  */
typedef struct {
    char   *ident;			/* lookup key */
    int     count;			/* connection count */
    int     rate;			/* connection rate */
    time_t  start;			/* time of first rate sample */
} CRATE_REMOTE;

 /*
  * Local (i.e. rate limit client) state.
  */
typedef struct {
    CRATE_REMOTE *crate_remote;		/* XXX should be list */
} CRATE_LOCAL;

 /*
  * Silly little macros.
  */
#define STR(x)			vstring_str(x)
#define STREQ(x,y)		(strcmp((x), (y)) == 0)

 /*
  * The following operations are implemented as macros with recognizable
  * names so that we don't lose sight of what the code is trying to do, and
  * related operations are defined side by side so that it isn't pages apart.
  */
#define CRATE_REMOTE_FIRST(remote, id) \
    do { \
	(remote)->ident = mystrdup(id); \
	(remote)->count = 1; \
	(remote)->rate = 1; \
	(remote)->start = event_time(); \
    } while(0)

#define CRATE_REMOTE_FREE(remote) \
    do { \
	myfree((remote)->ident); \
	myfree((char *) (remote)); \
    } while(0)

#define CRATE_REMOTE_NEXT(remote) \
    do { \
	time_t _now = event_time(); \
	if ((remote)->start + var_crate_sample < _now) { \
	    (remote)->rate = 1; \
	    (remote)->start = _now; \
	} else if ((remote)->rate < INT_MAX) { \
	    (remote)->rate += 1; \
	} \
	if ((remote)->count == 0) \
	    event_cancel_timer(crate_remote_expire, (char *) remote); \
	(remote)->count++; \
    } while(0)

#define CRATE_REMOTE_DROP_ONE(remote) \
    do { \
	if ((remote) && (remote)->count > 0) { \
	    if (--(remote)->count == 0) \
		event_request_timer(crate_remote_expire, (char *) remote, \
			var_crate_purge); \
	} \
    } while(0)

#define CRATE_LOCAL_INIT(local) \
    do { \
	(local)->crate_remote = 0; \
    } while(0)

#define CRATE_LOCAL_ADD_ONE(local, remote) \
    do { \
	/* XXX allow multiple remote clients per local server. */ \
	if ((local)->crate_remote) \
	    CRATE_REMOTE_DROP_ONE((local)->crate_remote); \
	(local)->crate_remote = (remote); \
    } while(0)

#define CRATE_LOCAL_DROP_ONE(local, remote) \
    do { \
	/* XXX allow multiple remote clients per local server. */ \
	if ((local)->crate_remote == (remote)) \
	    (local)->crate_remote = 0; \
    } while(0)

#define CRATE_LOCAL_DROP_ALL(stream, local) \
    do { \
	 /* XXX allow multiple remote clients per local server. */ \
	if ((local)->crate_remote) \
	    crate_remote_disconnect((stream), (local)->crate_remote->ident); \
    } while (0)

/* crate_remote_expire - purge expired connection state */

static void crate_remote_expire(int unused_event, char *context)
{
    CRATE_REMOTE *crate_remote = (CRATE_REMOTE *) context;
    char   *myname = "crate_remote_expire";

    if (msg_verbose)
	msg_info("%s %s", myname, crate_remote->ident);

    if (crate_remote->count != 0)
	msg_panic("%s: bad connection count: %d",
		  myname, crate_remote->count);

    htable_delete(crate_remote_map, crate_remote->ident,
		  (void (*) (char *)) 0);
    CRATE_REMOTE_FREE(crate_remote);
}

/* crate_remote_lookup - dump address status */

static void crate_remote_lookup(VSTREAM *client_stream, const char *ident)
{
    CRATE_REMOTE *crate_remote;
    char   *myname = "crate_remote_lookup";
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
			 ATTR_TYPE_NUM, CRATE_ATTR_STATUS, CRATE_STAT_OK,
			 ATTR_TYPE_END);
	ht_info = htable_list(crate_remote_map);
	for (ht = ht_info; *ht; ht++) {
	    crate_remote = (CRATE_REMOTE *) ht[0]->value;
	    attr_print_plain(client_stream, ATTR_FLAG_MORE,
			     ATTR_TYPE_STR, CRATE_ATTR_IDENT, ht[0]->key,
		       ATTR_TYPE_NUM, CRATE_ATTR_COUNT, crate_remote->count,
			 ATTR_TYPE_NUM, CRATE_ATTR_RATE, crate_remote->rate,
			     ATTR_TYPE_END);
	}
	attr_print_plain(client_stream, ATTR_FLAG_NONE, ATTR_TYPE_END);
	myfree((char *) ht_info);
    } else if ((crate_remote =
	      (CRATE_REMOTE *) htable_find(crate_remote_map, ident)) == 0) {
	attr_print_plain(client_stream, ATTR_FLAG_NONE,
			 ATTR_TYPE_NUM, CRATE_ATTR_STATUS, CRATE_STAT_FAIL,
			 ATTR_TYPE_NUM, CRATE_ATTR_COUNT, 0,
			 ATTR_TYPE_NUM, CRATE_ATTR_RATE, 0,
			 ATTR_TYPE_END);
    } else {
	attr_print_plain(client_stream, ATTR_FLAG_NONE,
			 ATTR_TYPE_NUM, CRATE_ATTR_STATUS, CRATE_STAT_OK,
		       ATTR_TYPE_NUM, CRATE_ATTR_COUNT, crate_remote->count,
			 ATTR_TYPE_NUM, CRATE_ATTR_RATE, crate_remote->rate,
			 ATTR_TYPE_END);
    }
}

/* crate_remote_connect - report connection event, query address status */

static void crate_remote_connect(VSTREAM *client_stream, const char *ident)
{
    CRATE_REMOTE *crate_remote;
    CRATE_LOCAL *crate_local;
    char   *myname = "crate_remote_connect";
    time_t  now;

    if (msg_verbose)
	msg_info("%s fd=%d stream=0x%lx ident=%s",
		 myname, vstream_fileno(client_stream),
		 (unsigned long) client_stream, ident);

    /*
     * Look up remote connection count information. Update remote connection
     * rate information. Simply reset the counter every var_crate_sample
     * seconds. This is easier than maintaining a moving average and it gives
     * a quicker response to tresspassers.
     */
    if ((crate_remote =
	 (CRATE_REMOTE *) htable_find(crate_remote_map, ident)) == 0) {
	crate_remote = (CRATE_REMOTE *) mymalloc(sizeof(*crate_remote));
	CRATE_REMOTE_FIRST(crate_remote, ident);
	htable_enter(crate_remote_map, ident, (char *) crate_remote);
    } else {
	CRATE_REMOTE_NEXT(crate_remote);
    }

    /*
     * Record this connection under the local client information, so that we
     * can clean up all its connection state when the local client goes away.
     */
    if ((crate_local =
	 (CRATE_LOCAL *) binhash_find(crate_local_map,
				      (char *) &client_stream,
				      sizeof(client_stream))) == 0) {
	crate_local = (CRATE_LOCAL *) mymalloc(sizeof(*crate_local));
	CRATE_LOCAL_INIT(crate_local);
	binhash_enter(crate_local_map, (char *) &client_stream,
		      sizeof(client_stream), (char *) crate_local);
    }
    CRATE_LOCAL_ADD_ONE(crate_local, crate_remote);
    if (msg_verbose)
	msg_info("%s: crate_local 0x%lx",
		 myname, (unsigned long) crate_local);

    /*
     * Respond to the local client.
     */
    attr_print_plain(client_stream, ATTR_FLAG_NONE,
		     ATTR_TYPE_NUM, CRATE_ATTR_STATUS, CRATE_STAT_OK,
		     ATTR_TYPE_NUM, CRATE_ATTR_COUNT, crate_remote->count,
		     ATTR_TYPE_NUM, CRATE_ATTR_RATE, crate_remote->rate,
		     ATTR_TYPE_END);
}

/* crate_remote_disconnect - report disconnect event */

static void crate_remote_disconnect(VSTREAM *client_stream, const char *ident)
{
    CRATE_REMOTE *crate_remote;
    CRATE_LOCAL *crate_local;
    char   *myname = "crate_remote_disconnect";

    if (msg_verbose)
	msg_info("%s fd=%d stream=0x%lx ident=%s",
		 myname, vstream_fileno(client_stream),
		 (unsigned long) client_stream, ident);

    /*
     * Look up the remote client.
     */
    if ((crate_remote =
	 (CRATE_REMOTE *) htable_find(crate_remote_map, ident)) != 0)
	CRATE_REMOTE_DROP_ONE(crate_remote);

    /*
     * Update the local client information.
     */
    if ((crate_local =
	 (CRATE_LOCAL *) binhash_find(crate_local_map,
				      (char *) &client_stream,
				      sizeof(client_stream))) != 0)
	CRATE_LOCAL_DROP_ONE(crate_local, crate_remote);
    if (msg_verbose)
	msg_info("%s: crate_local 0x%lx",
		 myname, (unsigned long) crate_local);

    /*
     * Respond to the local client.
     */
    attr_print_plain(client_stream, ATTR_FLAG_NONE,
		     ATTR_TYPE_NUM, CRATE_ATTR_STATUS, CRATE_STAT_OK,
		     ATTR_TYPE_END);
}

/* crate_service_done - clean up */

static void crate_service_done(VSTREAM *client_stream, char *unused_service,
			               char **unused_argv)
{
    CRATE_LOCAL *crate_local;
    char   *myname = "crate_service_done";

    if (msg_verbose)
	msg_info("%s fd=%d stream=0x%lx",
		 myname, vstream_fileno(client_stream),
		 (unsigned long) client_stream);

    /*
     * Look up the local client, and get rid of open remote connection state
     * that we still have for this local client. Do not destroy remote client
     * status information before it expires.
     */
    if ((crate_local =
	 (CRATE_LOCAL *) binhash_find(crate_local_map,
				      (char *) &client_stream,
				      sizeof(client_stream))) != 0) {
	if (msg_verbose)
	    msg_info("%s: crate_local 0x%lx",
		     myname, (unsigned long) crate_local);
	CRATE_LOCAL_DROP_ALL(client_stream, crate_local);
	binhash_delete(crate_local_map,
		       (char *) &client_stream,
		       sizeof(client_stream), myfree);
    } else if (msg_verbose)
	msg_info("client socket not found for fd=%d",
		 vstream_fileno(client_stream));
}

/* crate_service - perform service for client */

static void crate_service(VSTREAM *client_stream, char *service, char **argv)
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
			ATTR_TYPE_STR, CRATE_ATTR_REQ, request,
			ATTR_TYPE_STR, CRATE_ATTR_IDENT, ident,
			ATTR_TYPE_END) == 2) {
	if (STREQ(STR(request), CRATE_REQ_CONN)) {
	    crate_remote_connect(client_stream, STR(ident));
	} else if (STREQ(STR(request), CRATE_REQ_DISC)) {
	    crate_remote_disconnect(client_stream, STR(ident));
	} else if (STREQ(STR(request), CRATE_REQ_LOOKUP)) {
	    crate_remote_lookup(client_stream, STR(ident));
	} else {
	    msg_warn("unrecognized request: \"%s\", ignored", STR(request));
	    attr_print_plain(client_stream, ATTR_FLAG_NONE,
			  ATTR_TYPE_NUM, CRATE_ATTR_STATUS, CRATE_STAT_FAIL,
			     ATTR_TYPE_END);
	}
	vstream_fflush(client_stream);
    } else {
	/* Note: invokes crate_service_done() */
	multi_server_disconnect(client_stream);
    }
    vstring_free(ident);
    vstring_free(request);
}

/* post_jail_init - post-jail initialization */

static void post_jail_init(char *unused_name, char **unused_argv)
{

    /*
     * Sanity check.
     */
    if (var_crate_purge < var_crate_sample)
	msg_fatal("%s should not be less than %s",
		  VAR_CRATE_PURGE, VAR_CRATE_SAMPLE);

    /*
     * Initial client state tables.
     */
    crate_remote_map = htable_create(1000);
    crate_local_map = binhash_create(100);

    /*
     * Do not limit the number of client requests.
     */
    var_use_limit = 0;
}

/* main - pass control to the multi-threaded skeleton */

int     main(int argc, char **argv)
{
    static CONFIG_TIME_TABLE time_table[] = {
	VAR_CRATE_SAMPLE, DEF_CRATE_SAMPLE, &var_crate_sample, 1, 0,
	VAR_CRATE_PURGE, DEF_CRATE_PURGE, &var_crate_purge, 1, 0,
	0,
    };

    multi_server_main(argc, argv, crate_service,
		      MAIL_SERVER_TIME_TABLE, time_table,
		      MAIL_SERVER_POST_INIT, post_jail_init,
		      MAIL_SERVER_SOLITARY,
		      MAIL_SERVER_PRE_DISCONN, crate_service_done,
		      0);
}
