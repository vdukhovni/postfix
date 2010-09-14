/*++
/* NAME
/*	postscreen 8
/* SUMMARY
/*	Postfix SMTP triage server
/* SYNOPSIS
/*	\fBpostscreen\fR [generic Postfix daemon options]
/* DESCRIPTION
/*	The Postfix \fBpostscreen\fR(8) server performs triage on
/*	multiple inbound SMTP connections in parallel. While a
/*	single \fBpostscreen\fR(8) process keeps spambots away from
/*	Postfix SMTP server processes, more Postfix SMTP server
/*	processes remain available for legitimate clients.
/*
/*	\fBpostscreen\fR(8) maintains a temporary whitelist for
/*	clients that have passed a number of tests.  When an SMTP
/*	client IP address is whitelisted, \fBpostscreen\fR(8) hands
/*	off the connection immediately to a Postfix SMTP server
/*	process. This minimizes the overhead for legitimate mail.
/*
/*	By default, \fBpostscreen\fR(8) logs statistics and hands
/*	off every connection to a Postfix SMTP server process, while
/*	excluding clients in mynetworks from all tests (primarily,
/*	to avoid problems with non-standard SMTP implementations
/*	in network appliances).  This mode is useful for non-destructive
/*	testing.
/*
/*	In a typical production setting, \fBpostscreen\fR(8) is
/*	configured to reject mail from clients that fail one or
/*	more tests. \fBpostscreen\fR(8) logs rejected mail with the
/*	client address, helo, sender and recipient information.
/*
/*	\fBpostscreen\fR(8) is not an SMTP proxy; this is intentional.
/*	The purpose is to keep spambots away from Postfix SMTP
/*	server processes, while minimizing overhead for legitimate
/*	traffic.
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
/* BUGS
/*	Some of the non-default protocol tests involve
/*	\fBpostscreen\fR(8)'s built-in SMTP protocol engine. When
/*	these tests succeed, \fBpostscreen\fR(8) adds the client
/*	to the temporary whitelist but it cannot not hand off the
/*	"live" connection to a Postfix SMTP server process in the
/*	middle of a session.  Instead, \fBpostscreen\fR(8) defers
/*	attempts to deliver mail with a 4XX status, and waits for
/*	the client to disconnect.  The next time a good client
/*	connects, it will be allowed to talk to a Postfix SMTP
/*	server process to deliver mail. \fBpostscreen\fR(8) mitigates
/*	the impact of this limitation by giving such tests a long
/*	expiration time.
/*
/*	The \fBpostscreen\fR(8) built-in SMTP protocol engine does
/*	not announce support for STARTTLS, AUTH, XCLIENT or XFORWARD
/*	(support for STARTTLS and AUTH may be added in the future).
/*	End-user clients should connect directly to the submission
/*	service; other systems that require the above features
/*	should directly connect to a Postfix SMTP server, or they
/*	should be placed on the \fBpostscreen\fR(8) whitelist.
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
/*
/*	NOTE: Some \fBpostscreen\fR(8)  parameters implement
/*	stress-dependent behavior.  This is supported only when the
/*	default value is stress-dependent (that is, it looks like
/*	${stress?X}${stress:Y}).  Other parameters always evaluate
/*	as if the stress value is the empty string.
/* TRIAGE PARAMETERS
/* .ad
/* .fi
/* .IP "\fBpostscreen_bare_newline_action (ignore)\fR"
/*	The action that \fBpostscreen\fR(8) takes when an SMTP client sends
/*	a bare newline character, that is, a newline not preceded by carriage
/*	return.
/* .IP "\fBpostscreen_bare_newline_enable (no)\fR"
/*	Enable "bare newline" SMTP protocol tests in the \fBpostscreen\fR(8)
/*	server.
/* .IP "\fBpostscreen_blacklist_action (ignore)\fR"
/*	The action that \fBpostscreen\fR(8) takes when an SMTP client is
/*	permanently blacklisted with the postscreen_blacklist_networks
/*	parameter.
/* .IP "\fBpostscreen_blacklist_networks (empty)\fR"
/*	Network addresses that are permanently blacklisted; see the
/*	postscreen_blacklist_action parameter for possible actions.
/* .IP "\fBpostscreen_disable_vrfy_command ($disable_vrfy_command)\fR"
/*	Disable the SMTP VRFY command in the \fBpostscreen\fR(8) daemon.
/* .IP "\fBpostscreen_dnsbl_action (ignore)\fR"
/*	The action that \fBpostscreen\fR(8) takes when an SMTP client's combined
/*	DNSBL score is equal to or greater than a threshold (as defined
/*	with the postscreen_dnsbl_sites and postscreen_dnsbl_threshold
/*	parameters).
/* .IP "\fBpostscreen_dnsbl_reply_map (empty)\fR"
/*	A mapping from actual DNSBL domain name which includes a secret
/*	password, to the DNSBL domain name that postscreen will reply with
/*	when it rejects mail.
/* .IP "\fBpostscreen_dnsbl_sites (empty)\fR"
/*	Optional list of DNS blocklist domains, filters and weight
/*	factors.
/* .IP "\fBpostscreen_dnsbl_threshold (1)\fR"
/*	The inclusive lower bound for blocking an SMTP client, based on
/*	its combined DNSBL score as defined with the postscreen_dnsbl_sites
/*	parameter.
/* .IP "\fBpostscreen_forbidden_commands ($smtpd_forbidden_commands)\fR"
/*	List of commands that \fBpostscreen\fR(8) server considers in violation
/*	of the SMTP protocol.
/* .IP "\fBpostscreen_greet_action (ignore)\fR"
/*	The action that \fBpostscreen\fR(8) takes when an SMTP client speaks
/*	before its turn within the time specified with the postscreen_greet_wait
/*	parameter.
/* .IP "\fBpostscreen_greet_banner ($smtpd_banner)\fR"
/*	The \fItext\fR in the optional "220-\fItext\fR..." server
/*	response that
/*	\fBpostscreen\fR(8) sends ahead of the real Postfix SMTP server's "220
/*	text..." response, in an attempt to confuse bad SMTP clients so
/*	that they speak before their turn (pre-greet).
/* .IP "\fBpostscreen_greet_wait (${stress?2}${stress:6}s)\fR"
/*	The amount of time that \fBpostscreen\fR(8) will wait for an SMTP
/*	client to send a command before its turn, and for DNS blocklist
/*	lookup results to arrive (default: up to 2 seconds under stress,
/*	up to 6 seconds otherwise).
/* .IP "\fBpostscreen_helo_required ($smtpd_helo_required)\fR"
/*	Require that a remote SMTP client sends HELO or EHLO before
/*	commencing a MAIL transaction.
/* .IP "\fBpostscreen_non_smtp_command_action (drop)\fR"
/*	The action that \fBpostscreen\fR(8) takes when an SMTP client sends
/*	non-SMTP commands as specified with the postscreen_forbidden_commands
/*	parameter.
/* .IP "\fBpostscreen_non_smtp_command_enable (no)\fR"
/*	Enable "non-SMTP command" tests in the \fBpostscreen\fR(8) server.
/* .IP "\fBpostscreen_pipelining_action (enforce)\fR"
/*	The action that \fBpostscreen\fR(8) takes when an SMTP client sends
/*	multiple commands instead of sending one command and waiting for
/*	the server to respond.
/* .IP "\fBpostscreen_pipelining_enable (no)\fR"
/*	Enable "pipelining" SMTP protocol tests in the \fBpostscreen\fR(8)
/*	server.
/* .IP "\fBpostscreen_whitelist_networks ($mynetworks)\fR"
/*	Network addresses that are permanently whitelisted, and that
/*	will not be subjected to \fBpostscreen\fR(8) checks.
/* .IP "\fBsmtpd_service_name (smtpd)\fR"
/*	The internal service that \fBpostscreen\fR(8) forwards allowed
/*	connections to.
/* CACHE CONTROLS
/* .ad
/* .fi
/* .IP "\fBpostscreen_cache_cleanup_interval (12h)\fR"
/*	The amount of time between \fBpostscreen\fR(8) cache cleanup runs.
/* .IP "\fBpostscreen_cache_map (btree:$data_directory/ps_cache)\fR"
/*	Persistent storage for the \fBpostscreen\fR(8) server decisions.
/* .IP "\fBpostscreen_cache_retention_time (7d)\fR"
/*	The amount of time that \fBpostscreen\fR(8) will cache an expired
/*	temporary whitelist entry before it is removed.
/* .IP "\fBpostscreen_bare_newline_ttl (30d)\fR"
/*	The amount of time that \fBpostscreen\fR(8) will cache results from
/*	a successful "bare newline" SMTP protocol test.
/* .IP "\fBpostscreen_dnsbl_ttl (1h)\fR"
/*	The amount of time that \fBpostscreen\fR(8) will cache results from
/*	a successful DNS blocklist test.
/* .IP "\fBpostscreen_greet_ttl (1d)\fR"
/*	The amount of time that \fBpostscreen\fR(8) will cache results from
/*	a successful PREGREET test.
/* .IP "\fBpostscreen_non_smtp_command_ttl (30d)\fR"
/*	The amount of time that \fBpostscreen\fR(8) will cache results from
/*	a successful "non_smtp_command" SMTP protocol test.
/* .IP "\fBpostscreen_pipelining_ttl (30d)\fR"
/*	The amount of time that \fBpostscreen\fR(8) will cache results from
/*	a successful "pipelining" SMTP protocol test.
/* RESOURCE CONTROLS
/* .ad
/* .fi
/* .IP "\fBline_length_limit (2048)\fR"
/*	Upon input, long lines are chopped up into pieces of at most
/*	this length; upon delivery, long lines are reconstructed.
/* .IP "\fBpostscreen_command_count_limit (20)\fR"
/*	The limit on the total number of commands per SMTP session for
/*	\fBpostscreen\fR(8)'s built-in SMTP protocol engine.
/* .IP "\fBpostscreen_command_time_limit (${stress?10}${stress:300}s)\fR"
/*	The command "read" time limit for \fBpostscreen\fR(8)'s built-in SMTP
/*	protocol engine.
/* .IP "\fBpostscreen_post_queue_limit ($default_process_limit)\fR"
/*	The number of clients that can be waiting for service from a
/*	real SMTP server process.
/* .IP "\fBpostscreen_pre_queue_limit ($default_process_limit)\fR"
/*	The number of non-whitelisted clients that can be waiting for
/*	a decision whether they will receive service from a real SMTP server
/*	process.
/* .IP "\fBpostscreen_watchdog_timeout (10s)\fR"
/*	How much time a \fBpostscreen\fR(8) process may take to respond to
/*	an SMTP client command or to perform a cache operation before it
/*	is terminated by a built-in watchdog timer.
/* MISCELLANEOUS CONTROLS
/* .ad
/* .fi
/* .IP "\fBconfig_directory (see 'postconf -d' output)\fR"
/*	The default location of the Postfix main.cf and master.cf
/*	configuration files.
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
/* README FILES
/* .ad
/* .fi
/*	Use "\fBpostconf readme_directory\fR" or "\fBpostconf
/*	html_directory\fR" to locate this information.
/* .nf
/* .na
/*	POSTSCREEN_README, Postfix Postscreen Howto
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* HISTORY
/* .ad
/* .fi
/*	Many ideas in \fBpostscreen\fR(8) were explored in earlier
/*	work by Michael Tokarev, in OpenBSD spamd, and in MailChannels
/*	Traffic Control.
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

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <events.h>
#include <myaddrinfo.h>
#include <dict_cache.h>
#include <set_eugid.h>
#include <vstream.h>
#include <name_code.h>

/* Global library. */

#include <mail_conf.h>
#include <mail_params.h>
#include <mail_version.h>
#include <mail_proto.h>
#include <data_redirect.h>
#include <string_list.h>

/* Master server protocols. */

#include <mail_server.h>

/* Application-specific. */

#include <postscreen.h>

 /*
  * Configuration parameters.
  */
int     var_proc_limit;
char   *var_smtpd_service;
char   *var_smtpd_banner;
char   *var_smtpd_forbid_cmds;
bool    var_disable_vrfy_cmd;
bool    var_helo_required;

char   *var_ps_forbid_cmds;

bool    var_ps_disable_vrfy;
bool    var_ps_helo_required;

char   *var_ps_cache_map;
int     var_ps_cache_scan;
int     var_ps_cache_ret;
int     var_ps_post_queue_limit;
int     var_ps_pre_queue_limit;
int     var_ps_watchdog;

char   *var_ps_wlist_nets;
char   *var_ps_blist_nets;
char   *var_ps_blist_action;

char   *var_ps_greet_ttl;
int     var_ps_greet_wait;

char   *var_ps_pregr_banner;
char   *var_ps_pregr_action;
int     var_ps_pregr_ttl;

char   *var_ps_dnsbl_sites;
char   *var_ps_dnsbl_reply;
int     var_ps_dnsbl_thresh;
char   *var_ps_dnsbl_action;
int     var_ps_dnsbl_ttl;

bool    var_ps_pipel_enable;
char   *var_ps_pipel_action;
int     var_ps_pipel_ttl;

bool    var_ps_nsmtp_enable;
char   *var_ps_nsmtp_action;
int     var_ps_nsmtp_ttl;

bool    var_ps_barlf_enable;
char   *var_ps_barlf_action;
int     var_ps_barlf_ttl;

int     var_ps_cmd_count;
char   *var_ps_cmd_time;

 /*
  * Global variables.
  */
int     ps_check_queue_length;		/* connections being checked */
int     ps_post_queue_length;		/* being sent to real SMTPD */
DICT_CACHE *ps_cache_map;		/* cache table handle */
VSTRING *ps_temp;			/* scratchpad */
char   *ps_smtpd_service_name;		/* path to real SMTPD */
int     ps_pregr_action;		/* PS_ACT_DROP/ENFORCE/etc */
int     ps_dnsbl_action;		/* PS_ACT_DROP/ENFORCE/etc */
int     ps_pipel_action;		/* PS_ACT_DROP/ENFORCE/etc */
int     ps_nsmtp_action;		/* PS_ACT_DROP/ENFORCE/etc */
int     ps_barlf_action;		/* PS_ACT_DROP/ENFORCE/etc */
int     ps_min_ttl;			/* Update with new tests! */
int     ps_max_ttl;			/* Update with new tests! */
STRING_LIST *ps_forbid_cmds;		/* CONNECT GET POST */
int     ps_stress_greet_wait;		/* stressed greet wait */
int     ps_normal_greet_wait;		/* stressed greet wait */
int     ps_stress_cmd_time_limit;	/* stressed command limit */
int     ps_normal_cmd_time_limit;	/* normal command time limit */
int     ps_stress;			/* stress level */
int     ps_check_queue_length_lowat;	/* stress low-water mark */
int     ps_check_queue_length_hiwat;	/* stress high-water mark */
DICT   *ps_dnsbl_reply;			/* DNSBL name mapper */

 /*
  * Local variables.
  */
static ADDR_MATCH_LIST *ps_wlist_nets;	/* permanently whitelisted networks */
static ADDR_MATCH_LIST *ps_blist_nets;	/* permanently blacklisted networks */
static int ps_blist_action;		/* PS_ACT_DROP/ENFORCE/etc */

/* ps_dump - dump some statistics before exit */

static void ps_dump(void)
{

    /*
     * Dump preliminary cache cleanup statistics when the process commits
     * suicide while a cache cleanup run is in progress. We can't currently
     * distinguish between "postfix reload" (we should restart) or "maximal
     * idle time reached" (we could finish the cache cleanup first).
     */
    if (ps_cache_map) {
	dict_cache_close(ps_cache_map);
	ps_cache_map = 0;
    }
}

/* ps_drain - delayed exit after "postfix reload" */

static void ps_drain(char *unused_service, char **unused_argv)
{
    int     count;

    /*
     * After "postfix reload", complete work-in-progress in the background,
     * instead of dropping already-accepted connections on the floor.
     * 
     * Unfortunately we must close all writable tables, so we can't store or
     * look up reputation information. The reason is that we don't have any
     * multi-writer safety guarantees. We also can't use the single-writer
     * proxywrite service, because its latency guarantees are too weak.
     * 
     * All error retry counts shall be limited. Instead of blocking here, we
     * could retry failed fork() operations in the event call-back routines,
     * but we don't need perfection. The host system is severely overloaded
     * and service levels are already way down.
     * 
     * XXX Some Berkeley DB versions break with close-after-fork. Every new
     * version is an improvement over its predecessor.
     */
    if (ps_cache_map != 0) {
	dict_cache_close(ps_cache_map);
	ps_cache_map = 0;
    }
    for (count = 0; /* see below */ ; count++) {
	if (count >= 5) {
	    msg_fatal("fork: %m");
	} else if (event_server_drain() != 0) {
	    msg_warn("fork: %m");
	    sleep(1);
	    continue;
	} else {
	    return;
	}
    }
}

/* ps_service - handle new client connection */

static void ps_service(VSTREAM *smtp_client_stream,
		               char *unused_service,
		               char **unused_argv)
{
    const char *myname = "ps_service";
    PS_STATE *state;
    struct sockaddr_storage addr_storage;
    SOCKADDR_SIZE addr_storage_len = sizeof(addr_storage);
    MAI_HOSTADDR_STR smtp_client_addr;
    MAI_SERVPORT_STR smtp_client_port;
    int     aierr;
    const char *stamp_str;
    int     window_size;
    int     saved_flags;

    /*
     * This program handles all incoming connections, so it must not block.
     * We use event-driven code for all operations that introduce latency.
     */
    non_blocking(vstream_fileno(smtp_client_stream), NON_BLOCKING);

    /*
     * We use the event_server framework. This means we get already-accepted
     * connections so we have to invoke getpeername() to find out the remote
     * address and port.
     */
#define PS_SERVICE_DISCONNECT_AND_RETURN(stream) do { \
	event_server_disconnect(stream); \
	return; \
    } while (0);

    /*
     * Look up the remote SMTP client address and port.
     */
    if (getpeername(vstream_fileno(smtp_client_stream), (struct sockaddr *)
		    & addr_storage, &addr_storage_len) < 0) {
	msg_warn("getpeername: %m");
	ps_send_reply(vstream_fileno(smtp_client_stream),
		      "unknown_address", "unknown_port",
		      "421 4.3.2 No system resources\r\n");
	PS_SERVICE_DISCONNECT_AND_RETURN(smtp_client_stream);
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
	ps_send_reply(vstream_fileno(smtp_client_stream),
		      "unknown_address", "unknown_port",
		      "421 4.3.2 No system resources\r\n");
	PS_SERVICE_DISCONNECT_AND_RETURN(smtp_client_stream);
    }
    if (strncasecmp("::ffff:", smtp_client_addr.buf, 7) == 0)
	memmove(smtp_client_addr.buf, smtp_client_addr.buf + 7,
		sizeof(smtp_client_addr.buf) - 7);
    if (msg_verbose > 1)
	msg_info("%s: sq=%d cq=%d connect from %s:%s",
		 myname, ps_post_queue_length, ps_check_queue_length,
		 smtp_client_addr.buf, smtp_client_port.buf);

    /*
     * Bundle up all the loose session pieces. This zeroes all flags and time
     * stamps.
     */
    state = ps_new_session_state(smtp_client_stream, smtp_client_addr.buf,
				 smtp_client_port.buf);

    /*
     * Reply with 421 when we can't forward more connections.
     */
    if (var_ps_post_queue_limit > 0
	&& ps_post_queue_length >= var_ps_post_queue_limit) {
	msg_info("reject: connect from %s:%s: all server ports busy",
		 state->smtp_client_addr, state->smtp_client_port);
	PS_DROP_SESSION_STATE(state,
			      "421 4.3.2 All server ports are busy\r\n");
	return;
    }

    /*
     * The permanent whitelist has highest precedence (never block mail from
     * whitelisted sites, and never run tests against those sites).
     */
    if (ps_wlist_nets != 0
      && ps_addr_match_list_match(ps_wlist_nets, state->smtp_client_addr)) {
	msg_info("WHITELISTED %s", state->smtp_client_addr);
	ps_conclude(state);
	return;
    }

    /*
     * The permanent blacklist has second precedence. If the client is
     * permanently blacklisted, send some generic reply and hang up
     * immediately, or run more tests for logging purposes.
     */
    if (ps_blist_nets != 0
      && ps_addr_match_list_match(ps_blist_nets, state->smtp_client_addr)) {
	msg_info("BLACKLISTED %s", state->smtp_client_addr);
	PS_FAIL_SESSION_STATE(state, PS_STATE_FLAG_BLIST_FAIL);
	switch (ps_blist_action) {
	case PS_ACT_DROP:
	    PS_DROP_SESSION_STATE(state,
			     "521 5.3.2 Service currently unavailable\r\n");
	    return;
	case PS_ACT_ENFORCE:
	    PS_ENFORCE_SESSION_STATE(state,
			     "550 5.3.2 Service currently unavailable\r\n");
	    break;
	case PS_ACT_IGNORE:
	    PS_UNFAIL_SESSION_STATE(state, PS_STATE_FLAG_BLIST_FAIL);
	    /* Not: PS_PASS_SESSION_STATE. Repeat this test the next time. */
	    break;
	default:
	    msg_panic("%s: unknown pregreet action value %d",
		      myname, ps_blist_action);
	}
    }

    /*
     * The temporary whitelist (i.e. the postscreen cache) has the lowest
     * precedence. This cache contains information about the results of prior
     * tests. Whitelist the client when all enabled test results are still
     * valid.
     */
    if ((state->flags & PS_STATE_FLAG_ANY_FAIL) == 0
	&& ps_cache_map != 0
	&& (stamp_str = ps_cache_lookup(ps_cache_map, state->smtp_client_addr)) != 0) {
	saved_flags = state->flags;
	ps_parse_tests(state, stamp_str, event_time());
	state->flags |= saved_flags;
	if (msg_verbose)
	    msg_info("%s: cached + recent flags: %s",
		     myname, ps_print_state_flags(state->flags, myname));
	if ((state->flags & PS_STATE_FLAG_ANY_TODO) == 0) {
	    msg_info("PASS OLD %s", state->smtp_client_addr);
	    ps_conclude(state);
	    return;
	}
    } else {
	saved_flags = state->flags;
	ps_new_tests(state);
	state->flags |= saved_flags;
	if (msg_verbose)
	    msg_info("%s: new + recent flags: %s",
		     myname, ps_print_state_flags(state->flags, myname));
    }

    /*
     * Reply with 421 when we can't analyze more connections.
     */
    if (var_ps_pre_queue_limit > 0
	&& ps_check_queue_length - ps_post_queue_length >= var_ps_pre_queue_limit) {
	msg_info("reject: connect from %s:%s: all screening ports busy",
		 state->smtp_client_addr, state->smtp_client_port);
	PS_DROP_SESSION_STATE(state,
			      "421 4.3.2 All screening ports are busy\r\n");
	return;
    }

    /*
     * Before commencing the tests we could set the TCP window to the
     * smallest possible value to save some network bandwidth, at least with
     * spamware that waits until the server starts speaking.
     */
#if 0
    window_size = 1;
    if (setsockopt(vstream_fileno(smtp_client_stream), SOL_SOCKET, SO_RCVBUF,
		   (char *) &window_size, sizeof(window_size)) < 0)
	msg_warn("setsockopt SO_RCVBUF %d: %m", window_size);
#endif

    /*
     * If the client has no up-to-date results for some tests, do those tests
     * first. Otherwise, skip the tests and hand off the connection.
     */
    if (state->flags & PS_STATE_FLAG_EARLY_TODO)
	ps_early_tests(state);
    else if (state->flags & (PS_STATE_FLAG_SMTPD_TODO | PS_STATE_FLAG_NOFORWARD))
	ps_smtpd_tests(state);
    else
	ps_conclude(state);
}

/* ps_cache_validator - validate one cache entry */

static int ps_cache_validator(const char *client_addr,
			              const char *stamp_str,
			              char *unused_context)
{
    PS_STATE dummy;

    /*
     * This function is called by the cache cleanup pseudo thread.
     * 
     * When an entry is removed from the cache, the client will be reported as
     * "NEW" in the next session where it passes all tests again. To avoid
     * silly logging we remove the cache entry only after all tests have
     * expired longer ago than the cache retention time.
     */
    ps_parse_tests(&dummy, stamp_str, event_time() - var_ps_cache_ret);
    return ((dummy.flags & PS_STATE_FLAG_ANY_TODO) == 0);
}

/* pre_jail_init - pre-jail initialization */

static void pre_jail_init(char *unused_name, char **unused_argv)
{
    VSTRING *redirect;

    /*
     * Open read-only maps before dropping privilege, for consistency with
     * other Postfix daemons.
     */
    if (*var_ps_wlist_nets)
	ps_wlist_nets = addr_match_list_init(MATCH_FLAG_NONE, var_ps_wlist_nets);

    if (*var_ps_blist_nets)
	ps_blist_nets = addr_match_list_init(MATCH_FLAG_NONE, var_ps_blist_nets);
    if (*var_ps_forbid_cmds)
	ps_forbid_cmds = string_list_init(MATCH_FLAG_NONE, var_ps_forbid_cmds);
    if (*var_ps_dnsbl_reply)
	ps_dnsbl_reply = dict_open(var_ps_dnsbl_reply, O_RDONLY,
				   DICT_FLAG_DUP_WARN);

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
    redirect = vstring_alloc(100);

    /*
     * Keep state in persistent external map. As a safety measure we sync the
     * database on each update. This hurts on LINUX file systems that sync
     * all dirty disk blocks whenever any application invokes fsync().
     * 
     * Start the cache maintenance pseudo thread after dropping privileges.
     */
#define PS_DICT_OPEN_FLAGS (DICT_FLAG_DUP_REPLACE | DICT_FLAG_SYNC_UPDATE)

    if (*var_ps_cache_map)
	ps_cache_map =
	    dict_cache_open(data_redirect_map(redirect, var_ps_cache_map),
			    O_CREAT | O_RDWR, PS_DICT_OPEN_FLAGS);

    /*
     * Clean up and restore privilege.
     */
    vstring_free(redirect);
    RESTORE_SAVED_EUGID();
}

/* post_jail_init - post-jail initialization */

static void post_jail_init(char *unused_name, char **unused_argv)
{
    const NAME_CODE actions[] = {
	PS_NAME_ACT_DROP, PS_ACT_DROP,
	PS_NAME_ACT_ENFORCE, PS_ACT_ENFORCE,
	PS_NAME_ACT_IGNORE, PS_ACT_IGNORE,
	PS_NAME_ACT_CONT, PS_ACT_IGNORE,/* compatibility */
	0, -1,
    };
    int     cache_flags;

    /*
     * This routine runs after the skeleton code has entered the chroot jail.
     * Prevent automatic process suicide after a limited number of client
     * requests. It is OK to terminate after a limited amount of idle time.
     */
    var_use_limit = 0;

    /*
     * Other one-time initialization.
     */
    ps_temp = vstring_alloc(10);
    vstring_sprintf(ps_temp, "%s/%s", MAIL_CLASS_PRIVATE, var_smtpd_service);
    ps_smtpd_service_name = mystrdup(STR(ps_temp));
    ps_dnsbl_init();
    ps_early_init();
    ps_smtpd_init();

    if ((ps_blist_action = name_code(actions, NAME_CODE_FLAG_NONE,
				     var_ps_blist_action)) < 0)
	msg_fatal("bad %s value: %s", VAR_PS_BLIST_ACTION, var_ps_blist_action);
    if ((ps_dnsbl_action = name_code(actions, NAME_CODE_FLAG_NONE,
				     var_ps_dnsbl_action)) < 0)
	msg_fatal("bad %s value: %s", VAR_PS_DNSBL_ACTION, var_ps_dnsbl_action);
    if ((ps_pregr_action = name_code(actions, NAME_CODE_FLAG_NONE,
				     var_ps_pregr_action)) < 0)
	msg_fatal("bad %s value: %s", VAR_PS_PREGR_ACTION, var_ps_pregr_action);
    if ((ps_pipel_action = name_code(actions, NAME_CODE_FLAG_NONE,
				     var_ps_pipel_action)) < 0)
	msg_fatal("bad %s value: %s", VAR_PS_PIPEL_ACTION, var_ps_pipel_action);
    if ((ps_nsmtp_action = name_code(actions, NAME_CODE_FLAG_NONE,
				     var_ps_nsmtp_action)) < 0)
	msg_fatal("bad %s value: %s", VAR_PS_NSMTP_ACTION, var_ps_nsmtp_action);
    if ((ps_barlf_action = name_code(actions, NAME_CODE_FLAG_NONE,
				     var_ps_barlf_action)) < 0)
	msg_fatal("bad %s value: %s", VAR_PS_BARLF_ACTION, var_ps_barlf_action);

    /*
     * Start the cache maintenance pseudo thread last. Early cleanup makes
     * verbose logging more informative (we get positive confirmation that
     * the cleanup thread runs).
     */
    cache_flags = DICT_CACHE_FLAG_STATISTICS;
    if (msg_verbose > 1)
	cache_flags |= DICT_CACHE_FLAG_VERBOSE;
    if (ps_cache_map != 0 && var_ps_cache_scan > 0)
	dict_cache_control(ps_cache_map,
			   DICT_CACHE_CTL_FLAGS, cache_flags,
			   DICT_CACHE_CTL_INTERVAL, var_ps_cache_scan,
			   DICT_CACHE_CTL_VALIDATOR, ps_cache_validator,
			   DICT_CACHE_CTL_CONTEXT, (char *) 0,
			   DICT_CACHE_CTL_END);

    /*
     * Pre-compute the minimal and maximal TTL.
     */
    ps_min_ttl =
	PS_MIN(PS_MIN(var_ps_pregr_ttl, var_ps_dnsbl_ttl),
	       PS_MIN(PS_MIN(var_ps_pipel_ttl, var_ps_nsmtp_ttl),
		      var_ps_barlf_ttl));
    ps_max_ttl =
	PS_MAX(PS_MAX(var_ps_pregr_ttl, var_ps_dnsbl_ttl),
	       PS_MAX(PS_MAX(var_ps_pipel_ttl, var_ps_nsmtp_ttl),
		      var_ps_barlf_ttl));

    /*
     * Pre-compute the stress and normal command time limits.
     */
    mail_conf_update(VAR_STRESS, "yes");
    ps_stress_cmd_time_limit =
	get_mail_conf_time(VAR_PS_CMD_TIME, DEF_PS_CMD_TIME, 1, 0);
    ps_stress_greet_wait =
	get_mail_conf_time(VAR_PS_GREET_WAIT, DEF_PS_GREET_WAIT, 1, 0);

    mail_conf_update(VAR_STRESS, "");
    ps_normal_cmd_time_limit =
	get_mail_conf_time(VAR_PS_CMD_TIME, DEF_PS_CMD_TIME, 1, 0);
    ps_normal_greet_wait =
	get_mail_conf_time(VAR_PS_GREET_WAIT, DEF_PS_GREET_WAIT, 1, 0);

    ps_check_queue_length_lowat = .7 * var_ps_pre_queue_limit;
    ps_check_queue_length_hiwat = .9 * var_ps_pre_queue_limit;
    if (msg_verbose)
	msg_info(VAR_PS_CMD_TIME ": stress=%d normal=%d lowat=%d hiwat=%d",
		 ps_stress_cmd_time_limit, ps_normal_cmd_time_limit,
		 ps_check_queue_length_lowat, ps_check_queue_length_hiwat);
}

MAIL_VERSION_STAMP_DECLARE;

/* main - pass control to the multi-threaded skeleton */

int     main(int argc, char **argv)
{

    /*
     * List smtpd(8) parameters before any postscreen(8) parameters that have
     * defaults dependencies on them.
     */
    static const CONFIG_STR_TABLE str_table[] = {
	VAR_SMTPD_SERVICE, DEF_SMTPD_SERVICE, &var_smtpd_service, 1, 0,
	VAR_SMTPD_BANNER, DEF_SMTPD_BANNER, &var_smtpd_banner, 1, 0,
	VAR_SMTPD_FORBID_CMDS, DEF_SMTPD_FORBID_CMDS, &var_smtpd_forbid_cmds, 0, 0,
	VAR_PS_CACHE_MAP, DEF_PS_CACHE_MAP, &var_ps_cache_map, 0, 0,
	VAR_PS_PREGR_BANNER, DEF_PS_PREGR_BANNER, &var_ps_pregr_banner, 0, 0,
	VAR_PS_PREGR_ACTION, DEF_PS_PREGR_ACTION, &var_ps_pregr_action, 1, 0,
	VAR_PS_DNSBL_SITES, DEF_PS_DNSBL_SITES, &var_ps_dnsbl_sites, 0, 0,
	VAR_PS_DNSBL_ACTION, DEF_PS_DNSBL_ACTION, &var_ps_dnsbl_action, 1, 0,
	VAR_PS_PIPEL_ACTION, DEF_PS_PIPEL_ACTION, &var_ps_pipel_action, 1, 0,
	VAR_PS_NSMTP_ACTION, DEF_PS_NSMTP_ACTION, &var_ps_nsmtp_action, 1, 0,
	VAR_PS_BARLF_ACTION, DEF_PS_BARLF_ACTION, &var_ps_barlf_action, 1, 0,
	VAR_PS_WLIST_NETS, DEF_PS_WLIST_NETS, &var_ps_wlist_nets, 0, 0,
	VAR_PS_BLIST_NETS, DEF_PS_BLIST_NETS, &var_ps_blist_nets, 0, 0,
	VAR_PS_BLIST_ACTION, DEF_PS_BLIST_ACTION, &var_ps_blist_action, 1, 0,
	VAR_PS_FORBID_CMDS, DEF_PS_FORBID_CMDS, &var_ps_forbid_cmds, 0, 0,
	VAR_PS_DNSBL_REPLY, DEF_PS_DNSBL_REPLY, &var_ps_dnsbl_reply, 0, 0,
	0,
    };
    static const CONFIG_INT_TABLE int_table[] = {
	VAR_PROC_LIMIT, DEF_PROC_LIMIT, &var_proc_limit, 1, 0,
	VAR_PS_DNSBL_THRESH, DEF_PS_DNSBL_THRESH, &var_ps_dnsbl_thresh, 0, 0,
	VAR_PS_CMD_COUNT, DEF_PS_CMD_COUNT, &var_ps_cmd_count, 1, 0,
	0,
    };
    static const CONFIG_NINT_TABLE nint_table[] = {
	VAR_PS_POST_QLIMIT, DEF_PS_POST_QLIMIT, &var_ps_post_queue_limit, 5, 0,
	VAR_PS_PRE_QLIMIT, DEF_PS_PRE_QLIMIT, &var_ps_pre_queue_limit, 10, 0,
	0,
    };
    static const CONFIG_TIME_TABLE time_table[] = {
	VAR_PS_GREET_WAIT, DEF_PS_GREET_WAIT, &var_ps_greet_wait, 1, 0,
	VAR_PS_PREGR_TTL, DEF_PS_PREGR_TTL, &var_ps_pregr_ttl, 1, 0,
	VAR_PS_DNSBL_TTL, DEF_PS_DNSBL_TTL, &var_ps_dnsbl_ttl, 1, 0,
	VAR_PS_PIPEL_TTL, DEF_PS_PIPEL_TTL, &var_ps_pipel_ttl, 1, 0,
	VAR_PS_NSMTP_TTL, DEF_PS_NSMTP_TTL, &var_ps_nsmtp_ttl, 1, 0,
	VAR_PS_BARLF_TTL, DEF_PS_BARLF_TTL, &var_ps_barlf_ttl, 1, 0,
	VAR_PS_CACHE_RET, DEF_PS_CACHE_RET, &var_ps_cache_ret, 1, 0,
	VAR_PS_CACHE_SCAN, DEF_PS_CACHE_SCAN, &var_ps_cache_scan, 1, 0,
	VAR_PS_WATCHDOG, DEF_PS_WATCHDOG, &var_ps_watchdog, 10, 0,
	0,
    };
    static const CONFIG_BOOL_TABLE bool_table[] = {
	VAR_HELO_REQUIRED, DEF_HELO_REQUIRED, &var_helo_required,
	VAR_DISABLE_VRFY_CMD, DEF_DISABLE_VRFY_CMD, &var_disable_vrfy_cmd,
	VAR_PS_PIPEL_ENABLE, DEF_PS_PIPEL_ENABLE, &var_ps_pipel_enable,
	VAR_PS_NSMTP_ENABLE, DEF_PS_NSMTP_ENABLE, &var_ps_nsmtp_enable,
	VAR_PS_BARLF_ENABLE, DEF_PS_BARLF_ENABLE, &var_ps_barlf_enable,
	0,
    };
    static const CONFIG_RAW_TABLE raw_table[] = {
	VAR_PS_CMD_TIME, DEF_PS_CMD_TIME, &var_ps_cmd_time, 1, 0,
	0,
    };
    static const CONFIG_NBOOL_TABLE nbool_table[] = {
	VAR_PS_HELO_REQUIRED, DEF_PS_HELO_REQUIRED, &var_ps_helo_required,
	VAR_PS_DISABLE_VRFY, DEF_PS_DISABLE_VRFY, &var_ps_disable_vrfy,
	0,
    };

    /*
     * Fingerprint executables and core dumps.
     */
    MAIL_VERSION_STAMP_ALLOCATE;

    event_server_main(argc, argv, ps_service,
		      MAIL_SERVER_STR_TABLE, str_table,
		      MAIL_SERVER_INT_TABLE, int_table,
		      MAIL_SERVER_NINT_TABLE, nint_table,
		      MAIL_SERVER_TIME_TABLE, time_table,
		      MAIL_SERVER_BOOL_TABLE, bool_table,
		      MAIL_SERVER_RAW_TABLE, raw_table,
		      MAIL_SERVER_NBOOL_TABLE, nbool_table,
		      MAIL_SERVER_PRE_INIT, pre_jail_init,
		      MAIL_SERVER_POST_INIT, post_jail_init,
		      MAIL_SERVER_SOLITARY,
		      MAIL_SERVER_SLOW_EXIT, ps_drain,
		      MAIL_SERVER_EXIT, ps_dump,
		      MAIL_SERVER_WATCHDOG, &var_ps_watchdog,
		      0);
}
