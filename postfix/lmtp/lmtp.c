/*++
/* NAME
/*	lmtp 8
/* SUMMARY
/*	Postfix local delivery via LMTP
/* SYNOPSIS
/*	\fBlmtp\fR [generic Postfix daemon options] [server attributes...]
/* DESCRIPTION
/*	The LMTP client processes message delivery requests from
/*	the queue manager. Each request specifies a queue file, a sender
/*	address, a domain or host to deliver to, and recipient information.
/*	This program expects to be run from the \fBmaster\fR(8) process
/*	manager.
/*
/*	The LMTP client updates the queue file and marks recipients
/*	as finished, or it informs the queue manager that delivery should
/*	be tried again at a later time. Delivery problem reports are sent
/*	to the \fBbounce\fR(8) or \fBdefer\fR(8) daemon as appropriate.
/*
/*	There are two basic modes of operation for the LMTP client:
/* .IP \(bu
/*	Communication with a local LMTP server via UNIX domain sockets.
/* .IP \(bu
/*	Communication with a (possibly remote) LMTP server via
/*	Internet sockets.
/* .PP
/*	If no server attributes are specified, the LMTP client will contact
/*	the destination host derived from the message delivery request using
/*	the TCP port defined as \fBlmtp\fR in \fBservices\fR(4).  If no such
/*	service is found, the \fBlmtp_tcp_port\fR configuration parameter
/*	(default value of 24) will be used.
/*
/*	In order to use a local LMTP server, this LMTP server will need to
/*	be specified via the server attributes described in the following
/*	section.  Typically, the LMTP client would also be configured as the
/*	\fBlocal\fR delivery agent in the \fBmaster.cf\fR file.
/* SERVER ATTRIBUTE SYNTAX
/* .ad
/* .fi
/*	The server attributes are given in the \fBmaster.cf\fR file at
/*	the end of a service definition.  The syntax is as follows:
/* .IP "\fBserv\fR=\fItype\fR:\fIserver\fR"
/*	The LMTP server to connect to for final delivery.  The \fItype\fR
/*	portion can be either \fBunix\fR or \fBinet\fR. The \fIserver\fR
/*	portion is the path or address of the LMTP server, depending on the
/*	value of \fItype\fR, as shown below:
/* .RS
/* .IP "\fBserv=unix:\fR\fIclass\fR\fB/\fR\fIservname\fR"
/*	This specifies that the local LMTP server \fIservname\fR should be
/*	contacted for final delivery.  Both \fIclass\fR (either \fBpublic\fR
/*	or \fBprivate\fR) and \fIservname\fR correspond to the LMTP server
/*	entry in the \fBmaster.cf\fR file.  This LMTP server will likely
/*	be defined as a \fBspawn\fR(8) service.
/* .IP "\fBserv=inet:"
/*	If nothing follows the \fBinet:\fR type specifier, a connection will
/*	be attempted to the destination host indicated in the delivery request.
/*	This simplest case is identical to defining the LMTP client without
/*	any server attributes at all.
/* .IP "\fBserv=inet:\fR\fIaddress\fR"
/*	In this case, an Internet socket will be made to the server
/*	specified by \fIaddress\fR.  The connection will use a destination
/*	port as described in the previous section.
/* .IP "\fBserv=inet:\fR\fIaddress\fR\fB:\fR\fIport\fR"
/*	Connect to the LMTP server at \fIaddress\fR, but this time use port
/*	\fIport\fR instead of the default \fBlmtp\fR port.
/* .IP "\fBserv=inet:[\fR\fIipaddr\fR\fB]\fR"
/*     The LMTP server to contact is specified using an Internet address
/*     in the "dot notation".  That is, the numeric IP address rather
/*     than the DNS name for the server.  The default \fBlmtp\fR port
/*     is used.
/* .IP "\fBserv=inet:[\fR\fIipaddr\fR\fB]:\fR\fIport\fR"
/*     The LMTP server to contact is specified using the numeric IP address,
/*     at the port specified.
/* .RE
/* .PP
/* SECURITY
/* .ad
/* .fi
/*	The LMTP client is moderately security-sensitive. It talks to LMTP
/*	servers and to DNS servers on the network. The LMTP client can be
/*	run chrooted at fixed low privilege.
/* STANDARDS
/*	RFC 2033 (LMTP protocol)
/*	RFC 821 (SMTP protocol)
/*	RFC 1651 (SMTP service extensions)
/*	RFC 1870 (Message Size Declaration)
/*	RFC 2197 (Pipelining)
/* DIAGNOSTICS
/*	Problems and transactions are logged to \fBsyslogd\fR(8).
/*	Corrupted message files are marked so that the queue manager can
/*	move them to the \fBcorrupt\fR queue for further inspection.
/*
/*	Depending on the setting of the \fBnotify_classes\fR parameter,
/*	the postmaster is notified of bounces, protocol problems, and of
/*	other trouble.
/* BUGS
/* CONFIGURATION PARAMETERS
/* .ad
/* .fi
/*	The following \fBmain.cf\fR parameters are especially relevant to
/*	this program. See the Postfix \fBmain.cf\fR file for syntax details
/*	and for default values. Use the \fBpostfix reload\fR command after
/*	a configuration change.
/* .SH Miscellaneous
/* .ad
/* .fi
/* .IP \fBdebug_peer_level\fR
/*	Verbose logging level increment for hosts that match a
/*	pattern in the \fBdebug_peer_list\fR parameter.
/* .IP \fBdebug_peer_list\fR
/*	List of domain or network patterns. When a remote host matches
/*	a pattern, increase the verbose logging level by the amount
/*	specified in the \fBdebug_peer_level\fR parameter.
/* .IP \fBerror_notice_recipient\fR
/*	Recipient of protocol/policy/resource/software error notices.
/* .IP \fBnotify_classes\fR
/*	When this parameter includes the \fBprotocol\fR class, send mail to the
/*	postmaster with transcripts of LMTP sessions with protocol errors.
/* .IP \fBlmtp_skip_quit_response\fR
/*	Do not wait for the server response after sending QUIT.
/* .IP \fBlmtp_tcp_port\fR
/*	The TCP port to be used when connecting to a LMTP server.  Used as
/*	backup if the \fBlmtp\fR service is not found in \fBservices\fR(4).
/* .SH "Resource controls"
/* .ad
/* .fi
/* .IP \fBlmtp_cache_connection\fR
/*	Should we cache the connection to the LMTP server? The effectiveness
/*	of cached connections will be determined by the number of LMTP servers
/*	in use, and the concurrency limit specified for the LMTP client.
/*	Cached connections are closed under any of the following conditions:
/* .RS
/* .IP \(bu
/*	The idle timeout for the LMTP client is reached. This limit is
/*	enforced by \fBmaster\fR(8).
/* .IP \(bu
/*	A message request to a different destination than the one currently
/*	cached.
/* .IP \(bu
/*	The maximum number of requests per session is reached. This limit is
/*	enforced by \fBmaster\fR(8).
/* .IP \(bu
/*	Upon the onset of another delivery request, the LMTP server associated
/*	with the current session does not respond to the \fBRSET\fR command.
/* .RE
/* .IP \fBlmtp_destination_concurrency_limit\fR
/*	Limit the number of parallel deliveries to the same destination.
/*	The default limit is taken from the
/*	\fBdefault_destination_concurrency_limit\fR parameter.
/* .IP \fBlmtp_destination_recipient_limit\fR
/*	Limit the number of recipients per message delivery.
/*	The default limit is taken from the
/*	\fBdefault_destination_recipient_limit\fR parameter.
/* .IP \fBlocal_destination_recipient_limit\fR
/*	Limit the number of recipients per message delivery.
/*	The default limit is taken from the
/*	\fBdefault_destination_recipient_limit\fR parameter.
/*
/*	This parameter becomes significant if the LMTP client is used
/*	for local delivery.  Some LMTP servers can optimize final delivery
/*	if multiple recipients are allowed.  Therefore, it may be advantageous
/*	to set this to some number greater than one, depending on the capabilities
/*	of the machine.
/*
/*	Setting this parameter to 0 will lead to an unlimited number of
/*	recipients per delivery.  However, this could be risky since it may
/*	make the machine vulnerable to running out of resources if messages
/*	are encountered with an inordinate number of recipients.  Exercise
/*	care when setting this parameter.
/* .SH "Timeout controls"
/* .ad
/* .fi
/* .IP \fBlmtp_connect_timeout\fR
/*	Timeout in seconds for opening a connection to the LMTP server.
/*	If no connection can be made within the deadline, the message
/*	is deferred.
/* .IP \fBlmtp_lhlo_timeout\fR
/*	Timeout in seconds for sending the \fBLHLO\fR command, and for
/*	receiving the server response.
/* .IP \fBlmtp_mail_timeout\fR
/*	Timeout in seconds for sending the \fBMAIL FROM\fR command, and for
/*	receiving the server response.
/* .IP \fBlmtp_rcpt_timeout\fR
/*	Timeout in seconds for sending the \fBRCPT TO\fR command, and for
/*	receiving the server response.
/* .IP \fBlmtp_data_init_timeout\fR
/*	Timeout in seconds for sending the \fBDATA\fR command, and for
/*	receiving the server response.
/* .IP \fBlmtp_data_xfer_timeout\fR
/*	Timeout in seconds for sending the message content.
/* .IP \fBlmtp_data_done_timeout\fR
/*	Timeout in seconds for sending the "\fB.\fR" command, and for
/*	receiving the server response. When no response is received, a
/*	warning is logged that the mail may be delivered multiple times.
/* .IP \fBlmtp_rset_timeout\fR
/*	Timeout in seconds for sending the \fBRSET\fR command, and for
/*	receiving the server response.
/* .IP \fBlmtp_quit_timeout\fR
/*	Timeout in seconds for sending the \fBQUIT\fR command, and for
/*	receiving the server response.
/* SEE ALSO
/*	bounce(8) non-delivery status reports
/*	local(8) local mail delivery
/*	master(8) process manager
/*	qmgr(8) queue manager
/*	services(4) Internet services and aliases
/*	spawn(8) auxiliary command spawner
/*	syslogd(8) system logging
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*
/*	Alterations for LMTP by:
/*	Philip A. Prindeville
/*	Mirapoint, Inc.
/*	USA.
/*
/*	Additional work on LMTP by:
/*	Amos Gouaux
/*	University of Texas at Dallas
/*	P.O. Box 830688, MC34
/*	Richardson, TX 75083, USA
/*--*/

/* System library. */

#include <sys_defs.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dict.h>
#include <pwd.h>
#include <grp.h>

/* Utility library. */

#include <msg.h>
#include <argv.h>
#include <mymalloc.h>
#include <name_mask.h>

/* Global library. */

#include <deliver_request.h>
#include <mail_queue.h>
#include <mail_params.h>
#include <mail_conf.h>
#include <debug_peer.h>
#include <mail_error.h>

/* Single server skeleton. */

#include <mail_server.h>

/* Application-specific. */

#include "lmtp.h"

 /*
  * Tunable parameters. These have compiled-in defaults that can be overruled
  * by settings in the global Postfix configuration file.
  */
int     var_lmtp_tcp_port;
int     var_lmtp_conn_tmout;
int     var_lmtp_rset_tmout;
int     var_lmtp_lhlo_tmout;
int     var_lmtp_mail_tmout;
int     var_lmtp_rcpt_tmout;
int     var_lmtp_data0_tmout;
int     var_lmtp_data1_tmout;
int     var_lmtp_data2_tmout;
int     var_lmtp_quit_tmout;
char   *var_debug_peer_list;
int     var_debug_peer_level;
int     var_lmtp_cache_conn;
int     var_lmtp_skip_quit_resp;
char   *var_notify_classes;
char   *var_error_rcpt;

 /*
  * Global variables.
  * 
  * lmtp_errno is set by the address lookup routines and by the connection
  * management routines.
  * 
  * state is global for the connection caching to work.
  */
int     lmtp_errno;
static LMTP_STATE *state = 0;


/* get_service_attr - get command-line attributes */

static LMTP_ATTR *get_service_attr(char **argv)
{
    char   *myname = "get_service_attr";
    LMTP_ATTR *attr = (LMTP_ATTR *) mymalloc(sizeof(*attr));
    char   *type;
    char   *dest;
    char   *name;

    /*
     * Initialize.
     */
    attr->type = 0;
    attr->class = "";
    attr->name = "";

    /*
     * Iterate over the command-line attribute list.
     */
    if (msg_verbose)
	msg_info("%s: checking argv for lmtp server", myname);

    for ( /* void */ ; *argv != 0; argv++) {

	/*
	 * Are we configured to speak to a particular LMTP server?
	 */
	if (strncasecmp("serv=", *argv, sizeof("serv=") - 1) == 0) {
	    type = *argv + sizeof("serv=") - 1;
	    if ((dest = split_at(type, ':')) == 0)	/* XXX clobbers argv */
		msg_fatal("%s: invalid serv= arguments: %s", myname, *argv);

	    /*
	     * What kind of socket connection are we to make?
	     */
	    if (strcasecmp("unix", type) == 0) {
		attr->type = LMTP_SERV_TYPE_UNIX;
		attr->class = dest;
		if ((name = split_at(dest, '/')) == 0)	/* XXX clobbers argv */
		    msg_fatal("%s: invalid serv= arguments: %s", myname, *argv);
		attr->name = name;
	    } else if (strcasecmp("inet", type) == 0) {
		attr->type = LMTP_SERV_TYPE_INET;
		attr->name = dest;
	    } else
		msg_fatal("%s: invalid serv= arguments: %s", myname, *argv);
	    break;
	}

	/*
	 * Bad.
	 */
	else
	    msg_fatal("%s: unknown attribute name: %s", myname, *argv);
    }

    /*
     * Give the poor tester a clue of what is going on.
     */
    if (msg_verbose)
	msg_info("%s: type %d, class \"%s\", name \"%s\".", myname,
		 attr->type, attr->class, attr->name);
    return (attr);
}

/* deliver_message - deliver message with extreme prejudice */

static int deliver_message(DELIVER_REQUEST *request, char **argv)
{
    char   *myname = "deliver_message";
    static LMTP_ATTR *attr = 0;
    VSTRING *why;
    int     result;

    /*
     * Macro for readability.  We're going to the same destination if the
     * destination was specified on the command line (attr->name is not
     * null), or if the destination of the current session is the same as
     * request->nexthop.
     */
#define SAME_DESTINATION() \
    (*(attr)->name \
     || strcasecmp(state->session->destination, request->nexthop) == 0)

    if (msg_verbose)
	msg_info("%s: from %s", myname, request->sender);

    /*
     * Sanity checks.
     */
    if (attr == 0)
	attr = get_service_attr(argv);
    if (request->rcpt_list.len <= 0)
	msg_fatal("%s: recipient count: %d", myname, request->rcpt_list.len);

    /*
     * Initialize. Bundle all information about the delivery request, so that
     * we can produce understandable diagnostics when something goes wrong
     * many levels below. The alternative would be to make everything global.
     * 
     * Note: `state' is global (to this file) so that we can close a cached
     * connection via the MAIL_SERVER_EXIT function (cleanup). The alloc for
     * `state' is performed in the MAIL_SERVER_PRE_INIT function (pre_init).
     * 
     */
    why = vstring_alloc(100);
    state->request = request;
    state->src = request->fp;

    /*
     * See if we can reuse an existing connection.
     */
    if (state->session != 0) {

	/*
	 * Session already exists from a previous delivery. If we're not
	 * going to the same destination as before, disconnect and establish
	 * a connection to the specified destination.
	 */
	if (!SAME_DESTINATION()) {
	    lmtp_quit(state);
	    lmtp_chat_reset(state);
	    lmtp_session_reset(state);
	    debug_peer_restore();
	}

	/*
	 * Probe the session by sending RSET. If the connection is broken,
	 * clean up our side of the connection.
	 */
	else if (lmtp_rset(state) != 0) {
	    lmtp_chat_reset(state);
	    lmtp_session_reset(state);
	    debug_peer_restore();
	}

	/*
	 * Ready to go with another load.
	 */
	else {
	    ++state->reuse;
	    if (msg_verbose)
		msg_info("%s: reusing (count %d) session with: %s",
			 myname, state->reuse, state->session->host);
	}
    }

    /*
     * If no LMTP session exists, establish one.
     */
    if (state->session == 0) {

	/*
	 * Bounce or defer the recipients if no connection can be made.
	 */
	state->session = lmtp_connect(attr, request, why);
	if (state->session == 0) {
	    lmtp_site_fail(state, lmtp_errno == LMTP_RETRY ? 450 : 550,
			   "%s", vstring_str(why));
	}

	/*
	 * Further check connection by sending the LHLO greeting. If we
	 * cannot talk LMTP to this destination give up, at least for now.
	 */
	else {
	    debug_peer_check(state->session->host, state->session->addr);
	    if (lmtp_lhlo(state) != 0) {
		lmtp_session_reset(state);
		debug_peer_restore();
	    }
	}

    }

    /*
     * If a session exists, deliver this message to all requested recipients.
     * 
     */
    if (state->session != 0)
	lmtp_xfer(state);

    /*
     * At the end, notify the postmaster of any protocol errors.
     */
    if (state->history != 0
	&& (state->error_mask
	    & name_mask(mail_error_masks, var_notify_classes)))
	lmtp_chat_notify(state);

    /*
     * Disconnect if we're not cacheing connections.
     */
    if (!var_lmtp_cache_conn && state->session != 0) {
	lmtp_quit(state);
	lmtp_session_reset(state);
	debug_peer_restore();
    }

    /*
     * Clean up.
     */
    vstring_free(why);
    result = state->status;
    lmtp_chat_reset(state);

    return (result);
}

/* lmtp_service - perform service for client */

static void lmtp_service(VSTREAM *client_stream, char *unused_service, char **argv)
{
    DELIVER_REQUEST *request;
    int     status;

    /*
     * This routine runs whenever a client connects to the UNIX-domain socket
     * dedicated to remote LMTP delivery service. What we see below is a
     * little protocol to (1) tell the queue manager that we are ready, (2)
     * read a request from the queue manager, and (3) report the completion
     * status of that request. All connection-management stuff is handled by
     * the common code in single_server.c.
     */
    if ((request = deliver_request_read(client_stream)) != 0) {
	status = deliver_message(request, argv);
	deliver_request_done(client_stream, request, status);
    }
}

/* pre_init - pre-jail initialization */

static void pre_init(char *unused_name, char **unused_argv)
{
    debug_peer_init();
    state = lmtp_state_alloc();
}

/* cleanup - close any open connections, etc. */

static void cleanup()
{
    if (state == 0)
	return;

    if (state->session != 0) {
	lmtp_quit(state);
	lmtp_chat_reset(state);
	lmtp_session_free(state->session);
	debug_peer_restore();
	if (msg_verbose)
	    msg_info("cleanup: just closed down session");
    }
    lmtp_state_free(state);
}

/* pre_accept - see if tables have changed

static void pre_accept(char *unused_name, char **unused_argv)
{
    if (dict_changed()) {
	msg_info("table has changed -- exiting");
	cleanup();
	exit(0);
    }
}


/*
   main - pass control to the single-threaded skeleton
*/

int     main(int argc, char **argv)
{
    static CONFIG_STR_TABLE str_table[] = {
	VAR_DEBUG_PEER_LIST, DEF_DEBUG_PEER_LIST, &var_debug_peer_list, 0, 0,
	VAR_NOTIFY_CLASSES, DEF_NOTIFY_CLASSES, &var_notify_classes, 0, 0,
	VAR_ERROR_RCPT, DEF_ERROR_RCPT, &var_error_rcpt, 1, 0,
	0,
    };
    static CONFIG_INT_TABLE int_table[] = {
	VAR_LMTP_TCP_PORT, DEF_LMTP_TCP_PORT, &var_lmtp_tcp_port, 0, 0,
	VAR_LMTP_CONN_TMOUT, DEF_LMTP_CONN_TMOUT, &var_lmtp_conn_tmout, 0, 0,
	VAR_LMTP_RSET_TMOUT, DEF_LMTP_RSET_TMOUT, &var_lmtp_rset_tmout, 1, 0,
	VAR_LMTP_LHLO_TMOUT, DEF_LMTP_LHLO_TMOUT, &var_lmtp_lhlo_tmout, 1, 0,
	VAR_LMTP_MAIL_TMOUT, DEF_LMTP_MAIL_TMOUT, &var_lmtp_mail_tmout, 1, 0,
	VAR_LMTP_RCPT_TMOUT, DEF_LMTP_RCPT_TMOUT, &var_lmtp_rcpt_tmout, 1, 0,
	VAR_LMTP_DATA0_TMOUT, DEF_LMTP_DATA0_TMOUT, &var_lmtp_data0_tmout, 1, 0,
	VAR_LMTP_DATA1_TMOUT, DEF_LMTP_DATA1_TMOUT, &var_lmtp_data1_tmout, 1, 0,
	VAR_LMTP_DATA2_TMOUT, DEF_LMTP_DATA2_TMOUT, &var_lmtp_data2_tmout, 1, 0,
	VAR_LMTP_QUIT_TMOUT, DEF_LMTP_QUIT_TMOUT, &var_lmtp_quit_tmout, 1, 0,
	VAR_DEBUG_PEER_LEVEL, DEF_DEBUG_PEER_LEVEL, &var_debug_peer_level, 1, 0,
	0,
    };
    static CONFIG_BOOL_TABLE bool_table[] = {
	VAR_LMTP_CACHE_CONN, DEF_LMTP_CACHE_CONN, &var_lmtp_cache_conn,
	VAR_LMTP_SKIP_QUIT_RESP, DEF_LMTP_SKIP_QUIT_RESP, &var_lmtp_skip_quit_resp,
	0,
    };

    single_server_main(argc, argv, lmtp_service,
		       MAIL_SERVER_INT_TABLE, int_table,
		       MAIL_SERVER_STR_TABLE, str_table,
		       MAIL_SERVER_BOOL_TABLE, bool_table,
		       MAIL_SERVER_PRE_INIT, pre_init,
		       MAIL_SERVER_PRE_ACCEPT, pre_accept,
		       MAIL_SERVER_EXIT, cleanup,
		       0);
}
