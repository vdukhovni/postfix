/*++
/* NAME
/*	smtpd 8
/* SUMMARY
/*	Postfix SMTP server
/* SYNOPSIS
/*	\fBsmtpd\fR [generic Postfix daemon options]
/* DESCRIPTION
/*	The SMTP server accepts network connection requests
/*	and performs zero or more SMTP transactions per connection.
/*	Each received message is piped through the \fBcleanup\fR(8)
/*	daemon, and is placed into the \fBincoming\fR queue as one
/*	single queue file.  For this mode of operation, the program
/*	expects to be run from the \fBmaster\fR(8) process manager.
/*
/*	Alternatively, the SMTP server takes an established
/*	connection on standard input and deposits messages directly
/*	into the \fBmaildrop\fR queue. In this so-called stand-alone
/*	mode, the SMTP server can accept mail even while the mail
/*	system is not running.
/*
/*	The SMTP server implements a variety of policies for connection
/*	requests, and for parameters given to \fBHELO, ETRN, MAIL FROM, VRFY\fR
/*	and \fBRCPT TO\fR commands. They are detailed below and in the
/*	\fBmain.cf\fR configuration file.
/* SECURITY
/* .ad
/* .fi
/*	The SMTP server is moderately security-sensitive. It talks to SMTP
/*	clients and to DNS servers on the network. The SMTP server can be
/*	run chrooted at fixed low privilege.
/* STANDARDS
/*	RFC 821 (SMTP protocol)
/*	RFC 1123 (Host requirements)
/*	RFC 1652 (8bit-MIME transport)
/*	RFC 1869 (SMTP service extensions)
/*	RFC 1870 (Message Size Declaration)
/*	RFC 1985 (ETRN command)
/*	RFC 2554 (AUTH command)
/*	RFC 2821 (SMTP protocol)
/*	RFC 2920 (SMTP Pipelining)
/* DIAGNOSTICS
/*	Problems and transactions are logged to \fBsyslogd\fR(8).
/*
/*	Depending on the setting of the \fBnotify_classes\fR parameter,
/*	the postmaster is notified of bounces, protocol problems,
/*	policy violations, and of other trouble.
/* CONFIGURATION PARAMETERS
/* .ad
/* .fi
/*	The following \fBmain.cf\fR parameters are especially relevant to
/*	this program. See the Postfix \fBmain.cf\fR file for syntax details
/*	and for default values. Use the \fBpostfix reload\fR command after
/*	a configuration change.
/* .SH "Compatibility controls"
/* .ad
/* .fi
/* .IP \fBstrict_rfc821_envelopes\fR
/*	Disallow non-RFC 821 style addresses in SMTP commands. For example,
/*	the RFC822-style address forms with comments that Sendmail allows.
/* .IP \fBbroken_sasl_auth_clients\fR
/*	Support Microsoft clients that implement an older version of the AUTH
/*	protocol, and that expect an EHLO response of "250 AUTH=list"
/*	instead of "250 AUTH list".
/* .IP \fBsmtpd_sasl_exceptions_networks\fR
/*	Don't offer AUTH in the response to EHLO when talking to clients
/*	in the specified networks.  This is a workaround for clients that
/*	that demand a login and password from the user whenever AUTH is
/*	offered by an SMTP server.
/* .IP \fBsmtpd_noop_commands\fR
/*	List of commands that are treated as NOOP (no operation) commands,
/*	without any parameter syntax checking and without any state change.
/*	This list overrides built-in command definitions.
/* .SH "Content inspection controls"
/*	Optionally, Postfix can be configured to send new mail to
/*	external content filter software AFTER the mail is queued.
/* .IP \fBcontent_filter\fR
/*	The name of a mail delivery transport that filters mail and that
/*	either bounces mail or re-injects the result back into Postfix.
/*	This parameter uses the same syntax as the right-hand side of
/*	a Postfix transport table.
/* .IP \fBreceive_override_options\fB
/*	The following options override \fBmain.cf\fR settings.
/*	The options are either implemented by the SMTP server or
/*	are passed on to the downstream cleanup server.
/* .RS
/* .IP \fBno_unknown_recipient_checks\fR
/*	Do not try to reject unknown recipients. This is typically specified
/*	with the SMTP server \fBafter\fR an external content filter.
/* .IP \fBno_address_mappings\fR
/*	Disable canonical address mapping, virtual alias map expansion,
/*	address masquerading, and automatic BCC recipients. This is
/*	typically specified with the SMTP server \fBbefore\fR an external
/*	content filter.
/* .IP \fBno_header_body_checks\fR
/*	Disable header/body_checks. This is typically specified with the
/*	SMTP server \fBafter\fR an external content filter.
/* .RE
/* .SH "Pass-through proxy"
/* .ad
/* .fi
/* .ad
/*	Optionally, the Postfix SMTP server can be configured to
/*	forward all mail to a proxy server, for example a real-time
/*	content filter, BEFORE mail is queued.
/* .IP \fBsmtpd_proxy_filter\fR
/*	The \fIhost:port\fR of the SMTP proxy server. The \fIhost\fR
/*	or \fIhost:\fR portion is optional.
/* .IP \fBsmtpd_proxy_timeout\fR
/*	Timeout for connecting to, sending to and receiving from
/*	the SMTP proxy server.
/* .IP \fBsmtpd_proxy_ehlo\fR
/*	The hostname to use when sending an EHLO command to the
/*	SMTP proxy server.
/* .SH "Authentication controls"
/* .IP \fBsmtpd_sasl_auth_enable\fR
/*	Enable per-session authentication as per RFC 2554 (SASL).
/*	This functionality is available only when explicitly selected
/*	at program build time and explicitly enabled at runtime.
/* .IP \fBsmtpd_sasl_local_domain\fR
/*	The name of the local authentication realm.
/* .IP \fBsmtpd_sasl_security_options\fR
/*	Zero or more of the following.
/* .RS
/* .IP \fBnoplaintext\fR
/*	Disallow authentication methods that use plaintext passwords.
/* .IP \fBnoactive\fR
/*	Disallow authentication methods that are vulnerable to non-dictionary
/*	active attacks.
/* .IP \fBnodictionary\fR
/*	Disallow authentication methods that are vulnerable to passive
/*	dictionary attack.
/* .IP \fBnoanonymous\fR
/*	Disallow anonymous logins.
/* .RE
/* .IP \fBsmtpd_sender_login_maps\fR
/*	Maps that specify the SASL login name that owns a MAIL FROM sender
/*	address. Used by the \fBreject_sender_login_mismatch\fR sender
/*	anti-spoofing restriction.
/* .SH Miscellaneous
/* .ad
/* .fi
/* .IP \fBsmtpd_authorized_verp_clients\fR
/*	Hostnames, domain names and/or addresses of clients that are
/*	authorized to use the XVERP extension.
/* .IP \fBsmtpd_authorized_xclient_hosts\fR
/*	Hostnames, domain names and/or addresses of clients that are
/*	authorized to use the XCLIENT command.  This command changes
/*	client information for access control and/or logging purposes,
/*	with the exception of the
/*	\fBsmtpd_authorized_xclient_hosts\fR access control itself.
/* .IP \fBdebug_peer_level\fR
/*	Increment in verbose logging level when a remote host matches a
/*	pattern in the \fBdebug_peer_list\fR parameter.
/* .IP \fBdebug_peer_list\fR
/*	List of domain or network patterns. When a remote host matches
/*	a pattern, increase the verbose logging level by the amount
/*	specified in the \fBdebug_peer_level\fR parameter.
/* .IP \fBdefault_verp_delimiters\fR
/*	The default VERP delimiter characters that are used when the
/*	XVERP command is specified without explicit delimiters.
/* .IP \fBerror_notice_recipient\fR
/*	Recipient of protocol/policy/resource/software error notices.
/* .IP \fBhopcount_limit\fR
/*	Limit the number of \fBReceived:\fR message headers.
/* .IP \fBnotify_classes\fR
/*	List of error classes. Of special interest are:
/* .RS
/* .IP \fBpolicy\fR
/*	When a client violates any policy, mail a transcript of the
/*	entire SMTP session to the postmaster.
/* .IP \fBprotocol\fR
/*	When a client violates the SMTP protocol or issues an unimplemented
/*	command, mail a transcript of the entire SMTP session to the
/*	postmaster.
/* .RE
/* .IP \fBsmtpd_banner\fR
/*	Text that follows the \fB220\fR status code in the SMTP greeting banner.
/* .IP \fBsmtpd_expansion_filter\fR
/*	Controls what characters are allowed in $name expansion of
/*	rbl template responses and other text.
/* .IP \fBsmtpd_recipient_limit\fR
/*	Restrict the number of recipients that the SMTP server accepts
/*	per message delivery.
/* .IP \fBsmtpd_timeout\fR
/*	Limit the time to send a server response and to receive a client
/*	request.
/* .IP \fBsoft_bounce\fR
/*	Change hard (5xx) reject responses into soft (4xx) reject responses.
/*	This can be useful for testing purposes.
/* .IP \fBverp_delimiter_filter\fR
/*	The characters that Postfix accepts as VERP delimiter characters.
/* .SH "Known versus unknown recipients"
/* .ad
/* .fi
/* .IP \fBshow_user_unknown_table_name\fR
/*	Whether or not to reveal the table name in the "User unknown"
/*	responses. The extra detail makes trouble shooting easier
/*	but also reveals information that is nobody elses business.
/* .IP \fBunknown_local_recipient_reject_code\fR
/*	The response code when a client specifies a recipient whose domain
/*	matches \fB$mydestination\fR or \fB$inet_interfaces\fR, while
/*	\fB$local_recipient_maps\fR is non-empty and does not list
/*	the recipient address or address local-part.
/* .IP \fBunknown_relay_recipient_reject_code\fR
/*	The response code when a client specifies a recipient whose domain
/*	matches \fB$relay_domains\fR, while \fB$relay_recipient_maps\fR
/*	is non-empty and does not list the recipient address.
/* .IP \fBunknown_virtual_alias_reject_code\fR
/*	The response code when a client specifies a recipient whose domain
/*	matches \fB$virtual_alias_domains\fR, while the recipient is not
/*	listed in \fB$virtual_alias_maps\fR.
/* .IP \fBunknown_virtual_mailbox_reject_code\fR
/*	The response code when a client specifies a recipient whose domain
/*	matches \fB$virtual_mailbox_domains\fR, while the recipient is not
/*	listed in \fB$virtual_mailbox_maps\fR.
/* .SH "Resource controls"
/* .ad
/* .fi
/* .IP \fBline_length_limit\fR
/*	Limit the amount of memory in bytes used for the handling of
/*	partial input lines.
/* .IP \fBmessage_size_limit\fR
/*	Limit the total size in bytes of a message, including on-disk
/*	storage for envelope information.
/* .IP \fBqueue_minfree\fR
/*	Minimal amount of free space in bytes in the queue file system
/*	for the SMTP server to accept any mail at all (default: twice
/*	the \fBmessage_size_limit\fR value).
/* .IP \fBsmtpd_history_flush_threshold\fR
/*	Flush the command history to postmaster after receipt of RSET etc.
/*	only if the number of history lines exceeds the given threshold.
/* .IP \fBsmtpd_client_connection_count_limit\fR
/*	The maximal number of simultaneous connections that any
/*	client is allowed to make to this service.  When a client exceeds
/*	the limit, the SMTP server logs a warning with the client
/*	name/address and the service name as configured in master.cf.
/* .IP \fBsmtpd_client_connection_rate_limit\fR
/*	The maximal number of connections per unit time (specified
/*	with \fBconnection_rate_time_unit\fR) that any client
/*	is allowed to make to this service. When a client exceeds
/*	the limit, the SMTP server logs a warning with the client
/*	name/address and the service name as configured in master.cf.
/* .IP \fBsmtpd_client_connection_limit_exceptions\fR
/*	Hostnames, .domain names and/or network address blocks of clients
/*	that are excluded from connection count or rate limits.
/* .SH Tarpitting
/* .ad
/* .fi
/* .IP \fBsmtpd_error_sleep_time\fR
/*	Time to wait in seconds before sending a 4xx or 5xx server error
/*	response.
/* .IP \fBsmtpd_soft_error_limit\fR
/*	When an SMTP client has made this number of errors, wait
/*	\fIerror_count\fR seconds before responding to any client request.
/* .IP \fBsmtpd_hard_error_limit\fR
/*	Disconnect after a client has made this number of errors.
/* .IP \fBsmtpd_junk_command_limit\fR
/*	Limit the number of times a client can issue a junk command
/*	such as NOOP, VRFY, ETRN or RSET in one SMTP session before
/*	it is penalized with tarpit delays.
/* .SH "Delegated policy"
/* .ad
/* .fi
/* .IP \fBsmtpd_policy_service_timeout\fR
/*	Time limit for connecting to, writing to and receiving from
/*	a delegated SMTPD policy server.
/* .IP \fBsmtpd_policy_service_max_idle\fR
/*	Time after which an unused SMTPD policy service connection
/*	is closed.
/* .IP \fBsmtpd_policy_service_timeout\fR
/*	Time after which an active SMTPD policy service connection
/*	is closed.
/* .SH "UCE control restrictions"
/* .ad
/* .fi
/* .IP \fBparent_domain_matches_subdomains\fR
/*	List of Postfix features that use \fIdomain.tld\fR patterns
/*	to match \fIsub.domain.tld\fR (as opposed to
/*	requiring \fI.domain.tld\fR patterns).
/* .IP \fBsmtpd_client_restrictions\fR
/*	Restrict what clients may connect to this mail system.
/* .IP \fBsmtpd_helo_required\fR
/*	Require that clients introduce themselves at the beginning
/*	of an SMTP session.
/* .IP \fBsmtpd_helo_restrictions\fR
/*	Restrict what client hostnames are allowed in \fBHELO\fR and
/*	\fBEHLO\fR commands.
/* .IP \fBsmtpd_sender_restrictions\fR
/*	Restrict what sender addresses are allowed in \fBMAIL FROM\fR commands.
/* .IP \fBsmtpd_recipient_restrictions\fR
/*	Restrict what recipient addresses are allowed in \fBRCPT TO\fR commands.
/* .IP \fBsmtpd_etrn_restrictions\fR
/*	Restrict what domain names can be used in \fBETRN\fR commands,
/*	and what clients may issue \fBETRN\fR commands.
/* .IP \fBsmtpd_data_restrictions\fR
/*	Restrictions on the \fBDATA\fR command. Currently, the only restriction
/*	that makes sense here is \fBreject_unauth_pipelining\fR.
/* .IP \fBallow_untrusted_routing\fR
/*	Allow untrusted clients to specify addresses with sender-specified
/*	routing.  Enabling this opens up nasty relay loopholes involving
/*	trusted backup MX hosts.
/* .IP \fBsmtpd_restriction_classes\fR
/*	Declares the name of zero or more parameters that contain a
/*	list of UCE restrictions. The names of these parameters can
/*	then be used instead of the restriction lists that they represent.
/* .IP \fBsmtpd_null_access_lookup_key\fR
/*	The lookup key to be used in SMTPD access tables instead of the
/*	null sender address. A null sender address cannot be looked up.
/* .IP "\fBmaps_rbl_domains\fR (deprecated)"
/*	List of DNS domains that publish the addresses of blacklisted
/*	hosts. This is used with the deprecated \fBreject_maps_rbl\fR
/*	restriction.
/* .IP \fBpermit_mx_backup_networks\fR
/*	Only domains whose primary MX hosts match the listed networks
/*	are eligible for the \fBpermit_mx_backup\fR feature.
/* .IP \fBrelay_domains\fR
/*	Restrict what domains this mail system will relay
/*	mail to. The domains are routed to the delivery agent
/*	specified with the \fBrelay_transport\fR setting.
/* .SH "Sender/recipient address verification"
/* .ad
/* .fi
/*	Address verification is implemented by sending probe email
/*	messages that are not actually delivered, and is enabled
/*	via the reject_unverified_{sender,recipient} access restriction.
/*	The status of verification probes is maintained by the address
/*	verification service.
/* .IP \fBaddress_verify_poll_count\fR
/*	How many times to query the address verification service
/*	for completion of an address verification request.
/*	Specify 1 to implement a simple form of greylisting, that is,
/*	always defer the request for a new sender or recipient address.
/* .IP \fBaddress_verify_poll_delay\fR
/*	Time to wait after querying the address verification service
/*	for completion of an address verification request.
/* .SH "UCE control responses"
/* .ad
/* .fi
/* .IP \fBaccess_map_reject_code\fR
/*	Response code when a client violates an access database restriction.
/* .IP \fBdefault_rbl_reply\fR
/*	Default template reply when a request is RBL blacklisted.
/*	This template is used by the \fBreject_rbl_*\fR and
/*	\fBreject_rhsbl_*\fR restrictions. See also:
/*	\fBrbl_reply_maps\fR and \fBsmtpd_expansion_filter\fR.
/* .IP \fBdefer_code\fR
/*	Response code when a client request is rejected by the \fBdefer\fR
/*	restriction.
/* .IP \fBinvalid_hostname_reject_code\fR
/*	Response code when a client violates the \fBreject_invalid_hostname\fR
/*	restriction.
/* .IP \fBmaps_rbl_reject_code\fR
/*	Response code when a request is RBL blacklisted.
/* .IP \fBmulti_recipient_bounce_reject_code\fR
/*	Response code when a multi-recipient bounce is blocked.
/* .IP \fBrbl_reply_maps\fR
/*	Table with template responses for RBL blacklisted requests, indexed by
/*	RBL domain name. These templates are used by the \fBreject_rbl_*\fR
/*	and \fBreject_rhsbl_*\fR restrictions. See also:
/*	\fBdefault_rbl_reply\fR and \fBsmtpd_expansion_filter\fR.
/* .IP \fBreject_code\fR
/*	Response code when the client matches a \fBreject\fR restriction.
/* .IP \fBrelay_domains_reject_code\fR
/*	Response code when a client attempts to violate the mail relay
/*	policy.
/* .IP \fBunknown_address_reject_code\fR
/*	Response code when a client violates the \fBreject_unknown_address\fR
/*	restriction.
/* .IP \fBunknown_client_reject_code\fR
/*	Response code when a client without address to name mapping
/*	violates the \fBreject_unknown_client\fR restriction.
/* .IP \fBunknown_hostname_reject_code\fR
/*	Response code when a client violates the \fBreject_unknown_hostname\fR
/*	restriction.
/* .IP \fBunverified_sender_reject_code\fR
/*	Response code when a sender address is known to be undeliverable.
/* .IP \fBunverified_recipient_reject_code\fR
/*	Response code when a recipient address is known to be undeliverable.
/* SEE ALSO
/*	cleanup(8) message canonicalization
/*	master(8) process manager
/*	syslogd(8) system logging
/*	trivial-rewrite(8) address resolver
/*	verify(8) address verification service
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
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>			/* remove() */
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <stddef.h>			/* offsetof() */

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <vstring.h>
#include <vstream.h>
#include <vstring_vstream.h>
#include <stringops.h>
#include <events.h>
#include <smtp_stream.h>
#include <valid_hostname.h>
#include <dict.h>
#include <watchdog.h>
#include <iostuff.h>
#include <split_at.h>

/* Global library. */

#include <mail_params.h>
#include <record.h>
#include <rec_type.h>
#include <mail_proto.h>
#include <cleanup_user.h>
#include <mail_date.h>
#include <mail_conf.h>
#include <off_cvt.h>
#include <debug_peer.h>
#include <mail_error.h>
#include <flush_clnt.h>
#include <mail_stream.h>
#include <mail_queue.h>
#include <tok822.h>
#include <verp_sender.h>
#include <string_list.h>
#include <quote_822_local.h>
#include <lex_822.h>
#include <namadr_list.h>
#include <input_transp.h>
#include <anvil_clnt.h>
#include <xtext.h>

/* Single-threaded server skeleton. */

#include <mail_server.h>

/* Application-specific */

#include <smtpd_token.h>
#include <smtpd.h>
#include <smtpd_check.h>
#include <smtpd_chat.h>
#include <smtpd_sasl_proto.h>
#include <smtpd_sasl_glue.h>
#include <smtpd_proxy.h>

 /*
  * Tunable parameters. Make sure that there is some bound on the length of
  * an SMTP command, so that the mail system stays in control even when a
  * malicious client sends commands of unreasonable length (qmail-dos-1).
  * Make sure there is some bound on the number of recipients, so that the
  * mail system stays in control even when a malicious client sends an
  * unreasonable number of recipients (qmail-dos-2).
  */
int     var_smtpd_rcpt_limit;
int     var_smtpd_tmout;
int     var_smtpd_soft_erlim;
int     var_smtpd_hard_erlim;
int     var_queue_minfree;		/* XXX use off_t */
char   *var_smtpd_banner;
char   *var_notify_classes;
char   *var_client_checks;
char   *var_helo_checks;
char   *var_mail_checks;
char   *var_rcpt_checks;
char   *var_etrn_checks;
char   *var_data_checks;
int     var_unk_client_code;
int     var_bad_name_code;
int     var_unk_name_code;
int     var_unk_addr_code;
int     var_relay_code;
int     var_maps_rbl_code;
int     var_access_map_code;
char   *var_maps_rbl_domains;
char   *var_rbl_reply_maps;
int     var_helo_required;
int     var_reject_code;
int     var_defer_code;
int     var_smtpd_err_sleep;
int     var_non_fqdn_code;
char   *var_error_rcpt;
int     var_smtpd_delay_reject;
char   *var_rest_classes;
int     var_strict_rfc821_env;
bool    var_disable_vrfy_cmd;
char   *var_canonical_maps;
char   *var_rcpt_canon_maps;
char   *var_virt_alias_maps;
char   *var_virt_mailbox_maps;
char   *var_alias_maps;
char   *var_local_rcpt_maps;
bool    var_allow_untrust_route;
int     var_smtpd_junk_cmd_limit;
bool    var_smtpd_sasl_enable;
char   *var_smtpd_sasl_opts;
char   *var_smtpd_sasl_realm;
char   *var_smtpd_sasl_exceptions_networks;
char   *var_filter_xport;
bool    var_broken_auth_clients;
char   *var_perm_mx_networks;
char   *var_smtpd_snd_auth_maps;
char   *var_smtpd_noop_cmds;
char   *var_smtpd_null_key;
int     var_smtpd_hist_thrsh;
char   *var_smtpd_exp_filter;
char   *var_def_rbl_reply;
int     var_unv_from_code;
int     var_unv_rcpt_code;
int     var_mul_rcpt_code;
char   *var_relay_rcpt_maps;
char   *var_verify_sender;
int     var_local_rcpt_code;
int     var_virt_alias_code;
int     var_virt_mailbox_code;
int     var_relay_rcpt_code;
char   *var_verp_clients;
int     var_show_unk_rcpt_table;
int     var_verify_poll_count;
int     var_verify_poll_delay;
char   *var_smtpd_proxy_filt;
int     var_smtpd_proxy_tmout;
char   *var_smtpd_proxy_ehlo;
char   *var_input_transp;
int     var_smtpd_policy_tmout;
int     var_smtpd_policy_idle;
int     var_smtpd_policy_ttl;
char   *var_xclient_hosts;
int     var_smtpd_crate_limit;
int     var_smtpd_cconn_limit;
char   *var_smtpd_hoggers;

 /*
  * Silly little macros.
  */
#define STR(x)	vstring_str(x)
#define LEN(x)	VSTRING_LEN(x)

 /*
  * VERP command name.
  */
#define VERP_CMD	"XVERP"
#define VERP_CMD_LEN	5

static NAMADR_LIST *verp_clients;

 /*
  * XCLIENT command. Access control is cached, so that XCLIENT can't override
  * its own access control.
  */
static NAMADR_LIST *xclient_hosts;
static int xclient_allowed;

 /*
  * Client connection and rate limiting.
  */
ANVIL_CLNT *anvil_clnt;
static NAMADR_LIST *hogger_list;

 /*
  * Other application-specific globals.
  */
int     smtpd_input_transp_mask;

 /*
  * Forward declarations.
  */
static void helo_reset(SMTPD_STATE *);
static void mail_reset(SMTPD_STATE *);
static void rcpt_reset(SMTPD_STATE *);
static void chat_reset(SMTPD_STATE *, int);

#ifdef USE_SASL_AUTH

 /*
  * SASL exceptions.
  */
static NAMADR_LIST *sasl_exceptions_networks;

/* sasl_client_exception - can we offer AUTH for this client */

static int sasl_client_exception(SMTPD_STATE *state)
{
    int     match;

    /*
     * This is to work around a Netscape mail client bug where it tries to
     * use AUTH if available, even if user has not configured it. Returns
     * TRUE if AUTH should be offered in the EHLO.
     */
    if (sasl_exceptions_networks == 0)
	return (0);

    match = namadr_list_match(sasl_exceptions_networks,
			      state->name, state->addr);

    if (msg_verbose)
	msg_info("sasl_exceptions: %s[%s], match=%d",
		 state->name, state->addr, match);

    return (match);
}

#endif

/* collapse_args - put arguments together again */

static void collapse_args(int argc, SMTPD_TOKEN *argv)
{
    int     i;

    for (i = 1; i < argc; i++) {
	vstring_strcat(argv[0].vstrval, " ");
	vstring_strcat(argv[0].vstrval, argv[i].strval);
    }
    argv[0].strval = STR(argv[0].vstrval);
}

/* helo_cmd - process HELO command */

static int helo_cmd(SMTPD_STATE *state, int argc, SMTPD_TOKEN *argv)
{
    char   *err;

    if (argc < 2) {
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	smtpd_chat_reply(state, "501 Syntax: HELO hostname");
	return (-1);
    }
    if (argc > 2)
	collapse_args(argc - 1, argv + 1);
    if (SMTPD_STAND_ALONE(state) == 0
	&& var_smtpd_delay_reject == 0
	&& (err = smtpd_check_helo(state, argv[1].strval)) != 0) {
	smtpd_chat_reply(state, "%s", err);
	return (-1);
    }
    if (state->helo_name != 0)
	helo_reset(state);
    chat_reset(state, var_smtpd_hist_thrsh);
    mail_reset(state);
    rcpt_reset(state);
    state->helo_name = mystrdup(printable(argv[1].strval, '?'));
    neuter(state->helo_name, "<>()\\\";:@", '?');
    /* Downgrading the protocol name breaks the unauthorized pipelining test. */
    if (strcasecmp(state->protocol, MAIL_PROTO_ESMTP) != 0
	&& strcasecmp(state->protocol, MAIL_PROTO_SMTP) != 0) {
	myfree(state->protocol);
	state->protocol = mystrdup(MAIL_PROTO_SMTP);
    }
    smtpd_chat_reply(state, "250 %s", var_myhostname);
    return (0);
}

/* ehlo_cmd - process EHLO command */

static int ehlo_cmd(SMTPD_STATE *state, int argc, SMTPD_TOKEN *argv)
{
    char   *err;

    /*
     * XXX 2821 new feature: Section 4.1.4 specifies that a server must clear
     * all buffers and reset the state exactly as if a RSET command had been
     * issued.
     */
    if (argc < 2) {
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	smtpd_chat_reply(state, "501 Syntax: EHLO hostname");
	return (-1);
    }
    if (argc > 2)
	collapse_args(argc - 1, argv + 1);
    if (SMTPD_STAND_ALONE(state) == 0
	&& var_smtpd_delay_reject == 0
	&& (err = smtpd_check_helo(state, argv[1].strval)) != 0) {
	smtpd_chat_reply(state, "%s", err);
	return (-1);
    }
    if (state->helo_name != 0)
	helo_reset(state);
    chat_reset(state, var_smtpd_hist_thrsh);
    mail_reset(state);
    rcpt_reset(state);
    state->helo_name = mystrdup(printable(argv[1].strval, '?'));
    neuter(state->helo_name, "<>()\\\";:@", '?');
    if (strcasecmp(state->protocol, MAIL_PROTO_ESMTP) != 0) {
	myfree(state->protocol);
	state->protocol = mystrdup(MAIL_PROTO_ESMTP);
    }
    smtpd_chat_reply(state, "250-%s", var_myhostname);
    smtpd_chat_reply(state, "250-PIPELINING");
    if (var_message_limit)
	smtpd_chat_reply(state, "250-SIZE %lu",
			 (unsigned long) var_message_limit);	/* XXX */
    else
	smtpd_chat_reply(state, "250-SIZE");
    if (var_disable_vrfy_cmd == 0)
	smtpd_chat_reply(state, "250-VRFY");
    smtpd_chat_reply(state, "250-ETRN");
#ifdef USE_SASL_AUTH
    if (var_smtpd_sasl_enable && !sasl_client_exception(state)) {
	smtpd_chat_reply(state, "250-AUTH %s", state->sasl_mechanism_list);
	if (var_broken_auth_clients)
	    smtpd_chat_reply(state, "250-AUTH=%s", state->sasl_mechanism_list);
    }
#endif
    if (namadr_list_match(verp_clients, state->name, state->addr))
	smtpd_chat_reply(state, "250-%s", VERP_CMD);
    /* XCLIENT must not override its own access control. */
    if (xclient_allowed)
	smtpd_chat_reply(state, "250-%s", XCLIENT_CMD);
    smtpd_chat_reply(state, "250 8BITMIME");
    return (0);
}

/* helo_reset - reset HELO/EHLO command stuff */

static void helo_reset(SMTPD_STATE *state)
{
    if (state->helo_name)
	myfree(state->helo_name);
    state->helo_name = 0;
}

/* mail_open_stream - open mail queue file or IPC stream */

static void mail_open_stream(SMTPD_STATE *state)
{
    char   *postdrop_command;
    int     cleanup_flags;

    /*
     * XXX 2821: An SMTP server is not allowed to "clean up" mail except in
     * the case of original submissions. Presently, Postfix always runs all
     * mail through the cleanup server.
     * 
     * We could approximate the RFC as follows: Postfix rewrites mail if it
     * comes from a source that we are willing to relay for. This way, we
     * avoid rewriting most mail that comes from elsewhere. However, that
     * requires moving functionality away from the cleanup daemon elsewhere,
     * such as virtual address expansion, and header/body pattern matching.
     */

    /*
     * If running from the master or from inetd, connect to the cleanup
     * service.
     */
    cleanup_flags = CLEANUP_FLAG_MASK_EXTERNAL;
    if (smtpd_input_transp_mask & INPUT_TRANSP_ADDRESS_MAPPING)
	cleanup_flags &= ~(CLEANUP_FLAG_BCC_OK | CLEANUP_FLAG_MAP_OK);
    if (smtpd_input_transp_mask & INPUT_TRANSP_HEADER_BODY)
	cleanup_flags &= ~CLEANUP_FLAG_FILTER;

    if (SMTPD_STAND_ALONE(state) == 0) {
	state->dest = mail_stream_service(MAIL_CLASS_PUBLIC,
					  var_cleanup_service);
	if (state->dest == 0
	    || attr_print(state->dest->stream, ATTR_FLAG_NONE,
			  ATTR_TYPE_NUM, MAIL_ATTR_FLAGS, cleanup_flags,
			  ATTR_TYPE_END) != 0)
	    msg_fatal("unable to connect to the %s %s service",
		      MAIL_CLASS_PUBLIC, var_cleanup_service);
    }

    /*
     * Otherwise, pipe the message through the privileged postdrop helper.
     * XXX Make postdrop a manifest constant.
     */
    else {
	postdrop_command = concatenate(var_command_dir, "/postdrop",
			      msg_verbose ? " -v" : (char *) 0, (char *) 0);
	state->dest = mail_stream_command(postdrop_command);
	if (state->dest == 0)
	    msg_fatal("unable to execute %s", postdrop_command);
	myfree(postdrop_command);
    }
    state->cleanup = state->dest->stream;
    state->queue_id = mystrdup(state->dest->id);

    /*
     * Log the queue ID with the message origin.
     */
#ifdef USE_SASL_AUTH
    if (var_smtpd_sasl_enable)
	smtpd_sasl_mail_log(state);
    else
#endif
	msg_info("%s: client=%s", state->queue_id, FORWARD_NAMADDR(state));

    /*
     * Record the time of arrival, the sender envelope address, some session
     * information, and some additional attributes.
     */
    if (SMTPD_STAND_ALONE(state) == 0) {
	rec_fprintf(state->cleanup, REC_TYPE_TIME, "%ld", state->time);
	if (*var_filter_xport)
	    rec_fprintf(state->cleanup, REC_TYPE_FILT, "%s", var_filter_xport);
    }
    rec_fputs(state->cleanup, REC_TYPE_FROM, state->sender);
    if (state->encoding != 0)
	rec_fprintf(state->cleanup, REC_TYPE_ATTR, "%s=%s",
		    MAIL_ATTR_ENCODING, state->encoding);

    /*
     * Store the client attributes for logging purposes.
     */
    if (SMTPD_STAND_ALONE(state) == 0) {
	if (IS_AVAIL_CLIENT_NAME(FORWARD_NAME(state)))
	    rec_fprintf(state->cleanup, REC_TYPE_ATTR, "%s=%s",
			MAIL_ATTR_CLIENT_NAME, FORWARD_NAME(state));
	if (IS_AVAIL_CLIENT_ADDR(FORWARD_ADDR(state)))
	    rec_fprintf(state->cleanup, REC_TYPE_ATTR, "%s=%s",
			MAIL_ATTR_CLIENT_ADDR, FORWARD_ADDR(state));
	if (IS_AVAIL_CLIENT_NAMADDR(FORWARD_NAMADDR(state)))
	    rec_fprintf(state->cleanup, REC_TYPE_ATTR, "%s=%s",
			MAIL_ATTR_ORIGIN, FORWARD_NAMADDR(state));
	if (IS_AVAIL_CLIENT_HELO(FORWARD_HELO(state)))
	    rec_fprintf(state->cleanup, REC_TYPE_ATTR, "%s=%s",
			MAIL_ATTR_HELO_NAME, FORWARD_HELO(state));
	if (IS_AVAIL_CLIENT_PROTO(FORWARD_PROTO(state)))
	    rec_fprintf(state->cleanup, REC_TYPE_ATTR, "%s=%s",
			MAIL_ATTR_PROTO_NAME, FORWARD_PROTO(state));
    }
    if (state->verp_delims)
	rec_fputs(state->cleanup, REC_TYPE_VERP, state->verp_delims);
}

/* extract_addr - extract address from rubble */

static char *extract_addr(SMTPD_STATE *state, SMTPD_TOKEN *arg,
			          int allow_empty_addr, int strict_rfc821)
{
    char   *myname = "extract_addr";
    TOK822 *tree;
    TOK822 *tp;
    TOK822 *addr = 0;
    int     naddr;
    int     non_addr;
    char   *err = 0;
    char   *junk = 0;
    char   *text;
    char   *colon;

    /*
     * Special case.
     */
#define PERMIT_EMPTY_ADDR	1
#define REJECT_EMPTY_ADDR	0

    /*
     * Some mailers send RFC822-style address forms (with comments and such)
     * in SMTP envelopes. We cannot blame users for this: the blame is with
     * programmers violating the RFC, and with sendmail for being permissive.
     * 
     * XXX The SMTP command tokenizer must leave the address in externalized
     * (quoted) form, so that the address parser can correctly extract the
     * address from surrounding junk.
     * 
     * XXX We have only one address parser, written according to the rules of
     * RFC 822. That standard differs subtly from RFC 821.
     */
    if (msg_verbose)
	msg_info("%s: input: %s", myname, STR(arg->vstrval));
    if (STR(arg->vstrval)[0] == '<'
	&& STR(arg->vstrval)[LEN(arg->vstrval) - 1] == '>') {
	junk = text = mystrndup(STR(arg->vstrval) + 1, LEN(arg->vstrval) - 2);
    } else
	text = STR(arg->vstrval);

    /*
     * Truncate deprecated route address form.
     */
    if (*text == '@' && (colon = strchr(text, ':')) != 0)
	text = colon + 1;
    tree = tok822_parse(text);

    if (junk)
	myfree(junk);

    /*
     * Find trouble.
     */
    for (naddr = non_addr = 0, tp = tree; tp != 0; tp = tp->next) {
	if (tp->type == TOK822_ADDR) {
	    addr = tp;
	    naddr += 1;				/* count address forms */
	} else if (tp->type == '<' || tp->type == '>') {
	     /* void */ ;			/* ignore brackets */
	} else {
	    non_addr += 1;			/* count non-address forms */
	}
    }

    /*
     * Report trouble. Log a warning only if we are going to sleep+reject so
     * that attackers can't flood our logfiles.
     */
    if (naddr > 1
	|| (strict_rfc821 && (non_addr || *STR(arg->vstrval) != '<'))) {
	msg_warn("Illegal address syntax from %s in %s command: %s",
		 state->namaddr, state->where, STR(arg->vstrval));
	err = "501 Bad address syntax";
    }

    /*
     * Overwrite the input with the extracted address. This seems bad design,
     * but we really are not going to use the original data anymore. What we
     * start with is quoted (external) form, and what we need is unquoted
     * (internal form).
     */
    if (addr)
	tok822_internalize(arg->vstrval, addr->head, TOK822_STR_DEFL);
    else
	vstring_strcpy(arg->vstrval, "");
    arg->strval = STR(arg->vstrval);

    /*
     * Report trouble. Log a warning only if we are going to sleep+reject so
     * that attackers can't flood our logfiles.
     */
    if ((arg->strval[0] == 0 && !allow_empty_addr)
	|| (strict_rfc821 && arg->strval[0] == '@')) {
	msg_warn("Illegal address syntax from %s in %s command: %s",
		 state->namaddr, state->where, STR(arg->vstrval));
	err = "501 Bad address syntax";
    }

    /*
     * Cleanup.
     */
    tok822_free_tree(tree);
    if (msg_verbose)
	msg_info("%s: result: %s", myname, STR(arg->vstrval));
    return (err);
}

/* mail_cmd - process MAIL command */

static int mail_cmd(SMTPD_STATE *state, int argc, SMTPD_TOKEN *argv)
{
    char   *err;
    int     narg;
    char   *arg;
    char   *verp_delims = 0;

    state->encoding = 0;
    state->msg_size = 0;

    /*
     * Sanity checks.
     * 
     * XXX 2821 pedantism: Section 4.1.2 says that SMTP servers that receive a
     * command in which invalid character codes have been employed, and for
     * which there are no other reasons for rejection, MUST reject that
     * command with a 501 response. So much for the principle of "be liberal
     * in what you accept, be strict in what you send".
     */
    if (var_helo_required && state->helo_name == 0) {
	state->error_mask |= MAIL_ERROR_POLICY;
	smtpd_chat_reply(state, "503 Error: send HELO/EHLO first");
	return (-1);
    }
#define IN_MAIL_TRANSACTION(state) ((state)->sender != 0)

    if (IN_MAIL_TRANSACTION(state)) {
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	smtpd_chat_reply(state, "503 Error: nested MAIL command");
	return (-1);
    }
    if (argc < 3
	|| strcasecmp(argv[1].strval, "from:") != 0) {
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	smtpd_chat_reply(state, "501 Syntax: MAIL FROM: <address>");
	return (-1);
    }
    if (argv[2].tokval == SMTPD_TOK_ERROR) {
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	smtpd_chat_reply(state, "501 Bad sender address syntax");
	return (-1);
    }
    if ((err = extract_addr(state, argv + 2, PERMIT_EMPTY_ADDR, var_strict_rfc821_env)) != 0) {
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	smtpd_chat_reply(state, "%s", err);
	return (-1);
    }
    for (narg = 3; narg < argc; narg++) {
	arg = argv[narg].strval;
	if (strcasecmp(arg, "BODY=8BITMIME") == 0) {	/* RFC 1652 */
	    state->encoding = MAIL_ATTR_ENC_8BIT;
	} else if (strcasecmp(arg, "BODY=7BIT") == 0) {	/* RFC 1652 */
	    state->encoding = MAIL_ATTR_ENC_7BIT;
	} else if (strncasecmp(arg, "SIZE=", 5) == 0) {	/* RFC 1870 */
	    /* Reject non-numeric size. */
	    if (!alldig(arg + 5)) {
		state->error_mask |= MAIL_ERROR_PROTOCOL;
		smtpd_chat_reply(state, "501 Bad message size syntax");
		return (-1);
	    }
	    /* Reject size overflow. */
	    if ((state->msg_size = off_cvt_string(arg + 5)) < 0) {
		smtpd_chat_reply(state, "552 Message size exceeds file system imposed limit");
		state->error_mask |= MAIL_ERROR_POLICY;
		return (-1);
	    }
#ifdef USE_SASL_AUTH
	} else if (var_smtpd_sasl_enable && strncasecmp(arg, "AUTH=", 5) == 0) {
	    if ((err = smtpd_sasl_mail_opt(state, arg + 5)) != 0) {
		smtpd_chat_reply(state, "%s", err);
		return (-1);
	    }
#endif
	} else if (namadr_list_match(verp_clients, state->name, state->addr)
		   && strncasecmp(arg, VERP_CMD, VERP_CMD_LEN) == 0
		   && (arg[VERP_CMD_LEN] == '=' || arg[VERP_CMD_LEN] == 0)) {
	    if (arg[VERP_CMD_LEN] == 0) {
		verp_delims = var_verp_delims;
	    } else {
		verp_delims = arg + VERP_CMD_LEN + 1;
		if (verp_delims_verify(verp_delims) != 0) {
		    state->error_mask |= MAIL_ERROR_PROTOCOL;
		    smtpd_chat_reply(state, "501 Error: %s needs two characters from %s",
				     VERP_CMD, var_verp_filter);
		    return (-1);
		}
	    }
	} else {
	    state->error_mask |= MAIL_ERROR_PROTOCOL;
	    smtpd_chat_reply(state, "555 Unsupported option: %s", arg);
	    return (-1);
	}
    }
    if (verp_delims && argv[2].strval[0] == 0) {
	smtpd_chat_reply(state, "503 Error: %s requires non-null sender",
			 VERP_CMD);
	return (-1);
    }
    if (SMTPD_STAND_ALONE(state) == 0
	&& var_smtpd_delay_reject == 0
	&& (err = smtpd_check_mail(state, argv[2].strval)) != 0) {
	smtpd_chat_reply(state, "%s", err);
	return (-1);
    }
    state->time = time((time_t *) 0);

    /*
     * Check the queue file space, if applicable.
     */
#define USE_SMTPD_PROXY(state) \
	(SMTPD_STAND_ALONE(state) == 0 && *var_smtpd_proxy_filt)

    if (!USE_SMTPD_PROXY(state)) {
	if ((err = smtpd_check_size(state, state->msg_size)) != 0) {
	    smtpd_chat_reply(state, "%s", err);
	    return (-1);
	}
    }

    /*
     * No more early returns. The mail transaction is in progress.
     */
    state->sender = mystrdup(argv[2].strval);
    if (verp_delims)
	state->verp_delims = mystrdup(verp_delims);
    if (USE_SMTPD_PROXY(state))
	state->proxy_mail = mystrdup(STR(state->buffer));
    smtpd_chat_reply(state, "250 Ok");
    return (0);
}

/* mail_reset - reset MAIL command stuff */

static void mail_reset(SMTPD_STATE *state)
{

    /*
     * Unceremoniously close the pipe to the cleanup service. The cleanup
     * service will delete the queue file when it detects a premature
     * end-of-file condition on input.
     */
    if (state->cleanup != 0) {
	mail_stream_cleanup(state->dest);
	state->dest = 0;
	state->cleanup = 0;
    }
    state->err = 0;
    if (state->queue_id != 0) {
	myfree(state->queue_id);
	state->queue_id = 0;
    }
    if (state->sender) {
	myfree(state->sender);
	state->sender = 0;
    }
    if (state->verp_delims) {
	myfree(state->verp_delims);
	state->verp_delims = 0;
    }
#ifdef USE_SASL_AUTH
    if (var_smtpd_sasl_enable)
	smtpd_sasl_mail_reset(state);
#endif
    state->discard = 0;

    /*
     * Try to be nice. Don't bother when we lost the connection. Don't bother
     * waiting for a reply, it just increases latency.
     */
    if (state->proxy) {
	(void) smtpd_proxy_cmd(state, SMTPD_PROX_WANT_NONE, "QUIT");
	smtpd_proxy_close(state);
    }
    if (state->proxy_mail) {
	myfree(state->proxy_mail);
	state->proxy_mail = 0;
    }
    if (state->xclient.used)
	smtpd_xclient_reset(state);
}

/* rcpt_cmd - process RCPT TO command */

static int rcpt_cmd(SMTPD_STATE *state, int argc, SMTPD_TOKEN *argv)
{
    char   *err;
    int     narg;
    char   *arg;

    /*
     * Sanity checks.
     * 
     * XXX 2821 pedantism: Section 4.1.2 says that SMTP servers that receive a
     * command in which invalid character codes have been employed, and for
     * which there are no other reasons for rejection, MUST reject that
     * command with a 501 response. So much for the principle of "be liberal
     * in what you accept, be strict in what you send".
     */
    if (!IN_MAIL_TRANSACTION(state)) {
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	smtpd_chat_reply(state, "503 Error: need MAIL command");
	return (-1);
    }
    if (argc < 3
	|| strcasecmp(argv[1].strval, "to:") != 0) {
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	smtpd_chat_reply(state, "501 Syntax: RCPT TO: <address>");
	return (-1);
    }
    if (argv[2].tokval == SMTPD_TOK_ERROR) {
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	smtpd_chat_reply(state, "501 Bad recipient address syntax");
	return (-1);
    }
    if ((err = extract_addr(state, argv + 2, REJECT_EMPTY_ADDR, var_strict_rfc821_env)) != 0) {
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	smtpd_chat_reply(state, "%s", err);
	return (-1);
    }
    for (narg = 3; narg < argc; narg++) {
	arg = argv[narg].strval;
	if (1) {
	    state->error_mask |= MAIL_ERROR_PROTOCOL;
	    smtpd_chat_reply(state, "555 Unsupported option: %s", arg);
	    return (-1);
	}
    }
    if (var_smtpd_rcpt_limit && state->rcpt_count >= var_smtpd_rcpt_limit) {
	state->error_mask |= MAIL_ERROR_POLICY;
	smtpd_chat_reply(state, "452 Error: too many recipients");
	return (-1);
    }
    if (SMTPD_STAND_ALONE(state) == 0) {
	if ((err = smtpd_check_rcpt(state, argv[2].strval)) != 0) {
	    smtpd_chat_reply(state, "%s", err);
	    return (-1);
	}
    }

    /*
     * Don't access the proxy, queue file, or queue file writer process until
     * we have a valid recipient address.
     */
    if (state->proxy == 0 && state->proxy_mail) {
	if (smtpd_proxy_open(state, var_smtpd_proxy_filt,
			     var_smtpd_proxy_tmout, var_smtpd_proxy_ehlo,
			     state->proxy_mail) != 0) {
	    smtpd_chat_reply(state, "%s", STR(state->proxy_buffer));
	    return (-1);
	}
    } else if (state->cleanup == 0) {
	mail_open_stream(state);
    }
    if (state->proxy && smtpd_proxy_cmd(state, SMTPD_PROX_WANT_OK,
					"%s", STR(state->buffer)) != 0) {
	smtpd_chat_reply(state, "%s", STR(state->proxy_buffer));
	return (-1);
    }

    /*
     * Store the recipient. Remember the first one.
     */
    state->rcpt_count++;
    if (state->recipient == 0)
	state->recipient = mystrdup(argv[2].strval);
    if (state->cleanup)
	rec_fputs(state->cleanup, REC_TYPE_RCPT, argv[2].strval);
    smtpd_chat_reply(state, "250 Ok");
    return (0);
}

/* rcpt_reset - reset RCPT stuff */

static void rcpt_reset(SMTPD_STATE *state)
{
    if (state->recipient) {
	myfree(state->recipient);
	state->recipient = 0;
    }
    state->rcpt_count = 0;
}

/* data_cmd - process DATA command */

static int data_cmd(SMTPD_STATE *state, int argc, SMTPD_TOKEN *unused_argv)
{
    char   *err;
    char   *start;
    int     len;
    int     curr_rec_type;
    int     prev_rec_type;
    int     first = 1;
    VSTRING *why = 0;
    int     saved_err;
    int     (*out_record) (VSTREAM *, int, const char *, int);
    int     (*out_fprintf) (VSTREAM *, int, const char *,...);
    VSTREAM *out_stream;
    int     out_error;

    /*
     * Sanity checks. With ESMTP command pipelining the client can send DATA
     * before all recipients are rejected, so don't report that as a protocol
     * error.
     */
    if (state->rcpt_count == 0) {
	if (!IN_MAIL_TRANSACTION(state)) {
	    state->error_mask |= MAIL_ERROR_PROTOCOL;
	    smtpd_chat_reply(state, "503 Error: need RCPT command");
	} else {
	    smtpd_chat_reply(state, "554 Error: no valid recipients");
	}
	return (-1);
    }
    if (argc != 1) {
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	smtpd_chat_reply(state, "501 Syntax: DATA");
	return (-1);
    }
    if (SMTPD_STAND_ALONE(state) == 0 && (err = smtpd_check_data(state)) != 0) {
	smtpd_chat_reply(state, "%s", err);
	return (-1);
    }
    if (state->proxy && smtpd_proxy_cmd(state, SMTPD_PROX_WANT_MORE,
					"%s", STR(state->buffer)) != 0) {
	smtpd_chat_reply(state, "%s", STR(state->proxy_buffer));
	return (-1);
    }

    /*
     * One level of indirection to choose between normal or proxied
     * operation. We want to avoid massive code duplication within tons of
     * if-else clauses.
     */
    if (state->proxy) {
	out_stream = state->proxy;
	out_record = smtpd_proxy_rec_put;
	out_fprintf = smtpd_proxy_rec_fprintf;
	out_error = CLEANUP_STAT_PROXY;
    } else {
	out_stream = state->cleanup;
	out_record = rec_put;
	out_fprintf = rec_fprintf;
	out_error = CLEANUP_STAT_WRITE;
    }

    /*
     * Terminate the message envelope segment. Start the message content
     * segment, and prepend our own Received: header. If there is only one
     * recipient, list the recipient address.
     * 
     * Suppress our own Received: header in the unlikely case that we are an
     * intermediate proxy.
     */
    if (state->cleanup)
	rec_fputs(state->cleanup, REC_TYPE_MESG, "");
    if (!state->proxy || state->xclient.used == 0) {
	out_fprintf(out_stream, REC_TYPE_NORM,
		    "Received: from %s (%s [%s])",
		    state->helo_name ? state->helo_name : state->name,
		    state->name, state->addr);
	if (state->rcpt_count == 1 && state->recipient) {
	    out_fprintf(out_stream, REC_TYPE_NORM,
			state->cleanup ? "\tby %s (%s) with %s id %s" :
			"\tby %s (%s) with %s",
			var_myhostname, var_mail_name,
			state->protocol, state->queue_id);
	    quote_822_local(state->buffer, state->recipient);
	    out_fprintf(out_stream, REC_TYPE_NORM,
	      "\tfor <%s>; %s", STR(state->buffer), mail_date(state->time));
	} else {
	    out_fprintf(out_stream, REC_TYPE_NORM,
			state->cleanup ? "\tby %s (%s) with %s id %s;" :
			"\tby %s (%s) with %s;",
			var_myhostname, var_mail_name,
			state->protocol, state->queue_id);
	    out_fprintf(out_stream, REC_TYPE_NORM,
			"\t%s", mail_date(state->time));
	}
#ifdef RECEIVED_ENVELOPE_FROM
	quote_822_local(state->buffer, state->sender);
	out_fprintf(out_stream, REC_TYPE_NORM,
		    "\t(envelope-from %s)", STR(state->buffer));
#endif
    }
    smtpd_chat_reply(state, "354 End data with <CR><LF>.<CR><LF>");

    /*
     * Copy the message content. If the cleanup process has a problem, keep
     * reading until the remote stops sending, then complain. Read typed
     * records from the SMTP stream so we can handle data that spans buffers.
     * 
     * XXX Force an empty record when the queue file content begins with
     * whitespace, so that it won't be considered as being part of our own
     * Received: header. What an ugly Kluge.
     * 
     * XXX Deal with UNIX-style From_ lines at the start of message content
     * because sendmail permits it.
     */
    for (prev_rec_type = 0; /* void */ ; prev_rec_type = curr_rec_type) {
	if (smtp_get(state->buffer, state->client, var_line_limit) == '\n')
	    curr_rec_type = REC_TYPE_NORM;
	else
	    curr_rec_type = REC_TYPE_CONT;
	start = vstring_str(state->buffer);
	len = VSTRING_LEN(state->buffer);
	if (first) {
	    if (strncmp(start + strspn(start, ">"), "From ", 5) == 0) {
		out_fprintf(out_stream, curr_rec_type,
			    "X-Mailbox-Line: %s", start);
		continue;
	    }
	    first = 0;
	    if (len > 0 && IS_SPACE_TAB(start[0]))
		out_record(out_stream, REC_TYPE_NORM, "", 0);
	}
	if (prev_rec_type != REC_TYPE_CONT && *start == '.'
	    && (state->proxy == 0 ? (++start, --len) == 0 : len == 1))
	    break;
	if (state->err == CLEANUP_STAT_OK
	    && out_record(out_stream, curr_rec_type, start, len) < 0)
	    state->err = out_error;
    }

    /*
     * Send the end of DATA and finish the proxy connection. Set the
     * CLEANUP_STAT_PROXY error flag in case of trouble.
     */
    if (state->proxy) {
	if (state->err == CLEANUP_STAT_OK) {
	    (void) smtpd_proxy_cmd(state, SMTPD_PROX_WANT_ANY, ".");
	    if (state->err == CLEANUP_STAT_OK &&
		*STR(state->proxy_buffer) != '2')
		state->err = CLEANUP_STAT_CONT;
	}
	smtpd_proxy_close(state);
    }

    /*
     * Send the end-of-segment markers and finish the queue file record
     * stream.
     */
    else {
	if (state->err == CLEANUP_STAT_OK)
	    if (rec_fputs(state->cleanup, REC_TYPE_XTRA, "") < 0
		|| rec_fputs(state->cleanup, REC_TYPE_END, "") < 0
		|| vstream_fflush(state->cleanup))
		state->err = CLEANUP_STAT_WRITE;
	if (state->err == 0) {
	    why = vstring_alloc(10);
	    state->err = mail_stream_finish(state->dest, why);
	} else
	    mail_stream_cleanup(state->dest);
	state->dest = 0;
	state->cleanup = 0;
    }

    /*
     * Handle any errors. One message may suffer from multiple errors, so
     * complain only about the most severe error. Forgive any previous client
     * errors when a message was received successfully.
     * 
     * See also: qmqpd.c
     */
    if (state->err == CLEANUP_STAT_OK) {
	state->error_count = 0;
	state->error_mask = 0;
	state->junk_cmds = 0;
	if (state->queue_id)
	    smtpd_chat_reply(state, "250 Ok: queued as %s", state->queue_id);
	else
	    smtpd_chat_reply(state, "%s", STR(state->proxy_buffer));
    } else if ((state->err & CLEANUP_STAT_BAD) != 0) {
	state->error_mask |= MAIL_ERROR_SOFTWARE;
	smtpd_chat_reply(state, "451 Error: internal error %d", state->err);
    } else if ((state->err & CLEANUP_STAT_SIZE) != 0) {
	state->error_mask |= MAIL_ERROR_BOUNCE;
	smtpd_chat_reply(state, "552 Error: message too large");
    } else if ((state->err & CLEANUP_STAT_HOPS) != 0) {
	state->error_mask |= MAIL_ERROR_BOUNCE;
	smtpd_chat_reply(state, "554 Error: too many hops");
    } else if ((state->err & CLEANUP_STAT_CONT) != 0) {
	state->error_mask |= MAIL_ERROR_POLICY;
	if (state->proxy_buffer)
	    smtpd_chat_reply(state, "%s", STR(state->proxy_buffer));
	else
	    smtpd_chat_reply(state, "550 Error: %s", LEN(why) ?
			     STR(why) : "content rejected");
    } else if ((state->err & CLEANUP_STAT_WRITE) != 0) {
	state->error_mask |= MAIL_ERROR_RESOURCE;
	smtpd_chat_reply(state, "451 Error: queue file write error");
    } else if ((state->err & CLEANUP_STAT_PROXY) != 0) {
	state->error_mask |= MAIL_ERROR_SOFTWARE;
	smtpd_chat_reply(state, "%s", STR(state->proxy_buffer));
    } else {
	state->error_mask |= MAIL_ERROR_SOFTWARE;
	smtpd_chat_reply(state, "451 Error: internal error %d", state->err);
    }

    /*
     * Disconnect after transmission must not be treated as "lost connection
     * after DATA".
     */
    state->where = SMTPD_AFTER_DOT;

    /*
     * Cleanup. The client may send another MAIL command.
     */
    saved_err = state->err;
    chat_reset(state, var_smtpd_hist_thrsh);
    mail_reset(state);
    rcpt_reset(state);
    if (why)
	vstring_free(why);
    return (saved_err);
}

/* rset_cmd - process RSET */

static int rset_cmd(SMTPD_STATE *state, int argc, SMTPD_TOKEN *unused_argv)
{

    /*
     * Sanity checks.
     */
    if (argc != 1) {
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	smtpd_chat_reply(state, "501 Syntax: RSET");
	return (-1);
    }

    /*
     * Restore state to right after HELO/EHLO command.
     */
    chat_reset(state, var_smtpd_hist_thrsh);
    mail_reset(state);
    rcpt_reset(state);
    smtpd_chat_reply(state, "250 Ok");
    return (0);
}

/* noop_cmd - process NOOP */

static int noop_cmd(SMTPD_STATE *state, int argc, SMTPD_TOKEN *unused_argv)
{

    /*
     * XXX 2821 incompatibility: Section 4.1.1.9 says that NOOP can have a
     * parameter string which is to be ignored. NOOP instructions with
     * parameters? Go figure.
     * 
     * RFC 2821 violates RFC 821, which says that NOOP takes no parameters.
     */
#ifdef RFC821_SYNTAX

    /*
     * Sanity checks.
     */
    if (argc != 1) {
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	smtpd_chat_reply(state, "501 Syntax: NOOP");
	return (-1);
    }
#endif
    smtpd_chat_reply(state, "250 Ok");
    return (0);
}

/* vrfy_cmd - process VRFY */

static int vrfy_cmd(SMTPD_STATE *state, int argc, SMTPD_TOKEN *argv)
{
    char   *err = 0;

    /*
     * The SMTP standard (RFC 821) disallows unquoted special characters in
     * the VRFY argument. Common practice violates the standard, however.
     * Postfix accomodates common practice where it violates the standard.
     * 
     * XXX Impedance mismatch! The SMTP command tokenizer preserves quoting,
     * whereas the recipient restrictions checks expect unquoted (internal)
     * address forms. Therefore we must parse out the address, or we must
     * stop doing recipient restriction checks and lose the opportunity to
     * say "user unknown" at the SMTP port.
     * 
     * XXX 2821 incompatibility and brain damage: Section 4.5.1 requires that
     * VRFY is implemented. RFC 821 specifies that VRFY is optional. It gets
     * even worse: section 3.5.3 says that a 502 (command recognized but not
     * implemented) reply is not fully compliant.
     * 
     * Thus, an RFC 2821 compliant implementation cannot refuse to supply
     * information in reply to VRFY queries. That is simply bogus. The only
     * reply we could supply is a generic 252 reply. This causes spammers to
     * add tons of bogus addresses to their mailing lists (spam harvesting by
     * trying out large lists of potential recipient names with VRFY).
     */
#define SLOPPY	0

    if (var_disable_vrfy_cmd) {
	state->error_mask |= MAIL_ERROR_POLICY;
	smtpd_chat_reply(state, "502 VRFY command is disabled");
	return (-1);
    }
    if (argc < 2) {
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	smtpd_chat_reply(state, "501 Syntax: VRFY address");
	return (-1);
    }
    if (argc > 2)
	collapse_args(argc - 1, argv + 1);
    if ((err = extract_addr(state, argv + 1, REJECT_EMPTY_ADDR, SLOPPY)) != 0) {
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	smtpd_chat_reply(state, "%s", err);
	return (-1);
    }
    if (SMTPD_STAND_ALONE(state) == 0
	&& (err = smtpd_check_rcpt(state, argv[1].strval)) != 0) {
	smtpd_chat_reply(state, "%s", err);
	return (-1);
    }

    /*
     * XXX 2821 new feature: Section 3.5.1 requires that the VRFY response is
     * either "full name <user@domain>" or "user@domain". Postfix replies
     * with the address that was provided by the client, whether or not it is
     * in fully qualified domain form or not.
     * 
     * Reply code 250 is reserved for the case where the address is verified;
     * reply code 252 should be used when no definitive certainty exists.
     */
    smtpd_chat_reply(state, "252 %s", argv[1].strval);
    return (0);
}

/* etrn_cmd - process ETRN command */

static int etrn_cmd(SMTPD_STATE *state, int argc, SMTPD_TOKEN *argv)
{
    char   *err;

    /*
     * Sanity checks.
     */
    if (var_helo_required && state->helo_name == 0) {
	state->error_mask |= MAIL_ERROR_POLICY;
	smtpd_chat_reply(state, "503 Error: send HELO/EHLO first");
	return (-1);
    }
    if (IN_MAIL_TRANSACTION(state)) {
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	smtpd_chat_reply(state, "503 Error: MAIL transaction in progress");
	return (-1);
    }
    if (argc != 2) {
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	smtpd_chat_reply(state, "500 Syntax: ETRN domain");
	return (-1);
    }
    if (!ISALNUM(argv[1].strval[0]))
	argv[1].strval++;
    if (!valid_hostname(argv[1].strval, DONT_GRIPE)) {
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	smtpd_chat_reply(state, "501 Error: invalid parameter syntax");
	return (-1);
    }

    /*
     * XXX The implementation borrows heavily from the code that implements
     * UCE restrictions. These typically return 450 or 550 when a request is
     * rejected. RFC 1985 requires that 459 be sent when the server refuses
     * to perform the request.
     */
    if (SMTPD_STAND_ALONE(state)) {
	msg_warn("do not use ETRN in \"sendmail -bs\" mode");
	smtpd_chat_reply(state, "458 Unable to queue messages");
	return (-1);
    }
    if ((err = smtpd_check_etrn(state, argv[1].strval)) != 0) {
	smtpd_chat_reply(state, "%s", err);
	return (-1);
    }
    switch (flush_send(argv[1].strval)) {
    case FLUSH_STAT_OK:
	smtpd_chat_reply(state, "250 Queuing started");
	return (0);
    case FLUSH_STAT_DENY:
	msg_warn("reject: ETRN %.100s... from %s",
		 argv[1].strval, state->namaddr);
	smtpd_chat_reply(state, "459 <%s>: service unavailable",
			 argv[1].strval);
	return (-1);
    case FLUSH_STAT_BAD:
	msg_warn("bad ETRN %.100s... from %s", argv[1].strval, state->namaddr);
	smtpd_chat_reply(state, "458 Unable to queue messages");
	return (-1);
    default:
	msg_warn("unable to talk to fast flush service");
	smtpd_chat_reply(state, "458 Unable to queue messages");
	return (-1);
    }
}

/* quit_cmd - process QUIT command */

static int quit_cmd(SMTPD_STATE *state, int unused_argc, SMTPD_TOKEN *unused_argv)
{

    /*
     * Don't bother checking the syntax.
     */
    smtpd_chat_reply(state, "221 Bye");

    /*
     * When the "." and quit replies are pipelined, make sure they are
     * flushed now, to avoid repeated mail deliveries in case of a crash in
     * the "clean up before disconnect" code.
     */
    vstream_fflush(state->client);
    return (0);
}

 /*
  * Lookup tables with xclient/normal attribute offsets. Maybe we should not
  * try so hard to make XOVERRIDE and XFORWARD attribute lists identical.
  */
#define FUNC_OVERRIDE	0
#define FUNC_FORWARD	1

struct attr_offset {
    int     name[2];
    int     addr[2];
    int     namaddr[2];
    int     peer_code[2];
    int     protocol[2];
    int     helo_name[2];
};

#define ATTR_OFFSETS(member) \
	offsetof(SMTPD_STATE, member), offsetof(SMTPD_STATE, xclient.member)

static const struct attr_offset attr_offset = {
    ATTR_OFFSETS(name),
    ATTR_OFFSETS(addr),
    ATTR_OFFSETS(namaddr),
    ATTR_OFFSETS(peer_code),
    ATTR_OFFSETS(protocol),
    ATTR_OFFSETS(helo_name)
};

#define PTR_ATTR(state, func, attr) (((char *) state) + attr_offset.attr[func])
#define STR_ATTR(state, func, attr) *((char **) PTR_ATTR(state, func, attr))
#define INT_ATTR(state, func, attr) *((int *) PTR_ATTR(state, func, attr))

#define RST_STR_ATTR(state, func, attr) { \
	if (STR_ATTR(state, func, attr)) \
	    myfree(STR_ATTR(state, func, attr)); \
	STR_ATTR(state, func, attr) = 0; \
    }

#define UPD_STR_ATTR(state, func, attr, value) { \
	if (STR_ATTR(state, func, attr)) \
	    myfree(STR_ATTR(state, func, attr)); \
	STR_ATTR(state, func, attr) = mystrdup(value); \
    }

#define UPD_INT_ATTR(state, func, attr, value) { \
	INT_ATTR(state, func, attr) = (value); \
    }

/* xclient_cmd - process XCLIENT */

static int xclient_cmd(SMTPD_STATE *state, int argc, SMTPD_TOKEN *argv)
{
    int     arg_no;
    char   *raw_value;
    char   *cooked_value;
    char   *arg_val;
    int     update_namaddr = 0;
    int     function;

    /*
     * Sanity checks. The XCLIENT command does not override its own access
     * control.
     */
    if (IN_MAIL_TRANSACTION(state)) {
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	smtpd_chat_reply(state, "503 Error: MAIL transaction in progress");
	return (-1);
    }
    if (argc < 3) {
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	smtpd_chat_reply(state, "501 Syntax: %s function name=value...",
			 XCLIENT_CMD);
	return (-1);
    }
    if (!xclient_allowed) {
	state->error_mask |= MAIL_ERROR_POLICY;
	smtpd_chat_reply(state, "554 Error: insufficient authorization");
	return (-1);
    }
#define STREQ(x,y) (strcasecmp((x), (y)) == 0)

    /*
     * Function name: what attributes to update. Complain about unrecognized
     * request names. The set of requests is unlikely to change.
     */
    arg_val = argv[1].strval;
    printable(arg_val, '?');
    if (STREQ(arg_val, XCLIENT_OVERRIDE)) {
	function = FUNC_OVERRIDE;
    } else if (STREQ(arg_val, XCLIENT_FORWARD)) {
	function = FUNC_FORWARD;
	if (state->xclient.used == 0)
	    smtpd_xclient_preset(state);
    } else {					/* error */
	state->error_mask |= MAIL_ERROR_PROTOCOL;
	smtpd_chat_reply(state, "501 Bad %s function: %s",
			 XCLIENT_CMD, arg_val);
	return (-1);
    }

    /*
     * Iterate over all NAME=VALUE attributes. An empty value means the
     * information was not provided by the client and that we must not fall
     * back to the non-XCLIENT value.
     */
    for (arg_no = 2; arg_no < argc; arg_no++) {
	arg_val = argv[arg_no].strval;

	/*
	 * Decode the attribute value and for safety's sake mask
	 * non-printable characters in the raw and decoded values; we don't
	 * want to handle unexploded munitions. Do not complain about
	 * unrecognized attribute names. The set of attributes may change
	 * over time.
	 * 
	 * The client can send multiple XCLIENT attributes in a single command,
	 * or multiple XCLIENT commands with fewer attributes.
	 * 
	 * Note: XCLIENT OVERRIDE overrides only the specified remote client
	 * attributes (for testing), while XCLIENT FORWARD overrides all
	 * remote client attributes (for consistency).
	 */
	if ((raw_value = split_at(arg_val, '=')) == 0) {
	    state->error_mask |= MAIL_ERROR_PROTOCOL;
	    smtpd_chat_reply(state, "501 Error: name=value expected");
	    return (-1);
	}
	if (xtext_unquote(state->buffer, raw_value) == 0) {
	    state->error_mask |= MAIL_ERROR_PROTOCOL;
	    smtpd_chat_reply(state, "501 Bad attribute value syntax: %s",
			     printable(raw_value, '?'));
	    return (-1);
	}
	cooked_value = printable(STR(state->buffer), '?');
	(void) printable(raw_value, '?');

	/*
	 * CLIENT_NAME=hostname. Also updates the client hostname lookup
	 * status code. Treat a numerical hostname as an unavailable name.
	 */
	if (STREQ(arg_val, XCLIENT_NAME)) {
	    if (*raw_value && !valid_hostaddr(cooked_value, DONT_GRIPE)) {
		if (!valid_hostname(cooked_value, DONT_GRIPE)) {
		    state->error_mask |= MAIL_ERROR_PROTOCOL;
		    smtpd_chat_reply(state, "501 Bad hostname syntax: %s",
				     cooked_value);
		    return (-1);
		}
		UPD_STR_ATTR(state, function, name, cooked_value);
		UPD_INT_ATTR(state, function, peer_code, SMTPD_PEER_CODE_OK);
	    } else {
		UPD_STR_ATTR(state, function, name, CLIENT_NAME_UNKNOWN);
		UPD_INT_ATTR(state, function, peer_code, SMTPD_PEER_CODE_PERM);
	    }
	    update_namaddr = 1;
	}

	/*
	 * CLIENT_ADDR=client network address.
	 */
	else if (STREQ(arg_val, XCLIENT_ADDR)) {
	    if (*raw_value) {
		if (!valid_hostaddr(cooked_value, DONT_GRIPE)) {
		    state->error_mask |= MAIL_ERROR_PROTOCOL;
		    smtpd_chat_reply(state, "501 Bad address syntax: %s",
				     cooked_value);
		    return (-1);
		}
		UPD_STR_ATTR(state, function, addr, cooked_value);
	    } else {
		UPD_STR_ATTR(state, function, addr, CLIENT_ADDR_UNKNOWN);
	    }
	    update_namaddr = 1;
	}

	/*
	 * CLIENT_CODE=hostname lookup status. Reset the client hostname if
	 * the hostname lookup status is not OK.
	 */
	else if (STREQ(arg_val, XCLIENT_CODE)) {
	    if (STREQ(cooked_value, "OK")) {
		UPD_INT_ATTR(state, function, peer_code, SMTPD_PEER_CODE_OK);
	    } else if (STREQ(cooked_value, "TEMP")) {
		UPD_INT_ATTR(state, function, peer_code, SMTPD_PEER_CODE_TEMP);
		UPD_STR_ATTR(state, function, name, CLIENT_NAME_UNKNOWN);
		update_namaddr = 1;
	    } else if (STREQ(cooked_value, "PERM")) {
		UPD_INT_ATTR(state, function, peer_code, SMTPD_PEER_CODE_PERM);
		UPD_STR_ATTR(state, function, name, CLIENT_NAME_UNKNOWN);
		update_namaddr = 1;
	    } else {
		state->error_mask |= MAIL_ERROR_PROTOCOL;
		smtpd_chat_reply(state, "501 Bad hostname status: %s",
				 cooked_value);
		return (-1);
	    }
	}

	/*
	 * CLIENT_HELO=hostname. Disallow characters that could mess up our
	 * own Received: message headers but allow [].
	 */
	else if (STREQ(arg_val, XCLIENT_HELO)) {
	    if (*raw_value) {
		if (strlen(cooked_value) > VALID_HOSTNAME_LEN) {
		    state->error_mask |= MAIL_ERROR_PROTOCOL;
		    smtpd_chat_reply(state, "501 Bad HELO syntax: %s",
				     cooked_value);
		    return (-1);
		}
		neuter(cooked_value, "<>()\\\";:@", '?');
		UPD_STR_ATTR(state, function, helo_name, cooked_value);
	    } else {
		RST_STR_ATTR(state, function, helo_name);
	    }
	}

	/*
	 * CLIENT_PROTO=protocol name. Disallow characters that could mess up
	 * our own Received: message headers.
	 */
	else if (STREQ(arg_val, XCLIENT_PROTO)) {
	    if (*raw_value) {
		if (*cooked_value == 0 || strlen(cooked_value) > 64) {
		    state->error_mask |= MAIL_ERROR_PROTOCOL;
		    smtpd_chat_reply(state, "501 Bad protocol syntax: %s",
				     cooked_value);
		    return (-1);
		}
		neuter(cooked_value, "[]<>()\\\";:@", '?');
		UPD_STR_ATTR(state, function, protocol, cooked_value);
	    } else {
		UPD_STR_ATTR(state, function, protocol, CLIENT_PROTO_UNKNOWN);
	    }
	}

	/*
	 * Unknown attribute name. Don't complain, and log a warning. Logging
	 * is safe because only authorized clients can issue XCLIENT
	 * commands.
	 */
	else {
	    msg_warn("unknown %s attribute from %s: %s=%s",
		     XCLIENT_CMD, state->namaddr, arg_val, cooked_value);
	}
    }

    /*
     * Update the combined name and address when either has changed.
     */
    if (update_namaddr) {
	if (STR_ATTR(state, function, namaddr))
	    myfree(STR_ATTR(state, function, namaddr));
	STR_ATTR(state, function, namaddr) =
	    concatenate(STR_ATTR(state, function, name), "[",
			STR_ATTR(state, function, addr), "]",
			(char *) 0);
    }
    smtpd_chat_reply(state, "250 Ok");
    return (0);
}

/* chat_reset - notify postmaster and reset conversation log */

static void chat_reset(SMTPD_STATE *state, int threshold)
{

    /*
     * Notify the postmaster if there were errors. This usually indicates a
     * client configuration problem, or that someone is trying nasty things.
     * Either is significant enough to bother the postmaster. XXX Can't
     * report problems when running in stand-alone mode: postmaster notices
     * require availability of the cleanup service.
     */
    if (state->history != 0 && state->history->argc > threshold) {
	if (SMTPD_STAND_ALONE(state) == 0
	    && (state->error_mask & state->notify_mask))
	    smtpd_chat_notify(state);
	state->error_mask = 0;
	smtpd_chat_reset(state);
    }
}

 /*
  * The table of all SMTP commands that we know. Set the junk limit flag on
  * any command that can be repeated an arbitrary number of times without
  * triggering a tarpit delay of some sort.
  */
typedef struct SMTPD_CMD {
    char   *name;
    int     (*action) (SMTPD_STATE *, int, SMTPD_TOKEN *);
    int     flags;
} SMTPD_CMD;

#define SMTPD_CMD_FLAG_LIMIT    (1<<0)	/* limit usage */
#define SMTPD_CMD_FLAG_FORBIDDEN	(1<<1)	/* RFC 2822 mail header */

static SMTPD_CMD smtpd_cmd_table[] = {
    "HELO", helo_cmd, SMTPD_CMD_FLAG_LIMIT,
    "EHLO", ehlo_cmd, SMTPD_CMD_FLAG_LIMIT,

#ifdef USE_SASL_AUTH
    "AUTH", smtpd_sasl_auth_cmd, 0,
#endif

    "MAIL", mail_cmd, 0,
    "RCPT", rcpt_cmd, 0,
    "DATA", data_cmd, 0,
    "RSET", rset_cmd, SMTPD_CMD_FLAG_LIMIT,
    "NOOP", noop_cmd, SMTPD_CMD_FLAG_LIMIT,
    "VRFY", vrfy_cmd, SMTPD_CMD_FLAG_LIMIT,
    "ETRN", etrn_cmd, SMTPD_CMD_FLAG_LIMIT,
    "QUIT", quit_cmd, 0,
    "XCLIENT", xclient_cmd, SMTPD_CMD_FLAG_LIMIT,
    "Received:", 0, SMTPD_CMD_FLAG_FORBIDDEN,
    "Reply-To:", 0, SMTPD_CMD_FLAG_FORBIDDEN,
    "Message-ID:", 0, SMTPD_CMD_FLAG_FORBIDDEN,
    "Subject:", 0, SMTPD_CMD_FLAG_FORBIDDEN,
    "From:", 0, SMTPD_CMD_FLAG_FORBIDDEN,
    "CONNECT", 0, SMTPD_CMD_FLAG_FORBIDDEN,
    "User-Agent:", 0, SMTPD_CMD_FLAG_FORBIDDEN,
    0,
};

static STRING_LIST *smtpd_noop_cmds;

/* smtpd_proto - talk the SMTP protocol */

static void smtpd_proto(SMTPD_STATE *state, const char *service)
{
    int     argc;
    SMTPD_TOKEN *argv;
    SMTPD_CMD *cmdp;
    int     count;
    int     crate;

    /*
     * Print a greeting banner and run the state machine. Read SMTP commands
     * one line at a time. According to the standard, a sender or recipient
     * address could contain an escaped newline. I think this is perverse,
     * and anyone depending on this is really asking for trouble.
     * 
     * In case of mail protocol trouble, the program jumps back to this place,
     * so that it can perform the necessary cleanup before talking to the
     * next client. The setjmp/longjmp primitives are like a sharp tool: use
     * with care. I would certainly recommend against the use of
     * setjmp/longjmp in programs that change privilege levels.
     * 
     * In case of file system trouble the program terminates after logging the
     * error and after informing the client. In all other cases (out of
     * memory, panic) the error is logged, and the msg_cleanup() exit handler
     * cleans up, but no attempt is made to inform the client of the nature
     * of the problem.
     */
    smtp_timeout_setup(state->client, var_smtpd_tmout);

    switch (vstream_setjmp(state->client)) {

    default:
	msg_panic("smtpd_proto: unknown error reading from %s[%s]",
		  state->name, state->addr);
	break;

    case SMTP_ERR_TIME:
	state->reason = "timeout";
	smtpd_chat_reply(state, "421 Error: timeout exceeded");
	break;

    case SMTP_ERR_EOF:
	state->reason = "lost connection";
	break;

    case 0:

	/*
	 * XXX The client connection count/rate control must be consistent in
	 * its use of client address information in connect and disconnect
	 * events. For now we exclude xclient authorized hosts from
	 * connection count/rate control.
	 */
	if (SMTPD_STAND_ALONE(state) == 0
	    && !xclient_allowed
	    && anvil_clnt
	    && !namadr_list_match(hogger_list, state->name, state->addr)
	    && anvil_clnt_connect(anvil_clnt, service, state->addr,
				  &count, &crate) == ANVIL_STAT_OK) {
	    if (var_smtpd_cconn_limit > 0 && count > var_smtpd_cconn_limit) {
		smtpd_chat_reply(state, "450 Too many connections from %s",
				 state->addr);
		msg_warn("Too many connections: %d from %s for service %s",
			 count, state->addr, service);
		break;
	    }
	    if (var_smtpd_crate_limit > 0 && crate > var_smtpd_crate_limit) {
		smtpd_chat_reply(state, "450 Too many connections from %s",
				 state->addr);
		msg_warn("Too frequent connections: %d from %s for service %s",
			 crate, state->addr, service);
		break;
	    }
	}
	/* XXX We use the real client for connect access control. */
	if (SMTPD_STAND_ALONE(state) == 0
	    && var_smtpd_delay_reject == 0
	    && (state->access_denied = smtpd_check_client(state)) != 0) {
	    smtpd_chat_reply(state, "%s", state->access_denied);
	} else {
	    smtpd_chat_reply(state, "220 %s", var_smtpd_banner);
	}

	for (;;) {
	    if (state->error_count >= var_smtpd_hard_erlim) {
		state->reason = "too many errors";
		state->error_mask |= MAIL_ERROR_PROTOCOL;
		smtpd_chat_reply(state, "421 Error: too many errors");
		break;
	    }
	    watchdog_pat();
	    smtpd_chat_query(state);
	    if ((argc = smtpd_token(vstring_str(state->buffer), &argv)) == 0) {
		state->error_mask |= MAIL_ERROR_PROTOCOL;
		smtpd_chat_reply(state, "500 Error: bad syntax");
		state->error_count++;
		continue;
	    }
	    if (*var_smtpd_noop_cmds
		&& string_list_match(smtpd_noop_cmds, argv[0].strval)) {
		smtpd_chat_reply(state, "250 Ok");
		if (state->junk_cmds++ > var_smtpd_junk_cmd_limit)
		    state->error_count++;
		continue;
	    }
	    for (cmdp = smtpd_cmd_table; cmdp->name != 0; cmdp++)
		if (strcasecmp(argv[0].strval, cmdp->name) == 0)
		    break;
	    if (cmdp->name == 0) {
		smtpd_chat_reply(state, "502 Error: command not implemented");
		state->error_mask |= MAIL_ERROR_PROTOCOL;
		state->error_count++;
		continue;
	    }
	    if (cmdp->flags & SMTPD_CMD_FLAG_FORBIDDEN) {
		msg_warn("%s sent non-SMTP command: %.100s",
			 state->namaddr, vstring_str(state->buffer));
		smtpd_chat_reply(state, "221 Error: I can break rules, too. Goodbye.");
		break;
	    }
	    /* XXX We use the real client for connect access control. */
	    if (state->access_denied && cmdp->action != quit_cmd) {
		smtpd_chat_reply(state, "503 Error: access denied for %s",
				 state->namaddr);	/* RFC 2821 Sec 3.1 */
		state->error_count++;
		continue;
	    }
	    state->where = cmdp->name;
	    if (cmdp->action(state, argc, argv) != 0)
		state->error_count++;
	    if ((cmdp->flags & SMTPD_CMD_FLAG_LIMIT)
		&& state->junk_cmds++ > var_smtpd_junk_cmd_limit)
		state->error_count++;
	    if (cmdp->action == quit_cmd)
		break;
	}
	break;
    }

    /*
     * XXX The client connection count/rate control must be consistent in its
     * use of client address information in connect and disconnect events.
     * For now we exclude xclient authorized hosts from connection count/rate
     * control.
     */
    if (SMTPD_STAND_ALONE(state) == 0
	&& !xclient_allowed
	&& anvil_clnt
	&& !namadr_list_match(hogger_list, state->name, state->addr))
	anvil_clnt_disconnect(anvil_clnt, service, state->addr);

    /*
     * Log abnormal session termination, in case postmaster notification has
     * been turned off. In the log, indicate the last recognized state before
     * things went wrong. Don't complain about clients that go away without
     * sending QUIT.
     */
    if (state->reason && state->where
	&& (strcmp(state->where, SMTPD_AFTER_DOT)
	    || strcmp(state->reason, "lost connection")))
	msg_info("%s after %s from %s[%s]",
		 state->reason, state->where, state->name, state->addr);

    /*
     * Cleanup whatever information the client gave us during the SMTP
     * dialog.
     */
    helo_reset(state);
#ifdef USE_SASL_AUTH
    if (var_smtpd_sasl_enable)
	smtpd_sasl_auth_reset(state);
#endif
    chat_reset(state, 0);
    mail_reset(state);
    rcpt_reset(state);
}

/* smtpd_service - service one client */

static void smtpd_service(VSTREAM *stream, char *service, char **argv)
{
    SMTPD_STATE state;

    /*
     * Sanity check. This service takes no command-line arguments.
     */
    if (argv[0])
	msg_fatal("unexpected command-line argument: %s", argv[0]);

    /*
     * This routine runs when a client has connected to our network port, or
     * when the smtp server is run in stand-alone mode (input from pipe).
     * 
     * Look up and sanitize the peer name, then initialize some connection-
     * specific state. When the name service is hosed, hostname lookup will
     * take a while. This is why I always run a local name server on critical
     * machines.
     */
    smtpd_state_init(&state, stream);
    msg_info("connect from %s[%s]", state.name, state.addr);

    /*
     * XCLIENT must not override its own access control.
     */
    xclient_allowed =
	namadr_list_match(xclient_hosts, state.name, state.addr);

    /*
     * See if we need to turn on verbose logging for this client.
     */
    debug_peer_check(state.name, state.addr);

    /*
     * Provide the SMTP service.
     */
    smtpd_proto(&state, service);

    /*
     * After the client has gone away, clean up whatever we have set up at
     * connection time.
     */
    msg_info("disconnect from %s[%s]", state.name, state.addr);
    smtpd_state_reset(&state);
    debug_peer_restore();
}

/* pre_accept - see if tables have changed */

static void pre_accept(char *unused_name, char **unused_argv)
{
    const char *table;

    if ((table = dict_changed_name()) != 0) {
	msg_info("table %s has changed -- restarting", table);
	exit(0);
    }
}

/* pre_jail_init - pre-jail initialization */

static void pre_jail_init(char *unused_name, char **unused_argv)
{

    /*
     * Initialize blacklist/etc. patterns before entering the chroot jail, in
     * case they specify a filename pattern.
     */
    smtpd_noop_cmds = string_list_init(MATCH_FLAG_NONE, var_smtpd_noop_cmds);
    verp_clients = namadr_list_init(MATCH_FLAG_NONE, var_verp_clients);
    xclient_hosts = namadr_list_init(MATCH_FLAG_NONE, var_xclient_hosts);
    hogger_list = namadr_list_init(MATCH_FLAG_NONE, var_smtpd_hoggers);
    if (getuid() == 0 || getuid() == var_owner_uid)
	smtpd_check_init();
    debug_peer_init();

    if (var_smtpd_sasl_enable)
#ifdef USE_SASL_AUTH
	smtpd_sasl_initialize();

    if (*var_smtpd_sasl_exceptions_networks)
	sasl_exceptions_networks =
	    namadr_list_init(MATCH_FLAG_NONE,
			     var_smtpd_sasl_exceptions_networks);
#else
	msg_warn("%s is true, but SASL support is not compiled in",
		 VAR_SMTPD_SASL_ENABLE);
#endif
}

/* post_jail_init - post-jail initialization */

static void post_jail_init(char *unused_name, char **unused_argv)
{

    /*
     * Initialize the receive transparency options: do we want unknown
     * recipient checks, address mapping, header_body_checks?.
     */
    smtpd_input_transp_mask =
    input_transp_mask(VAR_INPUT_TRANSP, var_input_transp);

    /*
     * Sanity checks. The queue_minfree value should be at least as large as
     * (process_limit * message_size_limit) but that is unpractical, so we
     * arbitrarily pick a number and require twice the message size limit.
     */
    if (var_queue_minfree > 0
	&& var_message_limit > 0
	&& var_queue_minfree / 2 < var_message_limit)
	msg_warn("%s(%lu) should be at least 2*%s(%lu)",
		 VAR_QUEUE_MINFREE, (unsigned long) var_queue_minfree,
		 VAR_MESSAGE_LIMIT, (unsigned long) var_message_limit);

    /*
     * Connection rate management.
     */
    if (var_smtpd_crate_limit || var_smtpd_cconn_limit)
	anvil_clnt = anvil_clnt_create();
}

/* main - the main program */

int     main(int argc, char **argv)
{
    static CONFIG_INT_TABLE int_table[] = {
	VAR_SMTPD_RCPT_LIMIT, DEF_SMTPD_RCPT_LIMIT, &var_smtpd_rcpt_limit, 1, 0,
	VAR_SMTPD_SOFT_ERLIM, DEF_SMTPD_SOFT_ERLIM, &var_smtpd_soft_erlim, 1, 0,
	VAR_SMTPD_HARD_ERLIM, DEF_SMTPD_HARD_ERLIM, &var_smtpd_hard_erlim, 1, 0,
	VAR_QUEUE_MINFREE, DEF_QUEUE_MINFREE, &var_queue_minfree, 0, 0,
	VAR_UNK_CLIENT_CODE, DEF_UNK_CLIENT_CODE, &var_unk_client_code, 0, 0,
	VAR_BAD_NAME_CODE, DEF_BAD_NAME_CODE, &var_bad_name_code, 0, 0,
	VAR_UNK_NAME_CODE, DEF_UNK_NAME_CODE, &var_unk_name_code, 0, 0,
	VAR_UNK_ADDR_CODE, DEF_UNK_ADDR_CODE, &var_unk_addr_code, 0, 0,
	VAR_RELAY_CODE, DEF_RELAY_CODE, &var_relay_code, 0, 0,
	VAR_MAPS_RBL_CODE, DEF_MAPS_RBL_CODE, &var_maps_rbl_code, 0, 0,
	VAR_ACCESS_MAP_CODE, DEF_ACCESS_MAP_CODE, &var_access_map_code, 0, 0,
	VAR_REJECT_CODE, DEF_REJECT_CODE, &var_reject_code, 0, 0,
	VAR_DEFER_CODE, DEF_DEFER_CODE, &var_defer_code, 0, 0,
	VAR_NON_FQDN_CODE, DEF_NON_FQDN_CODE, &var_non_fqdn_code, 0, 0,
	VAR_SMTPD_JUNK_CMD, DEF_SMTPD_JUNK_CMD, &var_smtpd_junk_cmd_limit, 1, 0,
	VAR_SMTPD_HIST_THRSH, DEF_SMTPD_HIST_THRSH, &var_smtpd_hist_thrsh, 1, 0,
	VAR_UNV_FROM_CODE, DEF_UNV_FROM_CODE, &var_unv_from_code, 0, 0,
	VAR_UNV_RCPT_CODE, DEF_UNV_RCPT_CODE, &var_unv_rcpt_code, 0, 0,
	VAR_MUL_RCPT_CODE, DEF_MUL_RCPT_CODE, &var_mul_rcpt_code, 0, 0,
	VAR_LOCAL_RCPT_CODE, DEF_LOCAL_RCPT_CODE, &var_local_rcpt_code, 0, 0,
	VAR_VIRT_ALIAS_CODE, DEF_VIRT_ALIAS_CODE, &var_virt_alias_code, 0, 0,
	VAR_VIRT_MAILBOX_CODE, DEF_VIRT_MAILBOX_CODE, &var_virt_mailbox_code, 0, 0,
	VAR_RELAY_RCPT_CODE, DEF_RELAY_RCPT_CODE, &var_relay_rcpt_code, 0, 0,
	VAR_VERIFY_POLL_COUNT, DEF_VERIFY_POLL_COUNT, &var_verify_poll_count, 1, 0,
	VAR_SMTPD_CRATE_LIMIT, DEF_SMTPD_CRATE_LIMIT, &var_smtpd_crate_limit, 0, 0,
	VAR_SMTPD_CCONN_LIMIT, DEF_SMTPD_CCONN_LIMIT, &var_smtpd_cconn_limit, 0, 0,
	0,
    };
    static CONFIG_TIME_TABLE time_table[] = {
	VAR_SMTPD_TMOUT, DEF_SMTPD_TMOUT, &var_smtpd_tmout, 1, 0,
	VAR_SMTPD_ERR_SLEEP, DEF_SMTPD_ERR_SLEEP, &var_smtpd_err_sleep, 0, 0,
	VAR_SMTPD_PROXY_TMOUT, DEF_SMTPD_PROXY_TMOUT, &var_smtpd_proxy_tmout, 1, 0,
	VAR_VERIFY_POLL_DELAY, DEF_VERIFY_POLL_DELAY, &var_verify_poll_delay, 1, 0,
	VAR_SMTPD_POLICY_TMOUT, DEF_SMTPD_POLICY_TMOUT, &var_smtpd_policy_tmout, 1, 0,
	VAR_SMTPD_POLICY_IDLE, DEF_SMTPD_POLICY_IDLE, &var_smtpd_policy_idle, 1, 0,
	VAR_SMTPD_POLICY_TTL, DEF_SMTPD_POLICY_TTL, &var_smtpd_policy_ttl, 1, 0,
	0,
    };
    static CONFIG_BOOL_TABLE bool_table[] = {
	VAR_HELO_REQUIRED, DEF_HELO_REQUIRED, &var_helo_required,
	VAR_SMTPD_DELAY_REJECT, DEF_SMTPD_DELAY_REJECT, &var_smtpd_delay_reject,
	VAR_STRICT_RFC821_ENV, DEF_STRICT_RFC821_ENV, &var_strict_rfc821_env,
	VAR_DISABLE_VRFY_CMD, DEF_DISABLE_VRFY_CMD, &var_disable_vrfy_cmd,
	VAR_ALLOW_UNTRUST_ROUTE, DEF_ALLOW_UNTRUST_ROUTE, &var_allow_untrust_route,
	VAR_SMTPD_SASL_ENABLE, DEF_SMTPD_SASL_ENABLE, &var_smtpd_sasl_enable,
	VAR_BROKEN_AUTH_CLNTS, DEF_BROKEN_AUTH_CLNTS, &var_broken_auth_clients,
	VAR_SHOW_UNK_RCPT_TABLE, DEF_SHOW_UNK_RCPT_TABLE, &var_show_unk_rcpt_table,
	0,
    };
    static CONFIG_STR_TABLE str_table[] = {
	VAR_SMTPD_BANNER, DEF_SMTPD_BANNER, &var_smtpd_banner, 1, 0,
	VAR_NOTIFY_CLASSES, DEF_NOTIFY_CLASSES, &var_notify_classes, 0, 0,
	VAR_CLIENT_CHECKS, DEF_CLIENT_CHECKS, &var_client_checks, 0, 0,
	VAR_HELO_CHECKS, DEF_HELO_CHECKS, &var_helo_checks, 0, 0,
	VAR_MAIL_CHECKS, DEF_MAIL_CHECKS, &var_mail_checks, 0, 0,
	VAR_RCPT_CHECKS, DEF_RCPT_CHECKS, &var_rcpt_checks, 0, 0,
	VAR_ETRN_CHECKS, DEF_ETRN_CHECKS, &var_etrn_checks, 0, 0,
	VAR_DATA_CHECKS, DEF_DATA_CHECKS, &var_data_checks, 0, 0,
	VAR_MAPS_RBL_DOMAINS, DEF_MAPS_RBL_DOMAINS, &var_maps_rbl_domains, 0, 0,
	VAR_RBL_REPLY_MAPS, DEF_RBL_REPLY_MAPS, &var_rbl_reply_maps, 0, 0,
	VAR_ERROR_RCPT, DEF_ERROR_RCPT, &var_error_rcpt, 1, 0,
	VAR_REST_CLASSES, DEF_REST_CLASSES, &var_rest_classes, 0, 0,
	VAR_CANONICAL_MAPS, DEF_CANONICAL_MAPS, &var_canonical_maps, 0, 0,
	VAR_RCPT_CANON_MAPS, DEF_RCPT_CANON_MAPS, &var_rcpt_canon_maps, 0, 0,
	VAR_VIRT_ALIAS_MAPS, DEF_VIRT_ALIAS_MAPS, &var_virt_alias_maps, 0, 0,
	VAR_VIRT_MAILBOX_MAPS, DEF_VIRT_MAILBOX_MAPS, &var_virt_mailbox_maps, 0, 0,
	VAR_ALIAS_MAPS, DEF_ALIAS_MAPS, &var_alias_maps, 0, 0,
	VAR_LOCAL_RCPT_MAPS, DEF_LOCAL_RCPT_MAPS, &var_local_rcpt_maps, 0, 0,
	VAR_SMTPD_SASL_OPTS, DEF_SMTPD_SASL_OPTS, &var_smtpd_sasl_opts, 0, 0,
	VAR_SMTPD_SASL_REALM, DEF_SMTPD_SASL_REALM, &var_smtpd_sasl_realm, 0, 0,
	VAR_SMTPD_SASL_EXCEPTIONS_NETWORKS, DEF_SMTPD_SASL_EXCEPTIONS_NETWORKS, &var_smtpd_sasl_exceptions_networks, 0, 0,
	VAR_FILTER_XPORT, DEF_FILTER_XPORT, &var_filter_xport, 0, 0,
	VAR_PERM_MX_NETWORKS, DEF_PERM_MX_NETWORKS, &var_perm_mx_networks, 0, 0,
	VAR_SMTPD_SND_AUTH_MAPS, DEF_SMTPD_SND_AUTH_MAPS, &var_smtpd_snd_auth_maps, 0, 0,
	VAR_SMTPD_NOOP_CMDS, DEF_SMTPD_NOOP_CMDS, &var_smtpd_noop_cmds, 0, 0,
	VAR_SMTPD_NULL_KEY, DEF_SMTPD_NULL_KEY, &var_smtpd_null_key, 0, 0,
	VAR_RELAY_RCPT_MAPS, DEF_RELAY_RCPT_MAPS, &var_relay_rcpt_maps, 0, 0,
	VAR_VERIFY_SENDER, DEF_VERIFY_SENDER, &var_verify_sender, 0, 0,
	VAR_VERP_CLIENTS, DEF_VERP_CLIENTS, &var_verp_clients, 0, 0,
	VAR_SMTPD_PROXY_FILT, DEF_SMTPD_PROXY_FILT, &var_smtpd_proxy_filt, 0, 0,
	VAR_SMTPD_PROXY_EHLO, DEF_SMTPD_PROXY_EHLO, &var_smtpd_proxy_ehlo, 0, 0,
	VAR_INPUT_TRANSP, DEF_INPUT_TRANSP, &var_input_transp, 0, 0,
	VAR_XCLIENT_HOSTS, DEF_XCLIENT_HOSTS, &var_xclient_hosts, 0, 0,
	VAR_SMTPD_HOGGERS, DEF_SMTPD_HOGGERS, &var_smtpd_hoggers, 0, 0,
	0,
    };
    static CONFIG_RAW_TABLE raw_table[] = {
	VAR_SMTPD_EXP_FILTER, DEF_SMTPD_EXP_FILTER, &var_smtpd_exp_filter, 1, 0,
	VAR_DEF_RBL_REPLY, DEF_DEF_RBL_REPLY, &var_def_rbl_reply, 1, 0,
	0,
    };

    /*
     * Pass control to the single-threaded service skeleton.
     */
    single_server_main(argc, argv, smtpd_service,
		       MAIL_SERVER_INT_TABLE, int_table,
		       MAIL_SERVER_STR_TABLE, str_table,
		       MAIL_SERVER_RAW_TABLE, raw_table,
		       MAIL_SERVER_BOOL_TABLE, bool_table,
		       MAIL_SERVER_TIME_TABLE, time_table,
		       MAIL_SERVER_PRE_INIT, pre_jail_init,
		       MAIL_SERVER_PRE_ACCEPT, pre_accept,
		       MAIL_SERVER_POST_INIT, post_jail_init,
		       0);
}
