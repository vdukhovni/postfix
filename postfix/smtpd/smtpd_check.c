/*++
/* NAME
/*	smtpd_check 3
/* SUMMARY
/*	SMTP client request filtering
/* SYNOPSIS
/*	#include "smtpd.h"
/*	#include "smtpd_check.h"
/*
/*	void	smtpd_check_init()
/*
/*	char	*smtpd_check_client(state)
/*	SMTPD_STATE *state;
/*
/*	char	*smtpd_check_helo(state, helohost)
/*	SMTPD_STATE *state;
/*	char	*helohost;
/*
/*	char	*smtpd_check_mail(state, sender)
/*	SMTPD_STATE *state;
/*	char	*sender;
/*
/*	char	*smtpd_check_rcpt(state, recipient)
/*	SMTPD_STATE *state;
/*	char	*recipient;
/*
/*	char	*smtpd_check_etrn(state, recipient)
/*	SMTPD_STATE *state;
/*	char	*recipient;
/* DESCRIPTION
/*	This module implements additional checks on SMTP client requests.
/*	A client request is validated in the context of the session state.
/*	The result is either an error response (including the numerical
/*	code) or the result is a null pointer in case of success.
/*
/*	smtpd_check_init() initializes. This function should be called
/*	once during the process life time.
/*
/*	Each of the following routines scrutinizes the argument passed to
/*	an SMTP command such as HELO, MAIL FROM, RCPT TO, or scrutinizes
/*	the initial client connection request.  The administrator can
/*	specify what restrictions apply.
/*
/*	Restrictions are specified via configuration parameters named
/*	\fIsmtpd_{client,helo,sender,recipient}_restrictions.\fR Each
/*	configuration parameter specifies a list of zero or more
/*	restrictions that are applied in the order as specified.
/*
/*	Restrictions that can appear in some or all restriction
/*	lists:
/* .IP reject
/* .IP permit
/*	Reject or permit the request unconditionally. This is to be used
/*	at the end of a restriction list in order to make the default
/*	action explicit.
/* .IP reject_unknown_client
/*	Reject the request when the client hostname could not be found.
/*	The \fIunknown_client_reject_code\fR configuration parameter
/*	specifies the reject status code (default: 450).
/* .IP permit_mynetworks
/*	Allow the request when the client address matches the \fImynetworks\fR
/*	configuration parameter.
/* .IP maptype:mapname
/*	Meaning depends on context: client name/address, helo name, sender
/*	or recipient address.
/*	Perform a lookup in the specified access table. Reject the request
/*	when the lookup result is REJECT or when the result begins with a
/*	4xx or 5xx status code. Other numerical status codes are not
/*	permitted. Allow the request otherwise. The
/*	\fIaccess_map_reject_code\fR configuration parameter specifies the
/*	reject status code (default: 554).
/* .IP "check_client_access maptype:mapname"
/*	Look up the client host name or any of its parent domains, or
/*	the client address or any network obtained by stripping octets
/*	from the address.
/* .IP "check_helo_access maptype:mapname"
/*	Look up the HELO/EHLO hostname or any of its parent domains.
/* .IP "check_sender_access maptype:mapname"
/*	Look up the resolved sender address, any parent domains of the
/*	resolved sender address domain, or the localpart@.
/* .IP "check_recipient_access maptype:mapname"
/*	Look up the resolved recipient address in the named access table,
/*	any parent domains of the recipient domain, and the localpart@.
/* .IP reject_maps_rbl
/*	Look up the client network address in the real-time blackhole
/*	DNS zones below the domains listed in the "maps_rbl_domains"
/*	configuration parameter. The \fImaps_rbl_reject_code\fR
/*	configuration parameter specifies the reject status code
/*	(default: 554).
/* .IP permit_naked_ip_address
/*	Permit the use of a naked IP address (without enclosing [])
/*	in HELO/EHLO commands.
/*	This violates the RFC. You must enable this for some popular
/*	PC mail clients.
/* .IP reject_non_fqdn_hostname
/* .IP reject_non_fqdn_sender
/* .IP reject_non_fqdn_recipient
/*	Require that the HELO, MAIL FROM or RCPT TO commands specify
/*	a fully-qualified domain name. The non_fqdn_reject_code parameter
/*	specifies the error code (default: 504).
/* .IP reject_invalid_hostname
/*	Reject the request when the HELO/EHLO hostname does not satisfy RFC
/*	requirements.  The underscore is considered a legal hostname character,
/*	and so is a trailing dot.
/*	The \fIinvalid_hostname_reject_code\fR configuration parameter
/*	specifies the reject status code (default:501).
/* .IP reject_unknown_hostname
/*	Reject the request when the HELO/EHLO hostname has no A or MX record.
/*	The \fIunknown_hostname_reject_code\fR configuration
/*	parameter specifies the reject status code (default: 450).
/* .IP reject_unknown_sender_domain
/*	Reject the request when the resolved sender address has no
/*	DNS A or MX record.
/*	The \fIunknown_address_reject_code\fR configuration parameter
/*	specifies the reject status code (default: 450).
/* .IP reject_unknown_recipient_domain
/*	Reject the request when the resolved recipient address has no
/*	DNS A or MX record.
/*	The \fIunknown_address_reject_code\fR configuration parameter
/*	specifies the reject status code (default: 450).
/* .IP check_relay_domains
/*	Allow the request when either the client hostname or the resolved
/*	recipient domain matches the \fIrelay_domains\fR configuration
/*	parameter.  Reject the request otherwise.
/*	The \fIrelay_domains_reject_code\fR configuration parameter specifies
/*	the reject status code (default: 554).
/* .IP reject_unauth_destination
/*	Reject the request when the resolved recipient domain does not match
/*	the \fIrelay_domains\fR configuration parameter.  Same error code as
/*	check_relay_domains.
/* .IP reject_unauth_pipelining
/*	Reject the request when the client has already sent the next request
/*	without being told that the server implements SMTP command pipelining.
/* .IP permit_mx_backup
/*	Allow the request when the local mail system is mail exchanger
/*	for the recipient domain (this includes the case where the local
/*	system is the final destination).
/* .PP
/*	smtpd_check_client() validates the client host name or address.
/*	Relevant configuration parameters:
/* .IP smtpd_client_restrictions
/*	Restrictions on the names or addresses of clients that may connect
/*	to this SMTP server.
/* .PP
/*	smtpd_check_helo() validates the hostname provided with the
/*	HELO/EHLO commands. Relevant configuration parameters:
/* .IP smtpd_helo_restrictions
/*	Restrictions on the hostname that is sent with the HELO/EHLO
/*	command.
/* .PP
/*	smtpd_check_mail() validates the sender address provided with
/*	a MAIL FROM request. Relevant configuration parameters:
/* .IP smtpd_sender_restrictions
/*	Restrictions on the sender address that is sent with the MAIL FROM
/*	command.
/* .PP
/*	smtpd_check_rcpt() validates the recipient address provided
/*	with an RCPT TO request. Relevant configuration parameters:
/* .IP smtpd_recipient_restrictions
/*	Restrictions on the recipient address that is sent with the RCPT
/*	TO command.
/* .PP
/*	smtpd_check_etrn() validates the domain name provided with the
/*	ETRN command, and other client-provided information. Relevant
/*	configuration parameters:
/* .IP smtpd_etrn_restrictions
/*	Restrictions on the hostname that is sent with the HELO/EHLO
/*	command.
/* .PP
/*	smtpd_check_size() checks if a message with the given size can
/*	be received (zero means that the message size is unknown).  The
/*	message is rejected when:
/* .IP \(bu
/*	The message size exceeds the non-zero bound specified with the
/*	\fImessage_size_limit\fR configuration parameter. This is a
/*	permanent error.
/* .IP \(bu
/*	The message would cause the available queue file system space
/*	to drop below the bound specified with the \fImin_queue_free\fR
/*	configuration parameter. This is a temporary error.
/* .IP \(bu
/*	The message would use up more than half the available queue file
/*	system space. This is a temporary error.
/* .PP
/*	Arguments:
/* .IP name
/*	The client hostname, or \fIunknown\fR.
/* .IP addr
/*	The client address.
/* .IP helohost
/*	The hostname given with the HELO command.
/* .IP sender
/*	The sender address given with the MAIL FROM command.
/* .IP recipient
/*	The recipient address given with the RCPT TO or VRFY command.
/* .IP size
/*	The message size given with the MAIL FROM command (zero if unknown).
/* BUGS
/*	Policies like these should not be hard-coded in C, but should
/*	be user-programmable instead.
/* SEE ALSO
/*	namadr_list(3) host access control
/*	domain_list(3) domain access control
/*	fsspace(3) free file system space
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <netdb.h>
#include <setjmp.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

/* Utility library. */

#include <msg.h>
#include <vstring.h>
#include <split_at.h>
#include <fsspace.h>
#include <stringops.h>
#include <valid_hostname.h>
#include <argv.h>
#include <mymalloc.h>
#include <dict.h>

/* DNS library. */

#include <dns.h>

/* Global library. */

#include <namadr_list.h>
#include <domain_list.h>
#include <mail_params.h>
#include <canon_addr.h>
#include <resolve_clnt.h>
#include <mail_error.h>
#include <resolve_local.h>
#include <own_inet_addr.h>

/* Application-specific. */

#include "smtpd.h"
#include "smtpd_check.h"

 /*
  * Eject seat in case of parsing problems.
  */
static jmp_buf smtpd_check_buf;

 /*
  * Results of restrictions.
  */
#define SMTPD_CHECK_DUNNO	0	/* indifferent */
#define SMTPD_CHECK_OK		1	/* explicitly permit */
#define SMTPD_CHECK_REJECT	2	/* explicitly reject */

 /*
  * Intermediate results. These are static to avoid unnecessary stress on the
  * memory manager routines.
  */
static RESOLVE_REPLY reply;
static VSTRING *query;
static VSTRING *error_text;

 /*
  * Pre-opened access control lists.
  */
static DOMAIN_LIST *relay_domains;
static NAMADR_LIST *mynetworks;

 /*
  * Pre-parsed restriction lists.
  */
static ARGV *client_restrctions;
static ARGV *helo_restrctions;
static ARGV *mail_restrctions;
static ARGV *rcpt_restrctions;
static ARGV *etrn_restrctions;

 /*
  * Reject context.
  */
#define SMTPD_NAME_CLIENT	"Client host"
#define SMTPD_NAME_HELO		"Helo command"
#define SMTPD_NAME_SENDER	"Sender address"
#define SMTPD_NAME_RECIPIENT	"Recipient address"
#define SMTPD_NAME_ETRN		"Etrn command"

 /*
  * YASLM.
  */
#define STR	vstring_str

/* smtpd_check_parse - pre-parse restrictions */

static ARGV *smtpd_check_parse(char *checks)
{
    char   *saved_checks = mystrdup(checks);
    ARGV   *argv = argv_alloc(1);
    char   *bp = saved_checks;
    char   *name;

    /*
     * Pre-parse the restriction list, and open any dictionaries that we
     * encounter. Dictionaries must be opened before entering the chroot
     * jail.
     */
    while ((name = mystrtok(&bp, " \t\r\n,")) != 0) {
	argv_add(argv, name, (char *) 0);
	if (strchr(name, ':') && dict_handle(name) == 0)
	    dict_register(name, dict_open(name, O_RDONLY, DICT_FLAG_LOCK));
    }
    argv_terminate(argv);

    /*
     * Cleanup.
     */
    myfree(saved_checks);
    return (argv);
}

/* smtpd_check_init - initialize once during process lifetime */

void    smtpd_check_init(void)
{

    /*
     * Pre-open access control lists before going to jail.
     */
    mynetworks = namadr_list_init(var_mynetworks);
    relay_domains = domain_list_init(var_relay_domains);

    /*
     * Reply is used as a cache for resolved addresses, and error_text is
     * used for returning error responses.
     */
    resolve_clnt_init(&reply);
    query = vstring_alloc(10);
    error_text = vstring_alloc(10);

    /*
     * Pre-parse the restriction lists. At the same time, pre-open tables
     * before going to jail.
     */
    client_restrctions = smtpd_check_parse(var_client_checks);
    helo_restrctions = smtpd_check_parse(var_helo_checks);
    mail_restrctions = smtpd_check_parse(var_mail_checks);
    rcpt_restrctions = smtpd_check_parse(var_rcpt_checks);
    etrn_restrctions = smtpd_check_parse(var_etrn_checks);
}

/* smtpd_check_reject - do the boring things that must be done */

static int smtpd_check_reject(SMTPD_STATE *state, int error_class,
			              char *format,...)
{
    va_list ap;

    /*
     * Update the error class mask, and format the response. XXX What about
     * multi-line responses? For now we cheat and send whitespace.
     */
    state->error_mask |= error_class;
    va_start(ap, format);
    vstring_vsprintf(error_text, format, ap);
    va_end(ap);

    /*
     * Validate the response, that is, the response must begin with a
     * three-digit status code, and the first digit must be 4 or 5. If the
     * response is bad, log a warning and send a generic response instead.
     */
    if ((STR(error_text)[0] != '4' && STR(error_text)[0] != '5')
	|| !ISDIGIT(STR(error_text)[1]) || !ISDIGIT(STR(error_text)[2])
	|| ISDIGIT(STR(error_text)[3])) {
	msg_warn("response code configuration error: %s", STR(error_text));
	vstring_strcpy(error_text, "450 Service unavailable");
    }
    printable(STR(error_text), ' ');

    /*
     * Log what is happening. When the sysadmin discards policy violation
     * postmaster notices, this may be the only trace left that service was
     * rejected. Print the request, client name/address, and response.
     */
    if (state->recipient && state->sender) {
	msg_info("reject: %s from %s: %s; from=<%s> to=<%s>",
		 state->where, state->namaddr, STR(error_text),
		 state->sender, state->recipient);
    } else if (state->recipient) {
	msg_info("reject: %s from %s: %s; to=<%s>",
		 state->where, state->namaddr, STR(error_text),
		 state->recipient);
    } else if (state->sender) {
	msg_info("reject: %s from %s: %s; from=<%s>",
		 state->where, state->namaddr, STR(error_text),
		 state->sender);
    } else {
	msg_info("reject: %s from %s: %s",
		 state->where, state->namaddr, STR(error_text));
    }
    return (SMTPD_CHECK_REJECT);
}

/* reject_unknown_client - fail if client hostname is unknown */

static int reject_unknown_client(SMTPD_STATE *state)
{
    char   *myname = "reject_unknown_client";

    if (msg_verbose)
	msg_info("%s: %s %s", myname, state->name, state->addr);

    if (strcasecmp(state->name, "unknown") == 0)
	return (smtpd_check_reject(state, MAIL_ERROR_POLICY,
		 "%d Client host rejected: cannot find your hostname, [%s]",
				   var_unk_client_code, state->addr));
    return (SMTPD_CHECK_DUNNO);
}

/* permit_mynetworks - succeed if client is in a trusted network */

static int permit_mynetworks(SMTPD_STATE *state)
{
    char   *myname = "permit_mynetworks";

    if (msg_verbose)
	msg_info("%s: %s %s", myname, state->name, state->addr);

    if (namadr_list_match(mynetworks, state->name, state->addr))
	return (SMTPD_CHECK_OK);
    return (SMTPD_CHECK_DUNNO);
}

/* dup_if_truncate - save hostname and truncate if it ends in dot */

static char *dup_if_truncate(char *name)
{
    int     len;
    char   *result;

    /*
     * Truncate hostnames ending in dot but not dot-dot.
     */
    if ((len = strlen(name)) > 1
	&& name[len - 1] == '.'
	&& name[len - 2] != '.') {
	result = mystrndup(name, len - 1);
    } else
	result = name;
    return (result);
}

/* reject_invalid_hostaddr - fail if host address is incorrect */

static int reject_invalid_hostaddr(SMTPD_STATE *state, char *addr,
				        char *reply_name, char *reply_class)
{
    char   *myname = "reject_invalid_hostaddr";
    int     len;
    char   *test_addr;
    int     stat;

    if (msg_verbose)
	msg_info("%s: %s", myname, addr);

    if (addr[0] == '[' && (len = strlen(addr)) > 2 && addr[len - 1] == ']') {
	test_addr = mystrndup(&addr[1], len - 2);
    } else
	test_addr = addr;

    /*
     * Validate the address.
     */
    if (!valid_hostaddr(test_addr))
	stat = smtpd_check_reject(state, MAIL_ERROR_POLICY,
				  "%d <%s>: %s rejected: invalid ip address",
				var_bad_name_code, reply_name, reply_class);
    else
	stat = SMTPD_CHECK_DUNNO;

    /*
     * Cleanup.
     */
    if (test_addr != addr)
	myfree(test_addr);

    return (stat);
}

/* reject_invalid_hostname - fail if host/domain syntax is incorrect */

static int reject_invalid_hostname(SMTPD_STATE *state, char *name,
				        char *reply_name, char *reply_class)
{
    char   *myname = "reject_invalid_hostname";
    char   *test_name;
    int     stat;

    if (msg_verbose)
	msg_info("%s: %s", myname, name);

    /*
     * Truncate hostnames ending in dot but not dot-dot.
     */
    test_name = dup_if_truncate(name);

    /*
     * Validate the hostname.
     */
    if (!valid_hostname(test_name))
	stat = smtpd_check_reject(state, MAIL_ERROR_POLICY,
				  "%d <%s>: %s rejected: Invalid name",
				var_bad_name_code, reply_name, reply_class);
    else
	stat = SMTPD_CHECK_DUNNO;

    /*
     * Cleanup.
     */
    if (test_name != name)
	myfree(test_name);

    return (stat);
}

/* reject_non_fqdn_hostname - fail if host name is not in fqdn form */

static int reject_non_fqdn_hostname(SMTPD_STATE *state, char *name,
				        char *reply_name, char *reply_class)
{
    char   *myname = "reject_non_fqdn_hostname";
    char   *test_name;
    int     stat;

    if (msg_verbose)
	msg_info("%s: %s", myname, name);

    /*
     * Truncate hostnames ending in dot but not dot-dot.
     */
    test_name = dup_if_truncate(name);

    /*
     * Validate the hostname.
     */
    if (!valid_hostname(test_name) || !strchr(test_name, '.'))
	stat = smtpd_check_reject(state, MAIL_ERROR_POLICY,
		      "%d <%s>: %s rejected: need fully-qualified hostname",
				var_non_fqdn_code, reply_name, reply_class);
    else
	stat = SMTPD_CHECK_DUNNO;

    /*
     * Cleanup.
     */
    if (test_name != name)
	myfree(test_name);

    return (stat);
}

/* reject_unknown_hostname - fail if name has no A or MX record */

static int reject_unknown_hostname(SMTPD_STATE *state, char *name,
				        char *reply_name, char *reply_class)
{
    char   *myname = "reject_unknown_hostname";
    int     dns_status;

    if (msg_verbose)
	msg_info("%s: %s", myname, name);

    dns_status = dns_lookup_types(name, 0, (DNS_RR **) 0, (VSTRING *) 0,
				  (VSTRING *) 0, T_A, T_MX, 0);
    if (dns_status != DNS_OK)
	return (smtpd_check_reject(state, MAIL_ERROR_POLICY,
				   "%d <%s>: %s rejected: Host not found",
				   dns_status == DNS_NOTFOUND ?
				   var_unk_name_code : 450,
				   reply_name, reply_class));
    return (SMTPD_CHECK_DUNNO);
}

/* reject_unknown_mailhost - fail if name has no A or MX record */

static int reject_unknown_mailhost(SMTPD_STATE *state, char *name,
				        char *reply_name, char *reply_class)
{
    char   *myname = "reject_unknown_mailhost";
    int     dns_status;

    if (msg_verbose)
	msg_info("%s: %s", myname, name);

    dns_status = dns_lookup_types(name, 0, (DNS_RR **) 0, (VSTRING *) 0,
				  (VSTRING *) 0, T_A, T_MX, 0);
    if (dns_status != DNS_OK)
	return (smtpd_check_reject(state, MAIL_ERROR_POLICY,
				   "%d <%s>: %s rejected: Domain not found",
				   dns_status == DNS_NOTFOUND ?
				   var_unk_addr_code : 450,
				   reply_name, reply_class));
    return (SMTPD_CHECK_DUNNO);
}

/* check_relay_domains - OK/FAIL for message relaying */

static int check_relay_domains(SMTPD_STATE *state, char *recipient,
			               char *reply_name, char *reply_class)
{
    char   *myname = "check_relay_domains";
    char   *domain;

    if (msg_verbose)
	msg_info("%s: %s", myname, recipient);

    /*
     * Permit if the client matches the relay_domains list.
     */
    if (domain_list_match(relay_domains, state->name))
	return (SMTPD_CHECK_OK);

    /*
     * Resolve the address.
     */
    canon_addr_internal(query, recipient);
    resolve_clnt_query(STR(query), &reply);

    /*
     * Permit if destination is local. XXX This must be generalized for
     * per-domain user tables and for non-UNIX local delivery agents.
     */
    if (STR(reply.nexthop)[0] == 0
	|| (domain = strrchr(STR(reply.recipient), '@')) == 0)
	return (SMTPD_CHECK_OK);
    domain += 1;

    /*
     * Permit if the destination matches the relay_domains list.
     */
    if (domain_list_match(relay_domains, domain))
	return (SMTPD_CHECK_OK);

    /*
     * Deny relaying between sites that both are not in relay_domains.
     */
    return (smtpd_check_reject(state, MAIL_ERROR_POLICY,
			       "%d <%s>: %s rejected: Relay access denied",
			       var_relay_code, reply_name, reply_class));
}

/* reject_unauth_destination - FAIL for message relaying */

static int reject_unauth_destination(SMTPD_STATE *state, char *recipient)
{
    char   *myname = "reject_unauth_destination";
    char   *domain;

    if (msg_verbose)
	msg_info("%s: %s", myname, recipient);

    /*
     * Resolve the address.
     */
    canon_addr_internal(query, recipient);
    resolve_clnt_query(STR(query), &reply);

    /*
     * Pass if destination is local. XXX This must be generalized for
     * per-domain user tables and for non-UNIX local delivery agents.
     */
    if (STR(reply.nexthop)[0] == 0
	|| (domain = strrchr(STR(reply.recipient), '@')) == 0)
	return (SMTPD_CHECK_DUNNO);
    domain += 1;

    /*
     * Pass if the destination matches the relay_domains list.
     */
    if (domain_list_match(relay_domains, domain))
	return (SMTPD_CHECK_DUNNO);

    /*
     * Reject relaying to sites that are not listed in relay_domains.
     */
    return (smtpd_check_reject(state, MAIL_ERROR_POLICY,
			       "%d <%s>: Relay access denied",
			       var_relay_code, recipient));
}

/* reject_unauth_pipelining - reject improper use of SMTP command pipelining */

static int reject_unauth_pipelining(SMTPD_STATE *state)
{
    char   *myname = "reject_unauth_pipelining";

    if (msg_verbose)
	msg_info("%s: %s", myname, state->where);

    if (state->client != 0
	&& SMTPD_STAND_ALONE(state) == 0
	&& vstream_peek(state->client) > 0
	&& strcasecmp(state->protocol, "ESMTP") != 0) {
	return (smtpd_check_reject(state, MAIL_ERROR_PROTOCOL,
			    "503 Improper use of SMTP command pipelining"));
    }
    return (SMTPD_CHECK_DUNNO);
}

/* has_my_addr - see if this host name lists one of my network addresses */

static int has_my_addr(char *host)
{
    char   *myname = "has_my_addr";
    struct in_addr addr;
    char  **cpp;
    struct hostent *hp;

    if (msg_verbose)
	msg_info("%s: host %s", myname, host);

    /*
     * If we can't lookup the host, play safe and assume it is OK.
     */
#define YUP	1
#define NOPE	0

    if ((hp = gethostbyname(host)) == 0) {
	if (msg_verbose)
	    msg_info("%s: host %s: not found", myname, host);
	return (YUP);
    }
    if (hp->h_addrtype != AF_INET || hp->h_length != sizeof(addr)) {
	msg_warn("address type %d length %d for %s",
		 hp->h_addrtype, hp->h_length, host);
	return (YUP);
    }
    for (cpp = hp->h_addr_list; *cpp; cpp++) {
	memcpy((char *) &addr, *cpp, sizeof(addr));
	if (msg_verbose)
	    msg_info("%s: addr %s", myname, inet_ntoa(addr));
	if (own_inet_addr(&addr))
	    return (YUP);
    }
    if (msg_verbose)
	msg_info("%s: host %s: no match", myname, host);

    return (NOPE);
}

/* permit_mx_backup - permit use of me as MX backup for recipient domain */

static int permit_mx_backup(SMTPD_STATE *unused_state, const char *recipient)
{
    char   *myname = "permit_mx_backup";
    char   *domain;
    DNS_RR *mx_list;
    DNS_RR *mx;
    int     dns_status;

    if (msg_verbose)
	msg_info("%s: %s", myname, recipient);

    /*
     * Resolve the address.
     */
    canon_addr_internal(query, recipient);
    resolve_clnt_query(STR(query), &reply);

    /*
     * If the destination is local, it is acceptable, because we are
     * supposedly MX for our own address.
     */
    if (STR(reply.nexthop)[0] == 0
	|| (domain = strrchr(STR(reply.recipient), '@')) == 0)
	return (SMTPD_CHECK_OK);
    domain += 1;
    if (resolve_local(domain))
	return (SMTPD_CHECK_OK);

    if (msg_verbose)
	msg_info("%s: not local: %s", myname, recipient);

    /*
     * Skip numerical forms that didn't match the local system.
     */
    if (domain[0] == '#'
	|| (domain[0] == '[' && domain[strlen(domain) - 1] == ']'))
	return (SMTPD_CHECK_DUNNO);

    /*
     * Look up the list of MX host names for this domain. If no MX host is
     * found, perhaps it is a CNAME for the local machine. Clients aren't
     * supposed to send CNAMEs in SMTP commands, but it happens anyway. If we
     * can't look up the destination, play safe and assume it is OK.
     */
    dns_status = dns_lookup(domain, T_MX, 0, &mx_list,
			    (VSTRING *) 0, (VSTRING *) 0);
    if (dns_status == DNS_NOTFOUND)
	return (has_my_addr(domain) ? SMTPD_CHECK_OK : SMTPD_CHECK_DUNNO);
    if (dns_status != DNS_OK)
	return (SMTPD_CHECK_OK);

    /*
     * First, see if we match any of the MX host names listed. Only if no
     * name match is found, go through the trouble of host address lookups.
     */
    for (mx = mx_list; mx != 0; mx = mx->next) {
	if (msg_verbose)
	    msg_info("%s: resolve hostname: %s", myname, (char *) mx->data);
	if (resolve_local((char *) mx->data)) {
	    dns_rr_free(mx_list);
	    return (SMTPD_CHECK_OK);
	}
    }

    /*
     * Argh. Do further DNS lookups and match interface addresses.
     */
    for (mx = mx_list; mx != 0; mx = mx->next) {
	if (msg_verbose)
	    msg_info("%s: address lookup: %s", myname, (char *) mx->data);
	if (has_my_addr((char *) mx->data)) {
	    dns_rr_free(mx_list);
	    return (SMTPD_CHECK_OK);
	}
    }
    if (msg_verbose)
	msg_info("%s: no match", myname);

    dns_rr_free(mx_list);
    return (SMTPD_CHECK_DUNNO);
}

/* reject_non_fqdn_address - fail if address is not in fqdn form */

static int reject_non_fqdn_address(SMTPD_STATE *state, char *addr,
				        char *reply_name, char *reply_class)
{
    char   *myname = "reject_non_fqdn_address";
    char   *domain;
    char   *test_dom;
    int     stat;

    if (msg_verbose)
	msg_info("%s: %s", myname, addr);

    /*
     * Locate the domain information.
     */
    if ((domain = strrchr(addr, '@')) != 0)
	domain++;
    else
	domain = "";

    /*
     * Skip forms that we can't handle yet.
     */
    if (domain[0] == '#')
	return (SMTPD_CHECK_DUNNO);
    if (domain[0] == '[' && domain[strlen(domain) - 1] == ']')
	return (SMTPD_CHECK_DUNNO);

    /*
     * Truncate names ending in dot but not dot-dot.
     */
    test_dom = dup_if_truncate(domain);

    /*
     * Validate the domain.
     */
    if (!*test_dom || !valid_hostname(test_dom) || !strchr(test_dom, '.'))
	stat = smtpd_check_reject(state, MAIL_ERROR_POLICY,
		       "%d <%s>: %s rejected: need fully-qualified address",
				var_non_fqdn_code, reply_name, reply_class);
    else
	stat = SMTPD_CHECK_DUNNO;

    /*
     * Cleanup.
     */
    if (test_dom != domain)
	myfree(test_dom);

    return (stat);
}

/* reject_unknown_address - fail if address does not resolve */

static int reject_unknown_address(SMTPD_STATE *state, char *addr,
				        char *reply_name, char *reply_class)
{
    char   *myname = "reject_unknown_address";
    char   *domain;

    if (msg_verbose)
	msg_info("%s: %s", myname, addr);

    /*
     * Resolve the address.
     */
    canon_addr_internal(query, addr);
    resolve_clnt_query(STR(query), &reply);

    /*
     * Skip local destinations and non-DNS forms.
     */
    if (STR(reply.nexthop)[0] == 0
	|| (domain = strrchr(STR(reply.recipient), '@')) == 0)
	return (SMTPD_CHECK_DUNNO);
    domain += 1;
    if (domain[0] == '#')
	return (SMTPD_CHECK_DUNNO);
    if (domain[0] == '[' && domain[strlen(domain) - 1] == ']')
	return (SMTPD_CHECK_DUNNO);

    /*
     * Look up the name in the DNS.
     */
    return (reject_unknown_mailhost(state, domain, reply_name, reply_class));
}

/* check_table_result - translate table lookup result into pass/reject */

static int check_table_result(SMTPD_STATE *state, char *table,
			              const char *value, const char *datum,
			              char *reply_name, char *reply_class)
{
    char   *myname = "check_table_result";
    int     code;

    if (msg_verbose)
	msg_info("%s: %s %s %s", myname, table, value, datum);

    /*
     * DUNNO means skip this table.
     */
    if (strcasecmp(value, "DUNNO") == 0)
	return (SMTPD_CHECK_DUNNO);

    /*
     * REJECT means NO. Generate a generic error response.
     */
    if (strcasecmp(value, "REJECT") == 0)
	return (smtpd_check_reject(state, MAIL_ERROR_POLICY,
				   "%d <%s>: %s rejected: Access denied",
			     var_access_map_code, reply_name, reply_class));

    /*
     * 4xx or 5xx means NO as well. smtpd_check_reject() will validate the
     * response status code.
     */
    if (ISDIGIT(value[0]) && ISDIGIT(value[1]) && ISDIGIT(value[2])) {
	code = atoi(value);
	while (ISDIGIT(*value) || ISSPACE(*value))
	    value++;
	return (smtpd_check_reject(state, MAIL_ERROR_POLICY,
				   "%d <%s>: %s rejected: %s",
				   code, reply_name, reply_class, value));
    }

    /*
     * OK or RELAY or whatever means YES.
     */
    return (SMTPD_CHECK_OK);
}

/* check_access - table lookup without substring magic */

static int check_access(SMTPD_STATE *state, char *table, char *name, int flags,
			        char *reply_name, char *reply_class)
{
    char   *myname = "check_access";
    char   *low_name = lowercase(mystrdup(name));
    const char *value;
    DICT   *dict;

#define CHK_ACCESS_RETURN(x) { myfree(low_name); return(x); }
#define FULL	0
#define PARTIAL	DICT_FLAG_FIXED

    if (msg_verbose)
	msg_info("%s: %s", myname, name);

    if ((dict = dict_handle(table)) == 0)
	msg_panic("%s: dictionary not found: %s", myname, table);
    if (flags == 0 || (flags & dict->flags) != 0) {
	if ((value = dict_get(dict, low_name)) != 0)
	    CHK_ACCESS_RETURN(check_table_result(state, table, value, name,
						 reply_name, reply_class));
	if (dict_errno != 0)
	    msg_fatal("%s: table lookup problem", table);
    }
    CHK_ACCESS_RETURN(SMTPD_CHECK_DUNNO);
}

/* check_domain_access - domainname-based table lookup */

static int check_domain_access(SMTPD_STATE *state, char *table,
			               char *domain, int flags,
			               char *reply_name, char *reply_class)
{
    char   *myname = "check_domain_access";
    char   *low_domain = lowercase(mystrdup(domain));
    char   *name;
    char   *next;
    const char *value;
    DICT   *dict;

    if (msg_verbose)
	msg_info("%s: %s", myname, domain);

    /*
     * Try the name and its parent domains. Don't try top-level domains.
     */
#define CHK_DOMAIN_RETURN(x) { myfree(low_domain); return(x); }

    for (name = low_domain; (next = strchr(name, '.')) != 0; name = next + 1) {
	if ((dict = dict_handle(table)) == 0)
	    msg_panic("%s: dictionary not found: %s", myname, table);
	if (flags == 0 || (flags & dict->flags) != 0) {
	    if ((value = dict_get(dict, name)) != 0)
		CHK_DOMAIN_RETURN(check_table_result(state, table, value,
					  domain, reply_name, reply_class));
	    if (dict_errno != 0)
		msg_fatal("%s: table lookup problem", table);
	}
	flags = PARTIAL;
    }
    CHK_DOMAIN_RETURN(SMTPD_CHECK_DUNNO);
}

/* check_addr_access - address-based table lookup */

static int check_addr_access(SMTPD_STATE *state, char *table,
			             char *address, int flags,
			             char *reply_name, char *reply_class)
{
    char   *myname = "check_addr_access";
    char   *addr;
    const char *value;
    DICT   *dict;

    if (msg_verbose)
	msg_info("%s: %s", myname, address);

    /*
     * Try the address and its parent networks.
     */
    addr = STR(vstring_strcpy(error_text, address));

    do {
	if ((dict = dict_handle(table)) == 0)
	    msg_panic("%s: dictionary not found: %s", myname, table);
	if (flags == 0 || (flags & dict->flags) != 0) {
	    if ((value = dict_get(dict, addr)) != 0)
		return (check_table_result(state, table, value, address,
					   reply_name, reply_class));
	    if (dict_errno != 0)
		msg_fatal("%s: table lookup problem", table);
	}
	flags = PARTIAL;
    } while (split_at_right(addr, '.'));

    return (SMTPD_CHECK_DUNNO);
}

/* check_namadr_access - OK/FAIL based on host name/address lookup */

static int check_namadr_access(SMTPD_STATE *state, char *table,
			               char *name, char *addr, int flags,
			               char *reply_name, char *reply_class)
{
    char   *myname = "check_namadr_access";
    int     status;

    if (msg_verbose)
	msg_info("%s: name %s addr %s", myname, name, addr);

    /*
     * Look up the host name, or parent domains thereof. XXX A domain
     * wildcard may pre-empt a more specific address table entry.
     */
    if ((status = check_domain_access(state, table, name, flags,
				      reply_name, reply_class)) != 0)
	return (status);

    /*
     * Look up the network address, or parent networks thereof.
     */
    if ((status = check_addr_access(state, table, addr, flags,
				    reply_name, reply_class)) != 0)
	return (status);

    /*
     * Undecided when the host was not found.
     */
    return (SMTPD_CHECK_DUNNO);
}

/* check_mail_access - OK/FAIL based on mail address lookup */

static int check_mail_access(SMTPD_STATE *state, char *table, char *addr,
			             char *reply_name, char *reply_class)
{
    char   *myname = "check_mail_access";
    char   *ratsign;
    int     status;
    char   *local_at;

    if (msg_verbose)
	msg_info("%s: %s", myname, addr);

    /*
     * Resolve the address.
     */
    canon_addr_internal(query, addr);
    resolve_clnt_query(STR(query), &reply);

    /*
     * Garbage in, garbage out. Every address from canon_addr_internal() and
     * from resolve_clnt_query() must be fully qualified.
     */
    if ((ratsign = strrchr(STR(reply.recipient), '@')) == 0) {
	msg_warn("%s: no @domain in address: %s", myname, STR(reply.recipient));
	return (0);
    }

    /*
     * Look up the full address.
     */
    if ((status = check_access(state, table, STR(reply.recipient), FULL,
			       reply_name, reply_class)) != 0)
	return (status);

    /*
     * Look up the domain name, or parent domains thereof.
     */
    if ((status = check_domain_access(state, table, ratsign + 1, PARTIAL,
				      reply_name, reply_class)) != 0)
	return (status);

    /*
     * Look up localpart@
     */
    local_at = mystrndup(STR(reply.recipient),
			 ratsign - STR(reply.recipient) + 1);
    status = check_access(state, table, local_at, PARTIAL,
			  reply_name, reply_class);
    myfree(local_at);
    if (status != 0)
	return (status);

    /*
     * Undecided when no match found.
     */
    return (SMTPD_CHECK_DUNNO);
}

/* reject_maps_rbl - reject if client address in real-time blackhole list */

static int reject_maps_rbl(SMTPD_STATE *state)
{
    char   *myname = "reject_maps_rbl";
    ARGV   *octets = argv_split(state->addr, ".");
    VSTRING *query = vstring_alloc(100);
    char   *saved_domains = mystrdup(var_maps_rbl_domains);
    char   *bp = saved_domains;
    char   *rbl_domain;
    int     reverse_len;
    int     dns_status = DNS_FAIL;
    int     i;
    int     result;

    if (msg_verbose)
	msg_info("%s: %s", myname, state->addr);

    /*
     * Build the constant part of the RBL query: the reverse client address.
     */
    for (i = octets->argc - 1; i >= 0; i--) {
	vstring_strcat(query, octets->argv[i]);
	vstring_strcat(query, ".");
    }
    reverse_len = VSTRING_LEN(query);

    /*
     * Tack on each RBL domain name and query the DNS for an A record. If the
     * record exists, the client address is blacklisted.
     */
    while ((rbl_domain = mystrtok(&bp, " \t\r\n,")) != 0) {
	vstring_truncate(query, reverse_len);
	vstring_strcat(query, rbl_domain);
	dns_status = dns_lookup(STR(query), T_A, 0, (DNS_RR **) 0,
				(VSTRING *) 0, (VSTRING *) 0);
	if (dns_status == DNS_OK)
	    break;
    }

    /*
     * Report the result.
     */
    if (dns_status == DNS_OK)
	result = smtpd_check_reject(state, MAIL_ERROR_POLICY,
			    "%d Service unavailable; [%s] blocked using %s",
				var_maps_rbl_code, state->addr, rbl_domain);
    else
	result = SMTPD_CHECK_DUNNO;

    /*
     * Clean up.
     */
    argv_free(octets);
    vstring_free(query);
    myfree(saved_domains);

    return (result);
}

/* is_map_command - restriction has form: check_xxx_access type:name */

static int is_map_command(char *name, char *command, char ***argp)
{

    /*
     * This is a three-valued function: (a) this is not a check_xxx_access
     * command, (b) this is a malformed check_xxx_access command, (c) this is
     * a well-formed check_xxx_access command. That's too clumsy for function
     * result values, so we use regular returns for (a) and (c), and use long
     * jumps for the error case (b).
     */
    if (strcasecmp(name, command) != 0) {
	return (0);
    } else if (*(*argp + 1) == 0 || strchr(*(*argp += 1), ':') == 0) {
	msg_warn("restriction %s requires maptype:mapname", command);
	longjmp(smtpd_check_buf, -1);
    } else {
	return (1);
    }
}

/* generic_checks - generic restrictions */

static int generic_checks(SMTPD_STATE *state, char *name,
			          char ***cpp, int *status,
			          char *reply_name, char *reply_class)
{

    /*
     * Generic restrictions.
     */
    if (strcasecmp(name, PERMIT_ALL) == 0) {
	*status = SMTPD_CHECK_OK;
	if ((*cpp)[1] != 0)
	    msg_warn("restriction `%s' after `%s' is ignored",
		     (*cpp)[1], PERMIT_ALL);
	return (1);
    }
    if (strcasecmp(name, REJECT_ALL) == 0) {
	*status = smtpd_check_reject(state, MAIL_ERROR_POLICY,
				     "%d <%s>: %s rejected: Access denied",
				  var_reject_code, reply_name, reply_class);
	if ((*cpp)[1] != 0)
	    msg_warn("restriction `%s' after `%s' is ignored",
		     (*cpp)[1], REJECT_ALL);
	return (1);
    }
    if (strcasecmp(name, REJECT_UNAUTH_PIPE) == 0) {
	*status = reject_unauth_pipelining(state);
	return (1);
    }

    /*
     * Client name/address restrictions.
     */
    if (strcasecmp(name, REJECT_UNKNOWN_CLIENT) == 0) {
	*status = reject_unknown_client(state);
	return (1);
    }
    if (strcasecmp(name, PERMIT_MYNETWORKS) == 0) {
	*status = permit_mynetworks(state);
	return (1);
    }
    if (is_map_command(name, CHECK_CLIENT_ACL, cpp)) {
	*status = check_namadr_access(state, **cpp, state->name, state->addr,
				   FULL, state->namaddr, SMTPD_NAME_CLIENT);
	return (1);
    }
    if (strcasecmp(name, REJECT_MAPS_RBL) == 0) {
	*status = reject_maps_rbl(state);
	return (1);
    }

    /*
     * HELO/EHLO parameter restrictions.
     */
    if (is_map_command(name, CHECK_HELO_ACL, cpp) && state->helo_name) {
	if (state->helo_name)
	    *status = check_domain_access(state, **cpp, state->helo_name, FULL,
					  state->helo_name, SMTPD_NAME_HELO);
	return (1);
    }
    if (strcasecmp(name, REJECT_INVALID_HOSTNAME) == 0) {
	if (state->helo_name) {
	    if (*state->helo_name != '[')
		*status = reject_invalid_hostname(state, state->helo_name,
					 state->helo_name, SMTPD_NAME_HELO);
	    else
		*status = reject_invalid_hostaddr(state, state->helo_name,
					 state->helo_name, SMTPD_NAME_HELO);
	}
	return (1);
    }
    if (strcasecmp(name, REJECT_UNKNOWN_HOSTNAME) == 0) {
	if (state->helo_name) {
	    if (*state->helo_name != '[')
		*status = reject_unknown_hostname(state, state->helo_name,
					 state->helo_name, SMTPD_NAME_HELO);
	    else
		*status = reject_invalid_hostaddr(state, state->helo_name,
					 state->helo_name, SMTPD_NAME_HELO);
	}
	return (1);
    }
    if (strcasecmp(name, PERMIT_NAKED_IP_ADDR) == 0) {
	if (state->helo_name) {
	    if (state->helo_name[strspn(state->helo_name, "0123456789.")] == 0
	      && (*status = reject_invalid_hostaddr(state, state->helo_name,
				   state->helo_name, SMTPD_NAME_HELO)) == 0)
		*status = SMTPD_CHECK_OK;
	}
	return (1);
    }
    if (strcasecmp(name, REJECT_NON_FQDN_HOSTNAME) == 0) {
	if (state->helo_name) {
	    if (*state->helo_name != '[')
		*status = reject_non_fqdn_hostname(state, state->helo_name,
					 state->helo_name, SMTPD_NAME_HELO);
	    else
		*status = reject_invalid_hostaddr(state, state->helo_name,
					 state->helo_name, SMTPD_NAME_HELO);
	}
	return (1);
    }

    /*
     * Sender mail address restrictions.
     */
    if (is_map_command(name, CHECK_SENDER_ACL, cpp) && state->sender) {
	if (state->sender)
	    *status = check_mail_access(state, **cpp, state->sender,
					state->sender, SMTPD_NAME_SENDER);
	return (1);
    }
    if (strcasecmp(name, REJECT_UNKNOWN_ADDRESS) == 0) {
	if (state->sender)
	    *status = reject_unknown_address(state, state->sender,
					  state->sender, SMTPD_NAME_SENDER);
	return (1);
    }
    if (strcasecmp(name, REJECT_UNKNOWN_SENDDOM) == 0) {
	if (state->sender)
	    *status = reject_unknown_address(state, state->sender,
					  state->sender, SMTPD_NAME_SENDER);
	return (1);
    }
    if (strcasecmp(name, REJECT_NON_FQDN_SENDER) == 0) {
	if (*state->sender)
	    *status = reject_non_fqdn_address(state, state->sender,
					  state->sender, SMTPD_NAME_SENDER);
	return (1);
    }
    return (0);
}

/* smtpd_check_client - validate client name or address */

char   *smtpd_check_client(SMTPD_STATE *state)
{
    char  **cpp;
    char   *name;
    int     status;

    /*
     * Initialize.
     */
    if (state->name == 0 && state->addr == 0)
	return (0);
    status = setjmp(smtpd_check_buf);
    if (status != 0)
	return (0);

    /*
     * Apply restrictions in the order as specified.
     */
    for (cpp = client_restrctions->argv; (name = *cpp) != 0; cpp++) {
	if (strchr(name, ':') != 0) {
	    status = check_namadr_access(state, name, state->name, state->addr,
				   FULL, state->namaddr, SMTPD_NAME_CLIENT);
	} else if (generic_checks(state, name, &cpp, &status,
				  state->namaddr, SMTPD_NAME_CLIENT) == 0) {
	    msg_warn("unknown %s check: \"%s\"", VAR_CLIENT_CHECKS, name);
	    break;
	}
	if (status != 0)
	    break;
    }
    return (status == SMTPD_CHECK_REJECT ? STR(error_text) : 0);
}

/* smtpd_check_helo - validate HELO hostname */

char   *smtpd_check_helo(SMTPD_STATE *state, char *helohost)
{
    char  **cpp;
    char   *name;
    int     status;
    char   *saved_helo = state->helo_name;

    /*
     * Initialize.
     */
    if (helohost == 0)
	return (0);
    status = setjmp(smtpd_check_buf);
    if (status != 0)
	return (0);

    /*
     * Apply restrictions in the order as specified. Minor kluge so that we
     * can delegate work to the generic routine.
     */
    state->helo_name = mystrdup(helohost);
    for (cpp = helo_restrctions->argv; (name = *cpp) != 0; cpp++) {
	if (strchr(name, ':') != 0) {
	    status = check_domain_access(state, name, helohost, FULL,
					 helohost, SMTPD_NAME_HELO);
	} else if (generic_checks(state, name, &cpp, &status,
				  helohost, SMTPD_NAME_HELO) == 0) {
	    msg_warn("unknown %s check: \"%s\"", VAR_HELO_CHECKS, name);
	    break;
	}
	if (status != 0)
	    break;
    }
    myfree(state->helo_name);
    state->helo_name = saved_helo;
    return (status == SMTPD_CHECK_REJECT ? STR(error_text) : 0);
}

/* smtpd_check_mail - validate sender address */

char   *smtpd_check_mail(SMTPD_STATE *state, char *sender)
{
    char  **cpp;
    char   *name;
    int     status;
    char   *saved_sender = state->sender;

    /*
     * Initialize.
     */
    if (sender == 0)
	return (0);
    status = setjmp(smtpd_check_buf);
    if (status != 0)
	return (0);

    /*
     * Apply restrictions in the order as specified. Minor kluge so that we
     * can delegate work to the generic routine.
     */
    state->sender = mystrdup(sender);
    for (cpp = mail_restrctions->argv; (name = *cpp) != 0; cpp++) {
	if (strchr(name, ':') != 0) {
	    status = check_mail_access(state, name, sender,
				       sender, SMTPD_NAME_SENDER);
	} else if (generic_checks(state, name, &cpp, &status,
				  sender, SMTPD_NAME_SENDER) == 0) {
	    msg_warn("unknown %s check: \"%s\"", VAR_MAIL_CHECKS, name);
	    return (0);
	}
	if (status != 0)
	    break;
    }
    myfree(state->sender);
    state->sender = saved_sender;
    return (status == SMTPD_CHECK_REJECT ? STR(error_text) : 0);
}

/* smtpd_check_rcpt - validate recipient address */

char   *smtpd_check_rcpt(SMTPD_STATE *state, char *recipient)
{
    char  **cpp;
    char   *name;
    int     status;
    char   *saved_recipient = state->recipient;
    char   *err;

    /*
     * Initialize.
     */
    if (recipient == 0)
	return (0);

    /*
     * Minor kluge so that we can delegate work to the generic routine and so
     * that we can syslog the recipient with the reject messages.
     */
    state->recipient = mystrdup(recipient);

#define SMTPD_CHECK_RCPT_RETURN(x) { \
	myfree(state->recipient); \
	state->recipient = saved_recipient; \
	return (x); \
    }

    /*
     * Apply delayed restrictions.
     */
    if (var_smtpd_delay_reject)
	if ((err = smtpd_check_client(state)) != 0
	    || (err = smtpd_check_helo(state, state->helo_name)) != 0
	    || (err = smtpd_check_mail(state, state->sender)) != 0)
	    SMTPD_CHECK_RCPT_RETURN(err);

    /*
     * More initialization.
     */
    status = setjmp(smtpd_check_buf);
    if (status != 0)
	SMTPD_CHECK_RCPT_RETURN(0);

    /*
     * Apply restrictions in the order as specified.
     */
    for (cpp = rcpt_restrctions->argv; (name = *cpp) != 0; cpp++) {
	if (strchr(name, ':') != 0) {
	    status = check_mail_access(state, name, recipient,
				       recipient, SMTPD_NAME_RECIPIENT);
	} else if (is_map_command(name, CHECK_RECIP_ACL, &cpp)) {
	    status = check_mail_access(state, *cpp, recipient,
				       recipient, SMTPD_NAME_RECIPIENT);
	} else if (strcasecmp(name, PERMIT_MX_BACKUP) == 0) {
	    status = permit_mx_backup(state, recipient);
	} else if (strcasecmp(name, REJECT_UNAUTH_DEST) == 0) {
	    status = reject_unauth_destination(state, recipient);
	} else if (strcasecmp(name, CHECK_RELAY_DOMAINS) == 0) {
	    status = check_relay_domains(state, recipient,
					 recipient, SMTPD_NAME_RECIPIENT);
	    if (cpp[1] != 0)
		msg_warn("restriction `%s' after `%s' is ignored",
			 cpp[1], CHECK_RELAY_DOMAINS);
	} else if (strcasecmp(name, REJECT_UNKNOWN_RCPTDOM) == 0) {
	    status = reject_unknown_address(state, recipient,
					    recipient, SMTPD_NAME_RECIPIENT);
	} else if (strcasecmp(name, REJECT_NON_FQDN_RCPT) == 0) {
	    status = reject_non_fqdn_address(state, recipient,
					   recipient, SMTPD_NAME_RECIPIENT);
	} else if (generic_checks(state, name, &cpp, &status,
				  recipient, SMTPD_NAME_RECIPIENT) == 0) {
	    msg_warn("unknown %s check: \"%s\"", VAR_RCPT_CHECKS, name);
	    break;
	}
	if (status != 0)
	    break;
    }
    SMTPD_CHECK_RCPT_RETURN(status == SMTPD_CHECK_REJECT ? STR(error_text) : 0);
}

/* smtpd_check_etrn - validate ETRN request */

char   *smtpd_check_etrn(SMTPD_STATE *state, char *domain)
{
    char  **cpp;
    char   *name;
    int     status;
    char   *err;

    /*
     * Apply delayed restrictions.
     */
    if (var_smtpd_delay_reject)
	if ((err = smtpd_check_client(state)) != 0
	    || (err = smtpd_check_helo(state, state->helo_name)) != 0)
	    return (err);

    /*
     * Initialize.
     */
    if (domain == 0)
	return (0);
    status = setjmp(smtpd_check_buf);
    if (status != 0)
	return (0);

    /*
     * Apply restrictions in the order as specified.
     */
    for (cpp = etrn_restrctions->argv; (name = *cpp) != 0; cpp++) {
	if (strchr(name, ':') != 0) {
	    status = check_domain_access(state, name, domain, FULL,
					 domain, SMTPD_NAME_ETRN);
	} else if (is_map_command(name, CHECK_ETRN_ACL, &cpp)) {
	    status = check_domain_access(state, *cpp, domain, FULL,
					 domain, SMTPD_NAME_ETRN);
	} else if (generic_checks(state, name, &cpp, &status,
				  domain, SMTPD_NAME_ETRN) == 0) {
	    msg_warn("unknown %s check: \"%s\"", VAR_RCPT_CHECKS, name);
	    break;
	}
	if (status != 0)
	    break;
    }
    return (status == SMTPD_CHECK_REJECT ? STR(error_text) : 0);
}

/* smtpd_check_size - check optional SIZE parameter value */

char   *smtpd_check_size(SMTPD_STATE *state, off_t size)
{
    char   *myname = "smtpd_check_size";
    struct fsspace fsbuf;

    /*
     * Avoid overflow/underflow when comparing message size against available
     * space.
     */
#define BLOCKS(x)	((x) / fsbuf.block_size)

    if (var_message_limit > 0 && size > var_message_limit) {
	(void) smtpd_check_reject(state, MAIL_ERROR_POLICY,
				  "552 Message size exceeds fixed limit");
	return (STR(error_text));
    }
    fsspace(".", &fsbuf);
    if (msg_verbose)
	msg_info("%s: blocks %lu avail %lu min_free %lu size %lu",
		 myname,
		 (unsigned long) fsbuf.block_size,
		 (unsigned long) fsbuf.block_free,
		 (unsigned long) var_queue_minfree,
		 (unsigned long) size);
    if (BLOCKS(var_queue_minfree) >= fsbuf.block_free
	|| BLOCKS(size) >= fsbuf.block_free - BLOCKS(var_queue_minfree)
	|| BLOCKS(size) >= fsbuf.block_free / 2) {
	(void) smtpd_check_reject(state, MAIL_ERROR_RESOURCE,
				  "452 Insufficient system storage");
	return (STR(error_text));
    }
    return (0);
}

#ifdef TEST

 /*
  * Test program to try out all these restrictions without having to go live.
  * This is not entirely stand-alone, as it requires access to the Postfix
  * rewrite/resolve service. This is just for testing code, not for debugging
  * configuration files.
  */
#include <stdlib.h>

#include <msg_vstream.h>
#include <vstring_vstream.h>

#include <mail_conf.h>

#include <smtpd_chat.h>

 /*
  * Dummies. These are never set.
  */
char   *var_client_checks = "";
char   *var_helo_checks = "";
char   *var_mail_checks = "";
char   *var_rcpt_checks = "";
char   *var_etrn_checks = "";
char   *var_relay_domains = "";
char   *var_mynetworks = "";
char   *var_notify_classes = "";

 /*
  * String-valued configuration parameters.
  */
char   *var_maps_rbl_domains;
char   *var_mydest;
char   *var_inet_interfaces;

typedef struct {
    char   *name;
    char   *defval;
    char  **target;
} STRING_TABLE;

static STRING_TABLE string_table[] = {
    VAR_MAPS_RBL_DOMAINS, DEF_MAPS_RBL_DOMAINS, &var_maps_rbl_domains,
    VAR_MYDEST, DEF_MYDEST, &var_mydest,
    VAR_INET_INTERFACES, DEF_INET_INTERFACES, &var_inet_interfaces,
    0,
};

/* string_init - initialize string parameters */

static void string_init(void)
{
    STRING_TABLE *sp;

    for (sp = string_table; sp->name; sp++)
	sp->target[0] = mystrdup(sp->defval);
}

/* string_update - update string parameter */

static int string_update(char **argv)
{
    STRING_TABLE *sp;

    for (sp = string_table; sp->name; sp++) {
	if (strcasecmp(argv[0], sp->name) == 0) {
	    myfree(sp->target[0]);
	    sp->target[0] = mystrdup(argv[1]);
	    return (1);
	}
    }
    return (0);
}

 /*
  * Integer parameters.
  */
int     var_queue_minfree;		/* XXX use off_t */
typedef struct {
    char   *name;
    int     defval;
    int    *target;
} INT_TABLE;

int     var_unk_client_code;
int     var_bad_name_code;
int     var_unk_name_code;
int     var_unk_addr_code;
int     var_relay_code;
int     var_maps_rbl_code;
int     var_access_map_code;
int     var_reject_code;
int     var_non_fqdn_code;
int     var_smtpd_delay_reject;

static INT_TABLE int_table[] = {
    "msg_verbose", 0, &msg_verbose,
    VAR_UNK_CLIENT_CODE, DEF_UNK_CLIENT_CODE, &var_unk_client_code,
    VAR_BAD_NAME_CODE, DEF_BAD_NAME_CODE, &var_bad_name_code,
    VAR_UNK_NAME_CODE, DEF_UNK_NAME_CODE, &var_unk_name_code,
    VAR_UNK_ADDR_CODE, DEF_UNK_ADDR_CODE, &var_unk_addr_code,
    VAR_RELAY_CODE, DEF_RELAY_CODE, &var_relay_code,
    VAR_MAPS_RBL_CODE, DEF_MAPS_RBL_CODE, &var_maps_rbl_code,
    VAR_ACCESS_MAP_CODE, DEF_ACCESS_MAP_CODE, &var_access_map_code,
    VAR_REJECT_CODE, DEF_REJECT_CODE, &var_reject_code,
    VAR_NON_FQDN_CODE, DEF_NON_FQDN_CODE, &var_non_fqdn_code,
    VAR_SMTPD_DELAY_REJECT, DEF_SMTPD_DELAY_REJECT, &var_smtpd_delay_reject,
    0,
};

/* int_init - initialize int parameters */

static void int_init(void)
{
    INT_TABLE *sp;

    for (sp = int_table; sp->name; sp++)
	sp->target[0] = sp->defval;
}

/* int_update - update int parameter */

static int int_update(char **argv)
{
    INT_TABLE *ip;

    for (ip = int_table; ip->name; ip++) {
	if (strcasecmp(argv[0], ip->name) == 0) {
	    if (!ISDIGIT(*argv[1]))
		msg_fatal("bad number: %s %s", ip->name, argv[1]);
	    ip->target[0] = atoi(argv[1]);
	    return (1);
	}
    }
    return (0);
}

 /*
  * Restrictions.
  */
typedef struct {
    char   *name;
    ARGV  **target;
}       REST_TABLE;

static REST_TABLE rest_table[] = {
    "client_restrictions", &client_restrctions,
    "helo_restrictions", &helo_restrctions,
    "sender_restrictions", &mail_restrctions,
    "recipient_restrictions", &rcpt_restrctions,
    "etrn_restrictions", &etrn_restrctions,
    0,
};

/* rest_update - update restriction */

static int rest_update(char **argv)
{
    REST_TABLE *rp;

    for (rp = rest_table; rp->name; rp++) {
	if (strcasecmp(rp->name, argv[0]) == 0) {
	    argv_free(rp->target[0]);
	    rp->target[0] = smtpd_check_parse(argv[1]);
	    return (1);
	}
    }
    return (0);
}

/* resolve_clnt_init - initialize reply */

void    resolve_clnt_init(RESOLVE_REPLY *reply)
{
    reply->transport = vstring_alloc(100);
    reply->nexthop = vstring_alloc(100);
    reply->recipient = vstring_alloc(100);
}

/* canon_addr_internal - stub */

VSTRING *canon_addr_internal(VSTRING *result, const char *addr)
{
    if (addr == STR(result))
	msg_panic("canon_addr_internal: result clobbers input");
    if (strchr(addr, '@') == 0)
	msg_fatal("%s: address rewriting is disabled", addr);
    vstring_strcpy(result, addr);
}

/* resolve_clnt_query - stub */

void    resolve_clnt_query(const char *addr, RESOLVE_REPLY *reply)
{
    if (addr == STR(reply->recipient))
	msg_panic("resolve_clnt_query: result clobbers input");
    vstring_strcpy(reply->transport, "foo");
    vstring_strcpy(reply->nexthop, "foo");
    if (strchr(addr, '%'))
	msg_fatal("%s: address rewriting is disabled", addr);
    vstring_strcpy(reply->recipient, addr);
}

/* smtpd_chat_reset - stub */

void    smtpd_chat_reset(SMTPD_STATE *unused_state)
{
}

/* usage - scream and terminate */

static NORETURN usage(char *myname)
{
    msg_fatal("usage: %s", myname);
}

main(int argc, char **argv)
{
    VSTRING *buf = vstring_alloc(100);
    SMTPD_STATE state;
    ARGV   *args;
    char   *bp;
    char   *resp;

    /*
     * Initialization. Use dummies for client information.
     */
    msg_vstream_init(argv[0], VSTREAM_ERR);
    if (argc != 1)
	usage(argv[0]);
    string_init();
    int_init();
    smtpd_check_init();
    smtpd_state_init(&state, VSTREAM_IN, "", "");
    state.queue_id = "<queue id>";

    /*
     * Main loop: update config parameters or test the client, helo, sender
     * and recipient restrictions.
     */
    while (vstring_fgets_nonl(buf, VSTREAM_IN) != 0) {

	/*
	 * Tokenize the command. Note, the comma is not a separator, so that
	 * restriction lists can be entered as comma-separated lists.
	 */
	bp = STR(buf);
	if (!isatty(0)) {
	    vstream_printf(">>> %s\n", bp);
	    vstream_fflush(VSTREAM_OUT);
	}
	if (*bp == '#')
	    continue;
	if (*bp == '!') {
	    vstream_printf("exit %d\n", system(bp + 1));
	    continue;
	}
	args = argv_split(bp, " \t\r\n");

	/*
	 * Recognize the command.
	 */
	resp = "bad command";
	switch (args->argc) {

	    /*
	     * Special case: client identity.
	     */
	case 3:
#define UPDATE_STRING(ptr,val) { if (ptr) myfree(ptr); ptr = mystrdup(val); }

	    if (strcasecmp(args->argv[0], "client") == 0) {
		state.where = "CONNECT";
		UPDATE_STRING(state.name, args->argv[1]);
		UPDATE_STRING(state.addr, args->argv[2]);
		if (state.namaddr)
		    myfree(state.namaddr);
		state.namaddr = concatenate(state.name, "[", state.addr,
					    "]", (char *) 0);
		resp = smtpd_check_client(&state);
	    }
	    break;

	    /*
	     * Try config settings.
	     */
	case 2:
	    if (strcasecmp(args->argv[0], "mynetworks") == 0) {
		namadr_list_free(mynetworks);
		mynetworks = namadr_list_init(args->argv[1]);
		resp = 0;
		break;
	    }
	    if (strcasecmp(args->argv[0], "relay_domains") == 0) {
		domain_list_free(relay_domains);
		relay_domains = domain_list_init(args->argv[1]);
		resp = 0;
		break;
	    }
	    if (int_update(args->argv)
		|| string_update(args->argv)
		|| rest_update(args->argv)) {
		resp = 0;
		break;
	    }

	    /*
	     * Try restrictions.
	     */
	    if (strcasecmp(args->argv[0], "helo") == 0) {
		state.where = "HELO";
		resp = smtpd_check_helo(&state, args->argv[1]);
		UPDATE_STRING(state.helo_name, args->argv[1]);
	    } else if (strcasecmp(args->argv[0], "mail") == 0) {
		state.where = "MAIL";
		resp = smtpd_check_mail(&state, args->argv[1]);
		UPDATE_STRING(state.sender, args->argv[1]);
	    } else if (strcasecmp(args->argv[0], "rcpt") == 0) {
		state.where = "RCPT";
		resp = smtpd_check_rcpt(&state, args->argv[1]);
	    }
	    break;

	    /*
	     * Show commands.
	     */
	default:
	    resp = "Commands...\n\
		client <name> <address>\n\
		helo <hostname>\n\
		sender <address>\n\
		recipient <address>\n\
		msg_verbose <level>\n\
		client_restrictions <restrictions>\n\
		helo_restrictions <restrictions>\n\
		sender_restrictions <restrictions>\n\
		recipient_restrictions <restrictions>\n\
		\n\
		Note: no address rewriting \n";
	    break;
	}
	vstream_printf("%s\n", resp ? resp : "OK");
	vstream_fflush(VSTREAM_OUT);
	argv_free(args);
    }
    vstring_free(buf);
    smtpd_state_reset(&state);
#define FREE_STRING(s) { if (s) myfree(s); }
    FREE_STRING(state.helo_name);
    FREE_STRING(state.sender);
    exit(0);
}

#endif
