/*++
/* NAME
/*	postscreen 8
/* SUMMARY
/*	Postfix SMTP triage server
/* SYNOPSIS
/*	\fBpostscreen\fR [generic Postfix daemon options]
/* DESCRIPTION
/*	The Postfix \fBpostscreen\fR(8) server performs triage on
/*	multiple inbound SMTP connections in parallel. The program
/*	can run in two basic modes.
/*
/*	In \fBobservation mode\fR the purpose is to collect statistics
/*	without actually blocking mail. \fBpostscreen\fR(8) runs a
/*	number of tests before it forwards a connection to a real
/*	SMTP server process.  These tests introduce a delay of a
/*	few seconds; once a client passes the tests as "clean", its
/*	IP address is whitelisted and subsequent connections incur
/*	no delays until the whitelist entry expires.
/*
/*	In \fBenforcement mode\fR the purpose is to block mail
/*	without using up one Postfix SMTP server process for every
/*	connection.  Here, \fBpostscreen\fR(8) terminates connections
/*	from SMTP clients that fail the above tests, and forwards
/*	only the remaining connections to a real SMTP server process.
/*	By running time-consuming spam tests in parallel in
/*	\fBpostscreen\fR(8), more Postfix SMTP server processes
/*	remain available for legitimate clients.
/* .PP
/*	Note: \fBpostscreen\fR(8) is not an SMTP proxy; this is
/*	intentional. The purpose is to prioritize legitimate clients
/*	with as little overhead as possible.
/*
/*	\fBpostscreen\fR(8) logs its observations and takes actions
/*	as described in the sections that follow.
/* PERMANENT BLACKLIST TEST
/* .ad
/* .fi
/*	The postscreen_blacklist_networks parameter (default: empty)
/*	specifies a permanent blacklist for SMTP client IP addresses.
/*	The address syntax is as with mynetworks. When the SMTP
/*	client address matches the permanent blacklist, this is
/*	logged as:
/* .sp
/* .nf
/*	\fBBLACKLISTED \fIaddress\fR
/* .fi
/* .sp
/*	The postscreen_blacklist_action parameter specifies the
/*	action that is taken next:
/* .IP "\fBcontinue\fR (default, observation mode)"
/*	Continue with the SMTP GREETING PHASE TESTS below.
/* .IP "\fBdrop\fR (enforcement mode)"
/*	Drop the connection immediately with a 521 SMTP reply.  In
/*	a future implementation, the connection may instead be
/*	passed to a dummy SMTP protocol engine that logs sender and
/*	recipient information.
/* PERMANENT WHITELIST TEST
/* .ad
/* .fi
/*	The postscreen_whitelist_networks parameter (default:
/*	$mynetworks) specifies a permanent whitelist for SMTP client
/*	IP addresses.  This feature is not used for addresses that
/*	appear on the permanent blacklist. When the SMTP client
/*	address matches the permanent whitelist, this is logged as:
/* .sp
/* .nf
/*	\fBWHITELISTED \fIaddress\fR
/* .fi
/* .sp
/*	The action is not configurable: immediately forward the
/*	connection to a real SMTP server process.
/* TEMPORARY WHITELIST TEST
/* .ad    
/* .fi
/*	The \fBpostscreen\fR(8) daemon maintains a \fItemporary\fR
/*	whitelist for SMTP client IP addresses that have passed all
/*	the tests described below. The postscreen_cache_map parameter
/*	specifies the location of the temporary whitelist.  The
/*	temporary whitelist is not used for SMTP client addresses
/*	that appear on the \fIpermanent\fR blacklist or whitelist.
/*
/*	When the SMTP client address appears on the temporary
/*	whitelist, this is logged as:
/* .sp
/* .nf
/*	\fBPASS OLD \fIaddress\fR
/* .fi
/* .sp
/*	The action is not configurable: immediately forward the
/*	connection to a real SMTP server process.  The client is
/*	excluded from further tests until its temporary whitelist
/*	entry expires, as controlled with the postscreen_cache_ttl
/*	parameter.  Expired entries are silently renewed if possible.
/* SMTP GREETING PHASE TESTS
/* .ad
/* .fi
/*	The postscreen_greet_wait parameter specifies a time interval
/*	during which \fBpostscreen\fR(8) runs a number of tests as
/*	described below.  These tests run before the client may
/*	see the real SMTP server's "220 text..." server greeting.
/*	When the SMTP client passes all the tests, this is logged
/*	as:
/* .sp
/* .nf
/*	\fBPASS NEW \fIaddress\fR
/* .fi
/* .sp
/*	The action is to forward the connection to a real SMTP
/*	server process and to create a temporary whitelist entry
/*	that excludes the client IP address from further tests until
/*	the temporary whitelist entry expires, as controlled with
/*	the postscreen_cache_ttl parameter.
/*
/*	In a future implementation, the connection may first be passed to
/*	a dummy SMTP protocol engine that implements more protocol
/*	tests including greylisting, before the client is allowed
/*	to talk to a real SMTP server process.
/* PREGREET TEST
/* .ad
/* .fi
/*	The postscreen_greet_banner parameter specifies the text
/*	for a "220-text..." teaser banner (default: $smtpd_banner).
/*	The \fBpostscreen\fR(8) daemon sends this before the
/*	postscreen_greet_wait timer is started.  The purpose of the
/*	teaser banner is to confuse SPAM clients so that they speak
/*	before their turn. It has no effect on SMTP clients that
/*	correctly implement the protocol.
/*
/*	To avoid problems with broken SMTP engines in network
/*	appliances, either exclude them from all tests with the
/*	postscreen_whitelist_networks feature or else specify an
/*	empty postscreen_greet_banner value to disable the "220-text..."
/*	teaser banner.
/*
/*	When an SMTP client speaks before the postscreen_greet_wait
/*	time has elapsed, this is logged as:
/* .sp
/* .nf
/*	\fBPREGREET \fIcount \fBafter \fItime \fBfrom \fIaddress text...\fR
/* .fi
/* .sp
/*	Translation: the client at \fIaddress\fR sent \fIcount\fR
/*	bytes before its turn to speak, and this happened \fItime\fR
/*	seconds after the test started. The \fItext\fR is what the
/*	client sent (truncated at 100 bytes, and with non-printable
/*	characters replaced with "?").
/*
/*	The postscreen_greet_action parameter specifies the action
/*	that is taken next:
/* .IP "\fBcontinue\fR (default, observation mode)"
/*	Wait until the postscreen_greet_wait time has elapsed, then
/*	report DNSBL lookup results if applicable. Either perform
/*	DNSBL-related actions or forward the connection to a real
/*	SMTP server process.
/* .IP "\fBdrop\fR (enforcement mode)"
/*	Drop the connection immediately with a 521 SMTP reply.
/*	In a future implementation, the connection may instead be passed
/*	to a dummy SMTP protocol engine that logs sender and recipient
/*	information.
/* HANGUP TEST
/* .ad
/* .fi
/*	When the SMTP client hangs up without sending any data
/*	before the postscreen_greet_wait time has elapsed, this is
/*	logged as:
/* .sp
/* .nf
/*	\fBHANGUP after \fItime \fBfrom \fIaddress\fR
/* .fi
/* .sp
/*	The postscreen_hangup_action specifies the action
/*	that is taken next:
/* .IP "\fBcontinue\fR (default, observation mode)"
/*	Wait until the postscreen_greet_wait time has elapsed, then
/*	report DNSBL lookup results if applicable. Do not forward
/*	the broken connection to a real SMTP server process.
/* .IP "\fBdrop\fR (enforcement mode)"
/*	Drop the connection immediately.
/* DNS BLOCKLIST TEST
/* .ad
/* .fi
/*	The postscreen_dnsbl_sites parameter (default: empty)
/*	specifies a list of DNS blocklist servers. When the
/*	postscreen_greet_wait time has elapsed, and the SMTP client
/*	address is reported by at least one of these blocklists,
/*	this is logged as:
/* .sp
/* .nf
/*	\fBDNSBL rank \fIcount \fBfor \fIaddress\fR
/* .fi
/* .sp
/*	Translation: the client at \fIaddress\fR is listed with
/*	\fIcount\fR DNSBL servers. The \fIcount\fR does not
/*	depend on the number of DNS records that an individual DNSBL
/*	server returns.
/*
/*	The postscreen_dnsbl_action parameter specifies the action
/*	that is taken next:
/* .IP "\fBcontinue\fR (default, observation mode)"
/*	Forward the connection to a real SMTP server process.
/* .IP "\fBdrop\fR (enforcement mode)"
/*	Drop the connection immediately with a 521 SMTP reply.
/*	In a future implementation, the connection may instead be passed
/*	to a dummy SMTP protocol engine that logs sender and recipient
/*	information.
/* SECURITY
/* .ad
/* .fi
/*	The \fBpostscreen\fR(8) server is moderately security-sensitive.
/*	It talks to untrusted clients on the network. The process
/*	can be run chrooted at fixed low privilege.
/* STANDARDS
/*	RFC 5321 (SMTP, including multi-line 220 greetings)
/*	RFC 2920 (SMTP Pipelining)
/* DIAGNOSTICS
/*	Problems and transactions are logged to \fBsyslogd\fR(8).
/* CONFIGURATION PARAMETERS
/* .ad
/* .fi
/*	Changes to main.cf are not picked up automatically, as
/*	\fBpostscreen\fR(8) processes may run for several hours.
/*	Use the command "postfix reload" after a configuration
/*	change.
/*
/*	The text below provides only a parameter summary. See
/*	\fBpostconf\fR(5) for more details including examples.
/* TRIAGE PARAMETERS
/* .ad
/* .fi
/* .IP "\fBpostscreen_blacklist_action (continue)\fR"
/*	The action that \fBpostscreen\fR(8) takes when an SMTP client is
/*	permanently blacklisted with the postscreen_blacklist_networks
/*	parameter.
/* .IP "\fBpostscreen_blacklist_networks (empty)\fR"
/*	Network addresses that are permanently blacklisted; see the
/*	postscreen_blacklist_action parameter for possible actions.
/* .IP "\fBpostscreen_cache_map (btree:$data_directory/ps_whitelist)\fR"
/*	Persistent storage for the \fBpostscreen\fR(8) server decisions.
/* .IP "\fBpostscreen_cache_ttl (1d)\fR"
/*	The amount of time that \fBpostscreen\fR(8) will cache a decision for
/*	a specific SMTP client IP address.
/* .IP "\fBpostscreen_dnsbl_action (continue)\fR"
/*	The action that \fBpostscreen\fR(8) takes when an SMTP client is listed
/*	at the DNS blocklist domains specified with the postscreen_dnsbl_sites
/*	parameter.
/* .IP "\fBpostscreen_dnsbl_sites (empty)\fR"
/*	Optional list of DNS blocklist domains.
/* .IP "\fBpostscreen_greet_action (continue)\fR"
/*	The action that \fBpostscreen\fR(8) takes when an SMTP client speaks
/*	before its turn within the time specified with the postscreen_greet_wait
/*	parameter.
/* .IP "\fBpostscreen_greet_banner ($smtpd_banner)\fR"
/*	The text in the optional "220-text..." server response that
/*	\fBpostscreen\fR(8) sends ahead of the real Postfix SMTP server's "220
/*	text..." response, in an attempt to confuse bad SMTP clients so
/*	that they speak before their turn (pre-greet).
/* .IP "\fBpostscreen_greet_wait (4s)\fR"
/*	The amount of time that \fBpostscreen\fR(8) will wait for an SMTP
/*	client to send a command before its turn, and for DNS blocklist
/*	lookup results to arrive.
/* .IP "\fBpostscreen_hangup_action (continue)\fR"
/*	The action that \fBpostscreen\fR(8) takes when an SMTP client disconnects
/*	without sending data, within the time specified with the
/*	postscreen_greet_wait parameter.
/* .IP "\fBpostscreen_post_queue_limit ($default_process_limit)\fR"
/*	The number of clients that can be waiting for service from a
/*	real SMTP server process.
/* .IP "\fBpostscreen_pre_queue_limit ($default_process_limit)\fR"
/*	The number of non-whitelisted clients that can be waiting for
/*	a decision whether they will receive service from a real SMTP server
/*	process.
/* .IP "\fBpostscreen_whitelist_networks ($mynetworks)\fR"
/*	Network addresses that are permanently whitelisted, and that
/*	will not be subjected to \fBpostscreen\fR(8) checks.
/* .IP "\fBsmtpd_service (smtpd)\fR"
/*	The internal service that \fBpostscreen\fR(8) forwards allowed
/*	connections to.
/* MISCELLANEOUS CONTROLS
/* .ad
/* .fi
/* .IP "\fBconfig_directory (see 'postconf -d' output)\fR"
/*	The default location of the Postfix main.cf and master.cf
/*	configuration files.
/* .IP "\fBdaemon_timeout (18000s)\fR"
/*	How much time a Postfix daemon process may take to handle a
/*	request before it is terminated by a built-in watchdog timer.
/* .IP "\fBdelay_logging_resolution_limit (2)\fR"
/*	The maximal number of digits after the decimal point when logging
/*	sub-second delay values.
/* .IP "\fBcommand_directory (see 'postconf -d' output)\fR"
/*	The location of all postfix administrative commands.
/* .IP "\fBipc_timeout (3600s)\fR"
/*	The time limit for sending or receiving information over an internal
/*	communication channel.
/* .IP "\fBmax_idle (100s)\fR"
/*	The maximum amount of time that an idle Postfix daemon process waits
/*	for an incoming connection before terminating voluntarily.
/* .IP "\fBprocess_id (read-only)\fR"
/*	The process ID of a Postfix command or daemon process.
/* .IP "\fBprocess_name (read-only)\fR"
/*	The process name of a Postfix command or daemon process.
/* .IP "\fBsyslog_facility (mail)\fR"
/*	The syslog facility of Postfix logging.
/* .IP "\fBsyslog_name (see 'postconf -d' output)\fR"
/*	The mail system name that is prepended to the process name in syslog
/*	records, so that "smtpd" becomes, for example, "postfix/smtpd".
/* SEE ALSO
/*	smtpd(8), Postfix SMTP server
/*	dnsblog(8), temporary DNS helper
/*	syslogd(8), system logging
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
#include <sys/stat.h>
#include <stdlib.h>

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

#ifdef MISSING_STRTOUL
#define strtoul strtol
#endif

/* Utility library. */

#include <msg.h>
#include <connect.h>
#include <iostuff.h>
#include <events.h>
#include <mymalloc.h>
#include <myaddrinfo.h>
#include <dict.h>
#include <sane_accept.h>
#include <stringops.h>
#include <set_eugid.h>
#include <vstream.h>
#include <format_tv.h>
#include <htable.h>
#include <name_code.h>

/* Global library. */

#include <mail_conf.h>
#include <mail_params.h>
#include <mail_version.h>
#include <mail_proto.h>
#include <addr_match_list.h>

/* Master server protocols. */

#include <mail_server.h>

 /*
  * Configuration parameters.
  */
char   *var_smtpd_service;
char   *var_smtpd_banner;
char   *var_ps_cache_map;
int     var_ps_post_queue_limit;
int     var_ps_pre_queue_limit;
int     var_proc_limit;
int     var_ps_cache_ttl;
int     var_ps_greet_wait;
char   *var_ps_dnsbl_sites;
char   *var_ps_dnsbl_action;
char   *var_ps_greet_action;
char   *var_ps_hangup_action;
char   *var_ps_wlist_nets;
char   *var_ps_blist_nets;
char   *var_ps_greet_banner;
char   *var_ps_blist_action;

 /*
  * Per-session state. See also: new_session_state() and free_event_state()
  * below.
  */
typedef struct {
    int     flags;			/* see below */
    VSTREAM *smtp_client_stream;	/* remote SMTP client */
    int     smtp_server_fd;		/* real SMTP server */
    char   *smtp_client_addr;		/* client address */
    char   *smtp_client_port;		/* client port */
    struct timeval creation_time;	/* as the name says */
} PS_STATE;

#define PS_FLAG_NOCACHE		(1<<0)	/* don't store this client */
#define PS_FLAG_NOFORWARD	(1<<1)	/* don't forward this session */
#define PS_FLAG_EXPIRED		(1<<2)	/* whitelist expired */
#define PS_FLAG_WHITELISTED	(1<<3)	/* whitelisted (temp or perm) */

 /*
  * This program screens all inbound SMTP connections, so it better not waste
  * time.
  */
#define PS_GREET_TIMEOUT		5
#define PS_SMTP_WRITE_TIMEOUT		1
#define PS_SEND_SOCK_CONNECT_TIMEOUT	1
#define PS_SEND_SOCK_NOTIFY_TIMEOUT	100

#define PS_READ_BUF_SIZE		1024

#define DNSBL_SERVICE			"dnsblog"
#define DNSBLOG_TIMEOUT			10

#define PS_ACT_DROP			1
#define PS_ACT_CONT			2

static int check_queue_length;		/* connections being checked */
static int post_queue_length;		/* being sent to real SMTPD */
static DICT *cache_map;			/* cache table handle */
static VSTRING *temp;			/* scratchpad */
static char *smtp_service_name;		/* path to real SMTPD */
static char *teaser_greeting;		/* spamware teaser banner */
static ARGV *dnsbl_sites;		/* dns blocklist domains */
static VSTRING *reply_addr;		/* address in DNSBL reply */
static VSTRING *reply_domain;		/* domain in DNSBL reply */
static HTABLE *dnsbl_cache;		/* entries being queried */
static int dnsbl_action;		/* PS_ACT_DROP or PS_ACT_CONT */
static int greet_action;		/* PS_ACT_DROP or PS_ACT_CONT */
static int hangup_action;		/* PS_ACT_DROP or PS_ACT_CONT */
static ADDR_MATCH_LIST *wlist_nets;	/* permanently whitelisted networks */
static ADDR_MATCH_LIST *blist_nets;	/* permanently blacklisted networks */
static int blist_action;		/* PS_ACT_DROP or PS_ACT_CONT */

 /*
  * See log_adhoc.c for discussion.
  */
typedef struct {
    int     dt_sec;			/* make sure it's signed */
    int     dt_usec;			/* make sure it's signed */
} DELTA_TIME;

#define DELTA(x, y, z) \
    do { \
	(x).dt_sec = (y).tv_sec - (z).tv_sec; \
	(x).dt_usec = (y).tv_usec - (z).tv_usec; \
	while ((x).dt_usec < 0) { \
	    (x).dt_usec += 1000000; \
	    (x).dt_sec -= 1; \
	} \
	while ((x).dt_usec >= 1000000) { \
	    (x).dt_usec -= 1000000; \
	    (x).dt_sec += 1; \
	} \
	if ((x).dt_sec < 0) \
	    (x).dt_sec = (x).dt_usec = 0; \
    } while (0)

#define SIG_DIGS        2

/* READ_EVENT_REQUEST - prepare for transition to next state */

#define READ_EVENT_REQUEST(fd, action, context, timeout) do { \
    if (msg_verbose) msg_info("%s: read-request fd=%d", myname, (fd)); \
    event_enable_read((fd), (action), (context)); \
    event_request_timer((action), (context), (timeout)); \
} while (0)

/* CLEAR_EVENT_REQUEST - complete state transition */

#define CLEAR_EVENT_REQUEST(fd, action, context) do { \
    if (msg_verbose) msg_info("%s: clear-request fd=%d", myname, (fd)); \
    event_disable_readwrite(fd); \
    event_cancel_timer((action), (context)); \
} while (0)

/* SLMs. */

#define STR(x)	vstring_str(x)
#define LEN(x)	VSTRING_LEN(x)

 /*
  * DNSBL lookup status per client IP address.
  */
typedef struct {
    int     dnsbl_count;		/* is this address listed */
    int     refcount;			/* query reference count */
} PS_DNSBL_ENTRY;

/* postscreen_dnsbl_entry_create - create blocklist cache entry */

static PS_DNSBL_ENTRY *postscreen_dnsbl_entry_create(void)
{
    PS_DNSBL_ENTRY *entry;

    entry = (PS_DNSBL_ENTRY *) mymalloc(sizeof(*entry));
    entry->dnsbl_count = 0;
    entry->refcount = 0;
    return (entry);
}

/* postscreen_dnsbl_done - get blocklist cache entry, decrement refcount */

static int postscreen_dnsbl_done(const char *addr)
{
    const char *myname = "postscreen_dnsbl_done";
    PS_DNSBL_ENTRY *entry;
    int     dnsbl_count;

    /*
     * Sanity check.
     */
    if ((entry = (PS_DNSBL_ENTRY *) htable_find(dnsbl_cache, addr)) == 0)
	msg_panic("%s: no blocklist cache entry for %s", myname, addr);

    /*
     * Yes, cache reads are destructive.
     */
    dnsbl_count = entry->dnsbl_count;
    entry->refcount -= 1;
    if (entry->refcount < 1) {
	if (msg_verbose)
	    msg_info("%s: delete cache entry for %s", myname, addr);
	htable_delete(dnsbl_cache, addr, myfree);
    }
    return (dnsbl_count);
}

/* postscreen_dnsbl_reply - receive dnsbl reply, update blocklist cache entry */

static void postscreen_dnsbl_reply(int event, char *context)
{
    const char *myname = "postscreen_dnsbl_reply";
    VSTREAM *stream = (VSTREAM *) context;
    PS_DNSBL_ENTRY *entry;
    int     dnsbl_count;

    CLEAR_EVENT_REQUEST(vstream_fileno(stream), postscreen_dnsbl_reply, context);

    /*
     * Later, this will become an UDP-based DNS client that is built directly
     * into the postscreen daemon.
     * 
     * Don't panic when no blocklist cache entry exists. It may be gone when the
     * client triggered a "drop" action after pregreet, DNSBL lookup, or
     * hangup.
     */
    if (event == EVENT_READ
	&& attr_scan(stream,
		     ATTR_FLAG_MORE | ATTR_FLAG_STRICT,
		     ATTR_TYPE_STR, MAIL_ATTR_RBL_DOMAIN, reply_domain,
		     ATTR_TYPE_STR, MAIL_ATTR_ADDR, reply_addr,
		     ATTR_TYPE_INT, MAIL_ATTR_STATUS, &dnsbl_count,
		     ATTR_TYPE_END) == 3) {
	if ((entry = (PS_DNSBL_ENTRY *)
	     htable_find(dnsbl_cache, STR(reply_addr))) != 0)
	    entry->dnsbl_count += dnsbl_count;
    }
    vstream_fclose(stream);
}

/* postscreen_dnsbl_query  - send dnsbl query */

static void postscreen_dnsbl_query(const char *addr)
{
    const char *myname = "postscreen_dnsbl_query";
    int     fd;
    VSTREAM *stream;
    char  **cpp;
    PS_DNSBL_ENTRY *entry;

    /*
     * Avoid duplicate effort when this lookup is already in progress. Now,
     * we destroy the entry when the client replies. Later, we increment
     * refcounts with queries sent, and decrement refcounts with replies
     * received, so we can maintain state even after a client talks early,
     * and update the external cache asynchronously.
     */
    if ((entry = (PS_DNSBL_ENTRY *) htable_find(dnsbl_cache, addr)) != 0) {
	entry->refcount += 1;
	return;
    }
    if (msg_verbose)
	msg_info("%s: create cache entry for %s", myname, addr);
    entry = postscreen_dnsbl_entry_create();
    (void) htable_enter(dnsbl_cache, addr, (char *) entry);
    entry->refcount = 1;

    /*
     * Later, this will become an UDP-based DNS client that is built directly
     * into the postscreen daemon.
     */
    for (cpp = dnsbl_sites->argv; *cpp; cpp++) {
	if ((fd = LOCAL_CONNECT("private/" DNSBL_SERVICE, NON_BLOCKING, 1)) < 0) {
	    msg_warn("%s: connect to " DNSBL_SERVICE " service: %m", myname);
	    return;
	}
	stream = vstream_fdopen(fd, O_RDWR);
	attr_print(stream, ATTR_FLAG_NONE,
		   ATTR_TYPE_STR, MAIL_ATTR_RBL_DOMAIN, *cpp,
		   ATTR_TYPE_STR, MAIL_ATTR_ADDR, addr,
		   ATTR_TYPE_END);
	if (vstream_fflush(stream) != 0) {
	    msg_warn("%s: error sending to " DNSBL_SERVICE " service: %m", myname);
	    vstream_fclose(stream);
	    return;
	}
	READ_EVENT_REQUEST(vstream_fileno(stream), postscreen_dnsbl_reply,
			   (char *) stream, DNSBLOG_TIMEOUT);
    }
}

/* new_session_state - fill in connection state for event processing */

static PS_STATE *new_session_state(VSTREAM *stream, const char *addr,
				           const char *port)
{
    PS_STATE *state;

    state = (PS_STATE *) mymalloc(sizeof(*state));
    state->flags = 0;
    if ((state->smtp_client_stream = stream) != 0)
	check_queue_length++;
    state->smtp_server_fd = (-1);
    state->smtp_client_addr = mystrdup(addr);
    state->smtp_client_port = mystrdup(port);
    GETTIMEOFDAY(&state->creation_time);
    return (state);
}

/* free_session_state - destroy connection state including connections */

static void free_session_state(PS_STATE *state)
{
    if (state->smtp_client_stream != 0) {
	event_server_disconnect(state->smtp_client_stream);
	check_queue_length--;
    }
    if (state->smtp_server_fd >= 0) {
	close(state->smtp_server_fd);
	post_queue_length--;
    }
    myfree(state->smtp_client_addr);
    myfree(state->smtp_client_port);
    myfree((char *) state);

    if (check_queue_length < 0 || post_queue_length < 0)
	msg_panic("bad queue length: check_queue=%d, post_queue=%d",
		  check_queue_length, post_queue_length);
}

/* mydelta_time - pretty-formatted delta time */

static char *mydelta_time(VSTRING *buf, struct timeval tv, int *delta)
{
    DELTA_TIME pdelay;
    struct timeval now;

    GETTIMEOFDAY(&now);
    DELTA(pdelay, now, tv);
    VSTRING_RESET(buf);
    format_tv(buf, pdelay.dt_sec, pdelay.dt_usec, SIG_DIGS, var_delay_max_res);
    *delta = pdelay.dt_sec;
    return (STR(buf));
}

/* smtp_reply - reply to the client */

static int smtp_reply(int smtp_client_fd, const char *smtp_client_addr,
		              const char *smtp_client_port, const char *text)
{
    int     ret;

    /*
     * XXX Need to make sure that the TCP send buffer is large enough for any
     * response, so that a nasty client can't cause this process to block.
     */
    ret = (write_buf(smtp_client_fd, text, strlen(text), 1) < 0);
    if (ret != 0 && errno != EPIPE)
	msg_warn("write %s:%s: %m", smtp_client_addr, smtp_client_port);
    return (ret);
}

/* send_socket_close_event - file descriptor has arrived or timeout */

static void send_socket_close_event(int event, char *context)
{
    const char *myname = "send_socket_close_event";
    PS_STATE *state = (PS_STATE *) context;

    if (msg_verbose)
	msg_info("%s: sq=%d cq=%d event %d on send socket %d from %s:%s",
		 myname, post_queue_length, check_queue_length,
		 event, state->smtp_server_fd, state->smtp_client_addr,
		 state->smtp_client_port);

    /*
     * The real SMTP server has closed the local IPC channel, or we have
     * reached the limit of our patience. In the latter case it is still
     * possible that the real SMTP server will receive the socket so we
     * should not interfere.
     */
    CLEAR_EVENT_REQUEST(state->smtp_server_fd, send_socket_close_event, context);

    switch (event) {
    case EVENT_TIME:
	msg_warn("timeout sending connection to service %s", smtp_service_name);
	break;
    default:
	break;
    }
    free_session_state(state);
}

/* send_socket - send socket to real smtpd */

static void send_socket(PS_STATE *state)
{
    const char *myname = "send_socket";
    int     window_size;

    if (msg_verbose)
	msg_info("%s: sq=%d cq=%d send socket %d from %s:%s",
		 myname, post_queue_length, check_queue_length,
	 vstream_fileno(state->smtp_client_stream), state->smtp_client_addr,
		 state->smtp_client_port);

    /*
     * This is where we would adjust the window size to a value that is
     * appropriate for this client class.
     */
#if 0
    window_size = 65535;
    if (setsockopt(vstream_fileno(state->smtp_client_stream), SOL_SOCKET, SO_RCVBUF,
		   (char *) &window_size, sizeof(window_size)) < 0)
	msg_warn("setsockopt SO_RCVBUF %d: %m", window_size);
#endif

    /*
     * Connect to the real SMTP service over a local IPC channel, send the
     * file descriptor, and close the file descriptor to save resources.
     * Experience has shown that some systems will discard information when
     * we close a channel immediately after writing. Thus, we waste resources
     * waiting for the remote side to close the local IPC channel first. The
     * good side of waiting is that we learn when the real SMTP server is
     * falling behind.
     * 
     * This is where we would forward the connection to an SMTP server that
     * provides an appropriate level of service for this client class. For
     * example, a server that is more forgiving, or one that is more
     * suspicious. Alternatively, we could send attributes along with the
     * socket with client reputation information, making everything even more
     * Postfix-specific.
     */
    if ((state->smtp_server_fd = LOCAL_CONNECT(smtp_service_name, NON_BLOCKING,
				       PS_SEND_SOCK_CONNECT_TIMEOUT)) < 0) {
	msg_warn("cannot connect to service %s: %m", smtp_service_name);
	smtp_reply(vstream_fileno(state->smtp_client_stream),
		   state->smtp_client_addr, state->smtp_client_port,
		   "421 4.3.2 All server ports are busy\r\n");
	free_session_state(state);
	return;
    }
    post_queue_length++;
    if (LOCAL_SEND_FD(state->smtp_server_fd,
		      vstream_fileno(state->smtp_client_stream)) < 0) {
	msg_warn("cannot pass connection to service %s: %m", smtp_service_name);
	smtp_reply(vstream_fileno(state->smtp_client_stream), state->smtp_client_addr,
	      state->smtp_client_port, "421 4.3.2 No system resources\r\n");
	free_session_state(state);
	return;
    } else {

	/*
	 * Closing the smtp_client_fd here triggers a FreeBSD 7.1 kernel bug
	 * where smtp-source sometimes sees the connection being closed after
	 * it has already received the real SMTP server's 220 greeting!
	 */
#if 0
	event_server_disconnect(state->smtp_client_stream);
	state->smtp_client_stream = 0;
	check_queue_length--;
#endif
	READ_EVENT_REQUEST(state->smtp_server_fd, send_socket_close_event,
			   (char *) state, PS_SEND_SOCK_NOTIFY_TIMEOUT);
	return;
    }
}

/* smtp_read_event - handle pre-greet, EOF or timeout. */

static void smtp_read_event(int event, char *context)
{
    const char *myname = "smtp_read_event";
    PS_STATE *state = (PS_STATE *) context;
    char    read_buf[PS_READ_BUF_SIZE];
    int     read_count;
    int     dnsbl_count;
    int     elapsed;
    int     action;

    if (msg_verbose)
	msg_info("%s: sq=%d cq=%d event %d on smtp socket %d from %s:%s",
		 myname, post_queue_length, check_queue_length,
		 event, vstream_fileno(state->smtp_client_stream),
		 state->smtp_client_addr, state->smtp_client_port);

    /*
     * Either the remote SMTP client spoke before its turn, the connection
     * was closed, or we reached the limit of our patience.
     */
    CLEAR_EVENT_REQUEST(vstream_fileno(state->smtp_client_stream),
			smtp_read_event, context);

    /*
     * If this session ends here, we MUST read the blocklist cache otherwise
     * we have a memory leak.
     */
    switch (event) {

	/*
	 * The SMTP client did not speak before its turn. If it is DNS
	 * blocklisted, drop the connection, or continue and forward the
	 * connection to the real SMTP server.
	 * 
	 * This is where we would use a dummy SMTP protocol engine to further
	 * investigate whether the client cuts corners in the protocol,
	 * before allowing it to talk to a real SMTP server process.
	 */
    case EVENT_TIME:
	if (*var_ps_dnsbl_sites)
	    dnsbl_count = postscreen_dnsbl_done(state->smtp_client_addr);
	else
	    dnsbl_count = 0;
	if (dnsbl_count > 0) {
	    msg_info("DNSBL rank %d for %s",
		     dnsbl_count, state->smtp_client_addr);
	    if (dnsbl_action == PS_ACT_DROP) {
		smtp_reply(vstream_fileno(state->smtp_client_stream),
			   state->smtp_client_addr, state->smtp_client_port,
			   "521 5.7.1 Blocked by DNSBL\r\n");
		state->flags |= PS_FLAG_NOFORWARD;
	    }
	    state->flags |= PS_FLAG_NOCACHE;
	}
	if (state->flags & PS_FLAG_NOFORWARD) {
	    free_session_state(state);
	} else {
	    if ((state->flags & PS_FLAG_NOCACHE) == 0) {
		msg_info("PASS %s %s", (state->flags & PS_FLAG_EXPIRED) ?
			 "OLD" : "NEW", state->smtp_client_addr);
		if (cache_map != 0) {
		    vstring_sprintf(temp, "%ld", (long) event_time());
		    dict_put(cache_map, state->smtp_client_addr, STR(temp));
		}
	    }
	    send_socket(state);
	}
	break;

	/*
	 * EOF or the client spoke before its turn. We simply drop the
	 * connection, or we continue waiting and allow DNS replies to
	 * trickle in.
	 * 
	 * This is where we would use a dummy SMTP protocol engine to obtain the
	 * sender and recipient information before dropping a pregreeter's
	 * connection.
	 */
    default:
	if ((read_count = recv(vstream_fileno(state->smtp_client_stream),
			   read_buf, sizeof(read_buf) - 1, MSG_PEEK)) > 0) {
	    read_buf[read_count] = 0;
	    msg_info("PREGREET %d after %s from %s: %.100s", read_count,
		     mydelta_time(temp, state->creation_time, &elapsed),
		     state->smtp_client_addr, printable(read_buf, '?'));
	    action = greet_action;
	    if (action == PS_ACT_DROP)
		smtp_reply(vstream_fileno(state->smtp_client_stream),
			   state->smtp_client_addr, state->smtp_client_port,
			   "521 5.5.1 Protocol error\r\n");
	} else {
	    msg_info("HANGUP after %s from %s",
		     mydelta_time(temp, state->creation_time, &elapsed),
		     state->smtp_client_addr);
	    action = hangup_action;
	    state->flags |= PS_FLAG_NOFORWARD;
	}
	if (action == PS_ACT_DROP) {
	    if (*var_ps_dnsbl_sites)
		(void) postscreen_dnsbl_done(state->smtp_client_addr);
	    free_session_state(state);
	} else {
	    state->flags |= PS_FLAG_NOCACHE;
	    /* not: postscreen_dnsbl_done */
	    if (elapsed > var_ps_greet_wait)
		elapsed = var_ps_greet_wait;
	    event_request_timer(smtp_read_event, context,
				var_ps_greet_wait - elapsed);
	}
	break;
    }
}

/* postscreen_drain - delayed exit after "postfix reload" */

static void postscreen_drain(char *unused_service, char **unused_argv)
{
    int     count;

    /*
     * After "postfix reload", complete work-in-progress in the background,
     * instead of dropping already-accepted connections on the floor.
     * 
     * Unfortunately we must close all writable tables, so we can't store or
     * look up reputation information. The reason is that don't have any
     * multi-writer safety guarantees. We also can't use the single-writer
     * proxywrite service, because its latency guarantees are too weak.
     * 
     * All error retry counts shall be limited. Instead of blocking here, we
     * could retry failed fork() operations in the event call-back routines,
     * but we don't need perfection. The host system is severely overloaded
     * and service levels are already way down.
     */
    for (count = 0; /* see below */ ; count++) {
	if (count >= 5) {
	    msg_fatal("fork: %m");
	} else if (event_server_drain() != 0) {
	    msg_warn("fork: %m");
	    sleep(1);
	    continue;
	} else {
	    if (cache_map != 0) {
		dict_close(cache_map);
		cache_map = 0;
	    }
	    return;
	}
    }
}

/* postscreen_service - handle new client connection */

static void postscreen_service(VSTREAM *smtp_client_stream,
			               char *unused_service,
			               char **unused_argv)
{
    const char *myname = "postscreen_service";
    PS_STATE *state;
    struct sockaddr_storage addr_storage;
    SOCKADDR_SIZE addr_storage_len = sizeof(addr_storage);
    MAI_HOSTADDR_STR smtp_client_addr;
    MAI_SERVPORT_STR smtp_client_port;
    int     aierr;
    const char *stamp_str;
    time_t  stamp_time;
    int     window_size;
    int     state_flags = 0;

    /*
     * This program handles all incoming connections, so it must not block.
     * We use event-driven code for all operations that introduce latency.
     * 
     * We use the event_server framework. This means we get already-accepted
     * connections wrapped into a VSTREAM so we have to invoke getpeername()
     * to find out the remote address and port.
     */

#define PS_SERVICE_GIVEUP_AND_RETURN(stream) do { \
	event_server_disconnect(stream); \
	return; \
    } while (0);

    /*
     * Look up the remote SMTP client address and port.
     */
    if (getpeername(vstream_fileno(smtp_client_stream), (struct sockaddr *)
		    & addr_storage, &addr_storage_len) < 0) {
	msg_warn("getpeername: %m");
	smtp_reply(vstream_fileno(smtp_client_stream),
		   "unknown_address", "unknown_port",
		   "421 4.3.2 No system resources\r\n");
	PS_SERVICE_GIVEUP_AND_RETURN(smtp_client_stream);
    }

    /*
     * Convert the remote SMTP client address and port to printable form for
     * logging and access control.
     */
    if ((aierr = sockaddr_to_hostaddr((struct sockaddr *) & addr_storage,
				      addr_storage_len, &smtp_client_addr,
				      &smtp_client_port, 0)) != 0) {
	msg_warn("cannot convert client address/port to string: %s",
		 MAI_STRERROR(aierr));
	smtp_reply(vstream_fileno(smtp_client_stream),
		   "unknown_address", "unknown_port",
		   "421 4.3.2 No system resources\r\n");
	PS_SERVICE_GIVEUP_AND_RETURN(smtp_client_stream);
    }
    if (strncasecmp("::ffff:", smtp_client_addr.buf, 7) == 0)
	memmove(smtp_client_addr.buf, smtp_client_addr.buf + 7,
		sizeof(smtp_client_addr.buf) - 7);
    if (msg_verbose)
	msg_info("%s: sq=%d cq=%d connect from %s:%s",
		 myname, post_queue_length, check_queue_length,
		 smtp_client_addr.buf, smtp_client_port.buf);

    /*
     * Reply with 421 when we can't forward more connections.
     */
    if (var_ps_post_queue_limit > 0
	&& post_queue_length >= var_ps_post_queue_limit) {
	msg_info("reject: connect from %s:%s: all server ports busy",
		 smtp_client_addr.buf, smtp_client_port.buf);
	smtp_reply(vstream_fileno(smtp_client_stream),
		   smtp_client_addr.buf, smtp_client_port.buf,
		   "421 4.3.2 All server ports are busy\r\n");
	PS_SERVICE_GIVEUP_AND_RETURN(smtp_client_stream);
    }

    /*
     * The permanent blacklist has first precedence. If the client is
     * permanently blacklisted, send some generic reply and hang up
     * immediately, or torture them a little longer.
     */
    if (blist_nets != 0
	&& addr_match_list_match(blist_nets, smtp_client_addr.buf) != 0) {
	msg_info("BLACKLISTED %s", smtp_client_addr.buf);
	if (blist_action == PS_ACT_DROP) {
	    smtp_reply(vstream_fileno(smtp_client_stream),
		       smtp_client_addr.buf, smtp_client_port.buf,
		       "521 5.3.2 Service currently not available\r\n");
	    PS_SERVICE_GIVEUP_AND_RETURN(smtp_client_stream);
	}
    }

    /*
     * The permanent whitelist has second precedence.
     */
    else if (wlist_nets != 0
	  && addr_match_list_match(wlist_nets, smtp_client_addr.buf) != 0) {
	msg_info("WHITELISTED %s", smtp_client_addr.buf);
	state_flags |= PS_FLAG_WHITELISTED;
    }

    /*
     * Finally, the temporary whitelist (i.e. the postscreen cache) has the
     * lowest precedence.
     */
    else if (cache_map != 0
	  && (stamp_str = dict_get(cache_map, smtp_client_addr.buf)) != 0) {
	stamp_time = strtoul(stamp_str, 0, 10);
	if (stamp_time > event_time() - var_ps_cache_ttl) {
	    msg_info("PASS OLD %s", smtp_client_addr.buf);
	    state_flags |= PS_FLAG_WHITELISTED;
	} else
	    state_flags |= PS_FLAG_EXPIRED;
    }

    /*
     * If the client is permanently or temporarily whitelisted, send the
     * socket to the real SMTP service and get out of the way.
     */
    if (state_flags & PS_FLAG_WHITELISTED) {
	state = new_session_state(smtp_client_stream, smtp_client_addr.buf,
				  smtp_client_port.buf);
	send_socket(state);
	return;
    }

    /*
     * Reply with 421 when we can't analyze more connections.
     */
    if (var_ps_pre_queue_limit > 0
      && check_queue_length - post_queue_length >= var_ps_pre_queue_limit) {
	msg_info("reject: connect from %s:%s: all screening ports busy",
		 smtp_client_addr.buf, smtp_client_port.buf);
	smtp_reply(vstream_fileno(smtp_client_stream),
		   smtp_client_addr.buf, smtp_client_port.buf,
		   "421 4.3.2 All screening ports are busy\r\n");
	PS_SERVICE_GIVEUP_AND_RETURN(smtp_client_stream);
    }

    /*
     * If the client has no cached decision, send half the greeting banner,
     * by way of teaser, then wait briefly to see if the client speaks before
     * its turn.
     * 
     * This is where we would do DNS blocklist lookup in the background, and
     * cancel the lookup when the client takes action first.
     * 
     * Before sending the banner we could set the TCP window to the smallest
     * possible value to save some network bandwidth, at least with spamware
     * that waits until the server starts speaking.
     */
#if 0
    window_size = 1;
    if (setsockopt(vstream_fileno(smtp_client_stream), SOL_SOCKET, SO_RCVBUF,
		   (char *) &window_size, sizeof(window_size)) < 0)
	msg_warn("setsockopt SO_RCVBUF %d: %m", window_size);
#endif
    if (teaser_greeting != 0
     && smtp_reply(vstream_fileno(smtp_client_stream), smtp_client_addr.buf,
		   smtp_client_port.buf, teaser_greeting) != 0)
	PS_SERVICE_GIVEUP_AND_RETURN(smtp_client_stream);

    state = new_session_state(smtp_client_stream, smtp_client_addr.buf,
			      smtp_client_port.buf);
    state->flags |= state_flags;
    READ_EVENT_REQUEST(vstream_fileno(state->smtp_client_stream),
		       smtp_read_event, (char *) state, var_ps_greet_wait);

    /*
     * Run a DNS blocklist query while we wait for the client to respond.
     */
    if (*var_ps_dnsbl_sites)
	postscreen_dnsbl_query(smtp_client_addr.buf);
}

/* pre_jail_init - pre-jail initialization */

static void pre_jail_init(char *unused_name, char **unused_argv)
{

    /*
     * Open read-only maps as before dropping privilege, for consistency with
     * other Postfix daemons.
     */
    if (*var_ps_wlist_nets)
	wlist_nets =
	addr_match_list_init(MATCH_FLAG_NONE, var_ps_wlist_nets);

    if (*var_ps_blist_nets)
	blist_nets =
	    addr_match_list_init(MATCH_FLAG_NONE, var_ps_blist_nets);

    /*
     * Never, ever, get killed by a master signal, as that would corrupt the
     * database when we're in the middle of an update.
     */
    if (setsid() < 0)
	msg_warn("setsid: %m");

    /*
     * Security: don't create root-owned files that contain untrusted data.
     * And don't create Postfix-owned files in root-owned directories,
     * either. We want a correct relationship between (file or directory)
     * ownership and (file or directory) content. To open files before going
     * to jail, temporarily drop root privileges.
     */
    SAVE_AND_SET_EUGID(var_owner_uid, var_owner_gid);

    /*
     * Keep state in persistent external map. As a safety measure we sync the
     * database on each update. This hurts on LINUX systems that sync all
     * their dirty disk blocks whenever any application invokes fsync().
     */
#define PS_DICT_OPEN_FLAGS (DICT_FLAG_DUP_REPLACE | DICT_FLAG_SYNC_UPDATE)

    if (*var_ps_cache_map)
	cache_map = dict_open(var_ps_cache_map,
			      O_CREAT | O_RDWR,
			      PS_DICT_OPEN_FLAGS);

    /*
     * Clean up and restore privilege.
     */
    RESTORE_SAVED_EUGID();
}

/* post_jail_init - post-jail initialization */

static void post_jail_init(char *unused_name, char **unused_argv)
{
    const NAME_CODE actions[] = {
	"drop", PS_ACT_DROP,
	"continue", PS_ACT_CONT,
	0, -1,
    };

    /*
     * This routine runs after the skeleton code has entered the chroot jail.
     * Prevent automatic process suicide after a limited number of client
     * requests. It is OK to terminate after a limited amount of idle time.
     */
    var_use_limit = 0;

    /*
     * Other one-time initialization.
     */
    temp = vstring_alloc(10);
    vstring_sprintf(temp, "%s/%s", MAIL_CLASS_PRIVATE, var_smtpd_service);
    smtp_service_name = mystrdup(STR(temp));
    if (*var_ps_greet_banner) {
	vstring_sprintf(temp, "220-%s\r\n", var_ps_greet_banner);
	teaser_greeting = mystrdup(STR(temp));
    }
    dnsbl_sites = argv_split(var_ps_dnsbl_sites, ", \t\r\n");
    dnsbl_cache = htable_create(13);
    reply_addr = vstring_alloc(100);
    reply_domain = vstring_alloc(100);
    if ((blist_action = name_code(actions, NAME_CODE_FLAG_NONE,
				  var_ps_blist_action)) < 0)
	msg_fatal("bad %s value: %s", VAR_PS_BLIST_ACTION, var_ps_blist_action);
    if ((dnsbl_action = name_code(actions, NAME_CODE_FLAG_NONE,
				  var_ps_dnsbl_action)) < 0)
	msg_fatal("bad %s value: %s", VAR_PS_DNSBL_ACTION, var_ps_dnsbl_action);
    if ((greet_action = name_code(actions, NAME_CODE_FLAG_NONE,
				  var_ps_greet_action)) < 0)
	msg_fatal("bad %s value: %s", VAR_PS_GREET_ACTION, var_ps_greet_action);
    if ((hangup_action = name_code(actions, NAME_CODE_FLAG_NONE,
				   var_ps_hangup_action)) < 0)
	msg_fatal("bad %s value: %s", VAR_PS_HUP_ACTION, var_ps_hangup_action);
}

MAIL_VERSION_STAMP_DECLARE;

/* main - pass control to the multi-threaded skeleton */

int     main(int argc, char **argv)
{
    static const CONFIG_STR_TABLE str_table[] = {
	VAR_SMTPD_SERVICE, DEF_SMTPD_SERVICE, &var_smtpd_service, 1, 0,
	VAR_PS_CACHE_MAP, DEF_PS_CACHE_MAP, &var_ps_cache_map, 0, 0,
	VAR_SMTPD_BANNER, DEF_SMTPD_BANNER, &var_smtpd_banner, 1, 0,
	VAR_PS_DNSBL_SITES, DEF_PS_DNSBL_SITES, &var_ps_dnsbl_sites, 0, 0,
	VAR_PS_GREET_ACTION, DEF_PS_GREET_ACTION, &var_ps_greet_action, 1, 0,
	VAR_PS_DNSBL_ACTION, DEF_PS_DNSBL_ACTION, &var_ps_dnsbl_action, 1, 0,
	VAR_PS_HUP_ACTION, DEF_PS_HUP_ACTION, &var_ps_hangup_action, 1, 0,
	VAR_PS_WLIST_NETS, DEF_PS_WLIST_NETS, &var_ps_wlist_nets, 0, 0,
	VAR_PS_BLIST_NETS, DEF_PS_BLIST_NETS, &var_ps_blist_nets, 0, 0,
	VAR_PS_GREET_BANNER, DEF_PS_GREET_BANNER, &var_ps_greet_banner, 0, 0,
	VAR_PS_BLIST_ACTION, DEF_PS_BLIST_ACTION, &var_ps_blist_action, 1, 0,
	0,
    };
    static const CONFIG_INT_TABLE int_table[] = {
	VAR_PROC_LIMIT, DEF_PROC_LIMIT, &var_proc_limit, 1, 0,
	0,
    };
    static const CONFIG_NINT_TABLE nint_table[] = {
	VAR_PS_POST_QLIMIT, DEF_PS_POST_QLIMIT, &var_ps_post_queue_limit, 1, 0,
	VAR_PS_PRE_QLIMIT, DEF_PS_PRE_QLIMIT, &var_ps_pre_queue_limit, 1, 0,
	0,
    };
    static const CONFIG_TIME_TABLE time_table[] = {
	VAR_PS_CACHE_TTL, DEF_PS_CACHE_TTL, &var_ps_cache_ttl, 1, 0,
	VAR_PS_GREET_WAIT, DEF_PS_GREET_WAIT, &var_ps_greet_wait, 1, 0,
	0,
    };

    /*
     * Fingerprint executables and core dumps.
     */
    MAIL_VERSION_STAMP_ALLOCATE;

    event_server_main(argc, argv, postscreen_service,
		      MAIL_SERVER_STR_TABLE, str_table,
		      MAIL_SERVER_INT_TABLE, int_table,
		      MAIL_SERVER_NINT_TABLE, nint_table,
		      MAIL_SERVER_TIME_TABLE, time_table,
		      MAIL_SERVER_PRE_INIT, pre_jail_init,
		      MAIL_SERVER_POST_INIT, post_jail_init,
		      MAIL_SERVER_SOLITARY,
		      MAIL_SERVER_SLOW_EXIT, postscreen_drain,
		      0);
}
