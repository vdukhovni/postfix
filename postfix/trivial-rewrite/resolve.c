/*++
/* NAME
/*	resolve 3
/* SUMMARY
/*	mail address resolver
/* SYNOPSIS
/*	#include "trivial-rewrite.h"
/*
/*	void	resolve_init(void)
/*
/*	void	resolve_proto(stream)
/*	VSTREAM	*stream;
/*
/*	void	resolve_addr(rule, addr, result)
/*	char	*rule;
/*	char	*addr;
/*	VSTRING *result;
/* DESCRIPTION
/*	This module implements the trivial address resolving engine.
/*	It distinguishes between local and remote mail, and optionally
/*	consults one or more transport tables that map a destination
/*	to a transport, nexthop pair.
/*
/*	resolve_init() initializes data structures that are private
/*	to this module. It should be called once before using the
/*	actual resolver routines.
/*
/*	resolve_proto() implements the client-server protocol:
/*	read one address in FQDN form, reply with a (transport,
/*      nexthop, internalized recipient) triple.
/*
/*	resolve_addr() gives direct access to the address resolving
/*	engine. It resolves an internalized address to a (transport,
/*	nexthop, internalized recipient) triple.
/* STANDARDS
/* DIAGNOSTICS
/*	Problems and transactions are logged to the syslog daemon.
/* BUGS
/* SEE ALSO
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
#include <stdlib.h>
#include <string.h>

/* Utility library. */

#include <msg.h>
#include <vstring.h>
#include <vstream.h>
#include <vstring_vstream.h>
#include <split_at.h>

/* Global library. */

#include <mail_params.h>
#include <mail_proto.h>
#include <mail_addr.h>
#include <rewrite_clnt.h>
#include <resolve_local.h>
#include <mail_conf.h>
#include <quote_822_local.h>
#include <tok822.h>

/* Application-specific. */

#include "trivial-rewrite.h"
#include "transport.h"

#define STR	vstring_str

/* resolve_addr - resolve address according to rule set */

void    resolve_addr(char *addr, VSTRING *channel, VSTRING *nexthop,
		             VSTRING *nextrcpt)
{
    VSTRING *addr_buf = vstring_alloc(100);
    TOK822 *tree;
    TOK822 *saved_domain = 0;
    TOK822 *domain = 0;

    /*
     * The address is in internalized (unquoted) form, so we must externalize
     * it first before we can parse it.
     */
    quote_822_local(addr_buf, addr);
    tree = tok822_scan_addr(vstring_str(addr_buf));

    /*
     * Preliminary resolver: strip off all instances of the local domain.
     * Terminate when no destination domain is left over, or when the
     * destination domain is remote.
     */
#define RESOLVE_LOCAL(domain) \
    resolve_local(STR(tok822_internalize(addr_buf, domain, TOK822_STR_DEFL)))

    while (tree->head) {

	/*
	 * Strip trailing dot.
	 */
	if (tree->tail->type == '.')
	    tok822_free_tree(tok822_sub_keep_before(tree, tree->tail));

	/*
	 * A lone empty string becomes the postmaster.
	 */
	if (tree->head == tree->tail && tree->head->type == TOK822_QSTRING
	    && VSTRING_LEN(tree->head->vstr) == 0) {
	    tok822_free(tree->head);
	    tree->head = tok822_scan(MAIL_ADDR_POSTMASTER, &tree->tail);
	}

	/*
	 * Strip (and save) @domain if local.
	 */
	if ((domain = tok822_rfind_type(tree->tail, '@')) != 0) {
	    if (RESOLVE_LOCAL(domain->next) == 0)
		break;
	    tok822_sub_keep_before(tree, domain);
	    if (saved_domain)
		tok822_free_tree(saved_domain);
	    saved_domain = domain;
	}

	/*
	 * Replace foo%bar by foo@bar, site!user by user@site, rewrite to
	 * canonical form, and retry.
	 */
	if (var_swap_bangpath && tok822_rfind_type(tree->tail, '!') != 0) {
	    rewrite_tree(REWRITE_CANON, tree);
	} else if (var_percent_hack
		   && (domain = tok822_rfind_type(tree->tail, '%')) != 0) {
	    domain->type = '@';
	    rewrite_tree(REWRITE_CANON, tree);
	} else {
	    domain = 0;
	    break;
	}
    }

    /*
     * Non-local delivery: if no transport is specified, assume the transport
     * specified in var_def_transport. If no mail relay is specified in
     * var_relayhost, forward to the domain's mail exchanger.
     */
    if (domain != 0) {
	if (*var_transport_maps == 0
	    || (tok822_internalize(addr_buf, domain->next, TOK822_STR_DEFL),
		transport_lookup(STR(addr_buf), channel, nexthop) == 0)) {
	    vstring_strcpy(channel, var_def_transport);
	    if (*var_relayhost)
		vstring_strcpy(nexthop, var_relayhost);
	    else
		tok822_internalize(nexthop, domain->next, TOK822_STR_DEFL);
	}
	tok822_internalize(nextrcpt, tree, TOK822_STR_DEFL);
    }

    /*
     * Local delivery: if no domain was specified, assume the local machine.
     * See above for what happens with an empty localpart.
     */
    else {
	vstring_strcpy(channel, MAIL_SERVICE_LOCAL);
	vstring_strcpy(nexthop, "");
	if (saved_domain) {
	    tok822_sub_append(tree, saved_domain);
	    saved_domain = 0;
	} else {
	    tok822_sub_append(tree, tok822_alloc('@', (char *) 0));
	    tok822_sub_append(tree, tok822_scan(var_myhostname, (TOK822 **) 0));
	}
	tok822_internalize(nextrcpt, tree, TOK822_STR_DEFL);
    }

    /*
     * Clean up.
     */
    if (saved_domain)
	tok822_free_tree(saved_domain);
    tok822_free_tree(tree);
    vstring_free(addr_buf);
}

/* Static, so they can be used by the network protocol interface only. */

static VSTRING *channel;
static VSTRING *nexthop;
static VSTRING *nextrcpt;
static VSTRING *query;

/* resolve_proto - read request and send reply */

int     resolve_proto(VSTREAM *stream)
{
    if (mail_scan(stream, "%s", query) != 1)
	return (-1);

    resolve_addr(STR(query), channel, nexthop, nextrcpt);

    if (msg_verbose)
	msg_info("%s -> (`%s' `%s' `%s')", STR(query), STR(channel),
		 STR(nexthop), STR(nextrcpt));

    mail_print(stream, "%s %s %s", STR(channel), STR(nexthop), STR(nextrcpt));

    if (vstream_fflush(stream) != 0) {
	msg_warn("write resolver reply: %m");
	return (-1);
    }
    return (0);
}

/* resolve_init - module initializations */

void    resolve_init(void)
{
    query = vstring_alloc(100);
    channel = vstring_alloc(100);
    nexthop = vstring_alloc(100);
    nextrcpt = vstring_alloc(100);
}
