/*++
/* NAME
/*	smtp_tls_sess 3
/* SUMMARY
/*	SMTP_TLS_SESS structure management
/* SYNOPSIS
/*	#include "smtp.h"
/*
/*	void    smtp_tls_list_init()
/*
/*	SMTP_TLS_SESS *smtp_tls_sess_alloc(why, dest, host, port, valid)
/*	DSN_BUF *why;
/*	char	*dest;
/*	char	*host;
/*	unsigned port;
/*	int	valid;
/*
/*	SMTP_TLS_SESS *smtp_tls_sess_free(tls)
/*	SMTP_TLS_SESS *tls;
/* DESCRIPTION
/*	smtp_tls_list_init() initializes lookup tables used by the TLS
/*	policy engine.
/*
/*	smtp_tls_sess_alloc() allocates memory for an SMTP_TLS_SESS structure
/*	and initializes it based on the given information.  Any required
/*	table and DNS lookups can fail.  When this happens, "why" is updated
/*	with the error reason and a null pointer is returned.  NOTE: the
/*	port is in network byte order.  If "dest" is null, no policy checks are
/*	made, rather a trivial policy with tls disabled is returned (the
/*	remaining arguments are unused in this case and may be null).
/*
/*	smtp_tls_sess_free() destroys an SMTP_TLS_SESS structure and its
/*	members.  A null pointer is returned for convenience.
/*
/*	Arguments:
/* .IP dest
/*	The unmodified next-hop or fall-back destination including
/*	the optional [] and including the optional port or service.
/* .IP host
/*	The name of the host that we are connected to.  If the name was
/*	obtained via an MX lookup and/or CNAME expansion (any indirection),
/*	all those lookups must also have been validated.  If the hostname
/*	is validated, it must be the final name after all CNAME expansion.
/*	Otherwise, it is generally the original name or that returned by
/*	the MX lookup (see smtp_cname_overrides_servername).
/* .IP port
/*	The remote port, network byte order.
/* .IP valid
/*	The DNSSEC validation status of the host name.
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
/*	Viktor Dukhovni
/*
/*	TLS support originally by:
/*	Lutz Jaenicke
/*	BTU Cottbus
/*	Allgemeine Elektrotechnik
/*	Universitaetsplatz 3-4
/*	D-03044 Cottbus, Germany
/*--*/

/* System library. */

#include <sys_defs.h>

#ifdef USE_TLS

#include <stdlib.h>
#include <string.h>

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <vstring.h>
#include <stringops.h>
#include <valid_hostname.h>

/* Global library. */

#include <mail_params.h>
#include <maps.h>

/* Application-specific. */

#include "smtp.h"

static MAPS *tls_policy;		/* lookup table(s) */
static MAPS *tls_per_site;		/* lookup table(s) */

/* smtp_tls_list_init - initialize per-site policy lists */

void    smtp_tls_list_init(void)
{
    if (*var_smtp_tls_policy) {
	tls_policy = maps_create(SMTP_X(TLS_POLICY), var_smtp_tls_policy,
				 DICT_FLAG_LOCK | DICT_FLAG_FOLD_FIX);
	if (*var_smtp_tls_per_site)
	    msg_warn("%s ignored when %s is not empty.",
		     SMTP_X(TLS_PER_SITE), SMTP_X(TLS_POLICY));
	return;
    }
    if (*var_smtp_tls_per_site) {
	tls_per_site = maps_create(SMTP_X(TLS_PER_SITE), var_smtp_tls_per_site,
				   DICT_FLAG_LOCK | DICT_FLAG_FOLD_FIX);
    }
}

/* policy_name - printable tls policy level */

static const char *policy_name(int tls_level)
{
    const char *name = str_tls_level(tls_level);

    if (name == 0)
	name = "unknown";
    return name;
}

/* tls_site_lookup - look up per-site TLS security level */

static void tls_site_lookup(SMTP_TLS_SESS *tls, int *site_level,
		              const char *site_name, const char *site_class,
			            DSN_BUF *why)
{
    const char *lookup;

    /*
     * Look up a non-default policy. In case of multiple lookup results, the
     * precedence order is a permutation of the TLS enforcement level order:
     * VERIFY, ENCRYPT, NONE, MAY, NOTFOUND. I.e. we override MAY with a more
     * specific policy including NONE, otherwise we choose the stronger
     * enforcement level.
     */
    if ((lookup = maps_find(tls_per_site, site_name, 0)) != 0) {
	if (!strcasecmp(lookup, "NONE")) {
	    /* NONE overrides MAY or NOTFOUND. */
	    if (*site_level <= TLS_LEV_MAY)
		*site_level = TLS_LEV_NONE;
	} else if (!strcasecmp(lookup, "MAY")) {
	    /* MAY overrides NOTFOUND but not NONE. */
	    if (*site_level < TLS_LEV_NONE)
		*site_level = TLS_LEV_MAY;
	} else if (!strcasecmp(lookup, "MUST_NOPEERMATCH")) {
	    if (*site_level < TLS_LEV_ENCRYPT)
		*site_level = TLS_LEV_ENCRYPT;
	} else if (!strcasecmp(lookup, "MUST")) {
	    if (*site_level < TLS_LEV_VERIFY)
		*site_level = TLS_LEV_VERIFY;
	} else {
	    msg_warn("%s: unknown TLS policy '%s' for %s %s",
		     tls_per_site->title, lookup, site_class, site_name);
	    dsb_simple(why, "4.7.5", "client TLS configuration problem");
	    *site_level = TLS_LEV_INVALID;
	    return;
	}
    } else if (tls_per_site->error) {
	msg_warn("%s: %s \"%s\": per-site table lookup error",
		 tls_per_site->title, site_class, site_name);
	dsb_simple(why, "4.3.0", "Temporary lookup error");
	*site_level = TLS_LEV_INVALID;
	return;
    }
    return;
}

/* tls_policy_lookup_one - look up destination TLS policy */

static void tls_policy_lookup_one(SMTP_TLS_SESS *tls, int *site_level,
				          const char *site_name,
			               const char *site_class, DSN_BUF *why)
{
    const char *lookup;
    char   *policy;
    char   *saved_policy;
    char   *tok;
    const char *err;
    char   *name;
    char   *val;
    static VSTRING *cbuf;

#undef FREE_RETURN
#define FREE_RETURN do { myfree(saved_policy); return; } while (0)

#define WHERE \
    vstring_str(vstring_sprintf(cbuf, "%s, %s \"%s\"", \
		tls_policy->title, site_class, site_name))

    if (cbuf == 0)
	cbuf = vstring_alloc(10);

    if ((lookup = maps_find(tls_policy, site_name, 0)) == 0) {
	if (tls_policy->error) {
	    msg_warn("%s: policy table lookup error", WHERE);
	    dsb_simple(why, "4.3.0", "Temporary lookup error");
	    *site_level = TLS_LEV_INVALID;
	}
	return;
    }
    saved_policy = policy = mystrdup(lookup);

    if ((tok = mystrtok(&policy, "\t\n\r ,")) == 0) {
	msg_warn("%s: invalid empty policy", WHERE);
	dsb_simple(why, "4.7.5", "client TLS configuration problem");
	*site_level = TLS_LEV_INVALID;
	FREE_RETURN;
    }
    *site_level = tls_level_lookup(tok);
    if (*site_level == TLS_LEV_INVALID) {
	/* tls_level_lookup() logs no warning. */
	msg_warn("%s: invalid security level \"%s\"", WHERE, tok);
	dsb_simple(why, "4.7.5", "client TLS configuration problem");
	FREE_RETURN;
    }

    /*
     * Warn about ignored attributes when TLS is disabled.
     */
    if (*site_level < TLS_LEV_MAY) {
	while ((tok = mystrtok(&policy, "\t\n\r ,")) != 0)
	    msg_warn("%s: ignoring attribute \"%s\" with TLS disabled",
		     WHERE, tok);
	FREE_RETURN;
    }

    /*
     * Errors in attributes may have security consequences, don't ignore
     * errors that can degrade security.
     */
    while ((tok = mystrtok(&policy, "\t\n\r ,")) != 0) {
	if ((err = split_nameval(tok, &name, &val)) != 0) {
	    msg_warn("%s: malformed attribute/value pair \"%s\": %s",
		     WHERE, tok, err);
	    dsb_simple(why, "4.7.5", "client TLS configuration problem");
	    *site_level = TLS_LEV_INVALID;
	    FREE_RETURN;
	}
	/* Only one instance per policy. */
	if (!strcasecmp(name, "ciphers")) {
	    if (*val == 0) {
		msg_warn("%s: attribute \"%s\" has empty value", WHERE, name);
		dsb_simple(why, "4.7.5", "client TLS configuration problem");
		*site_level = TLS_LEV_INVALID;
		FREE_RETURN;
	    }
	    if (tls->grade) {
		msg_warn("%s: attribute \"%s\" is specified multiple times",
			 WHERE, name);
		dsb_simple(why, "4.7.5", "client TLS configuration problem");
		*site_level = TLS_LEV_INVALID;
		FREE_RETURN;
	    }
	    tls->grade = mystrdup(val);
	    continue;
	}
	/* Only one instance per policy. */
	if (!strcasecmp(name, "protocols")) {
	    if (tls->protocols) {
		msg_warn("%s: attribute \"%s\" is specified multiple times",
			 WHERE, name);
		dsb_simple(why, "4.7.5", "client TLS configuration problem");
		*site_level = TLS_LEV_INVALID;
		FREE_RETURN;
	    }
	    tls->protocols = mystrdup(val);
	    continue;
	}
	/* Multiple instances per policy. */
	if (!strcasecmp(name, "match")) {
	    char   *delim = *site_level == TLS_LEV_FPRINT ? "|" : ":";

	    if (*site_level <= TLS_LEV_ENCRYPT) {
		msg_warn("%s: attribute \"%s\" invalid at security level "
			 "\"%s\"", WHERE, name, policy_name(*site_level));
		dsb_simple(why, "4.7.5", "client TLS configuration problem");
		*site_level = TLS_LEV_INVALID;
		FREE_RETURN;
	    }
	    if (*val == 0) {
		msg_warn("%s: attribute \"%s\" has empty value", WHERE, name);
		dsb_simple(why, "4.7.5", "client TLS configuration problem");
		*site_level = TLS_LEV_INVALID;
		FREE_RETURN;
	    }
	    if (tls->matchargv == 0)
		tls->matchargv = argv_split(val, delim);
	    else
		argv_split_append(tls->matchargv, val, delim);
	    continue;
	}
	/* Only one instance per policy. */
	if (!strcasecmp(name, "exclude")) {
	    if (tls->exclusions) {
		msg_warn("%s: attribute \"%s\" is specified multiple times",
			 WHERE, name);
		dsb_simple(why, "4.7.5", "client TLS configuration problem");
		*site_level = TLS_LEV_INVALID;
		FREE_RETURN;
	    }
	    tls->exclusions = vstring_strcpy(vstring_alloc(10), val);
	    continue;
	}
	msg_warn("%s: invalid attribute name: \"%s\"", WHERE, name);
	dsb_simple(why, "4.7.5", "client TLS configuration problem");
	*site_level = TLS_LEV_INVALID;
	FREE_RETURN;
    }

    FREE_RETURN;
}

/* tls_policy_lookup - look up destination TLS policy */

static void tls_policy_lookup(SMTP_TLS_SESS *tls, int *site_level,
			              const char *site_name,
			              const char *site_class, DSN_BUF *why)
{

    /*
     * Only one lookup with [nexthop]:port, [nexthop] or nexthop:port These
     * are never the domain part of localpart@domain, rather they are
     * explicit nexthops from transport:nexthop, and match only the
     * corresponding policy. Parent domain matching (below) applies only to
     * sub-domains of the recipient domain.
     */
    if (!valid_hostname(site_name, DONT_GRIPE)) {
	tls_policy_lookup_one(tls, site_level, site_name, site_class, why);
	return;
    }
    do {
	tls_policy_lookup_one(tls, site_level, site_name, site_class, why);
    } while (*site_level == TLS_LEV_NOTFOUND
	     && (site_name = strchr(site_name + 1, '.')) != 0);
}

/* set_cipher_grade - Set cipher grade and exclusions */

static void set_cipher_grade(SMTP_TLS_SESS *tls)
{
    const char *mand_exclude = "";
    const char *also_exclude = "";

    /*
     * Use main.cf cipher level if no per-destination value specified. With
     * mandatory encryption at least encrypt, and with mandatory verification
     * at least authenticate!
     */
    switch (tls->level) {
    case TLS_LEV_INVALID:
    case TLS_LEV_NONE:
	return;

    case TLS_LEV_MAY:
	if (tls->grade == 0)
	    tls->grade = mystrdup(var_smtp_tls_ciph);
	break;

    case TLS_LEV_ENCRYPT:
	if (tls->grade == 0)
	    tls->grade = mystrdup(var_smtp_tls_mand_ciph);
	mand_exclude = var_smtp_tls_mand_excl;
	also_exclude = "eNULL";
	break;

    case TLS_LEV_FPRINT:
    case TLS_LEV_VERIFY:
    case TLS_LEV_SECURE:
	if (tls->grade == 0)
	    tls->grade = mystrdup(var_smtp_tls_mand_ciph);
	mand_exclude = var_smtp_tls_mand_excl;
	also_exclude = "aNULL";
	break;
    }

#define ADD_EXCLUDE(vstr, str) \
    do { \
	if (*(str)) \
	    vstring_sprintf_append((vstr), "%s%s", \
				   VSTRING_LEN(vstr) ? " " : "", (str)); \
    } while (0)

    /*
     * The "exclude" policy table attribute overrides main.cf exclusion
     * lists.
     */
    if (tls->exclusions == 0) {
	tls->exclusions = vstring_alloc(10);
	ADD_EXCLUDE(tls->exclusions, var_smtp_tls_excl_ciph);
	ADD_EXCLUDE(tls->exclusions, mand_exclude);
    }
    ADD_EXCLUDE(tls->exclusions, also_exclude);
}

/* smtp_tls_sess_alloc - session TLS policy parameters */

SMTP_TLS_SESS *smtp_tls_sess_alloc(DSN_BUF *why, const char *dest,
			         const char *host, unsigned port, int valid)
{
    const char *myname = "smtp_tls_sess_alloc";
    int     global_level;
    int     site_level;
    SMTP_TLS_SESS *tls = (SMTP_TLS_SESS *) mymalloc(sizeof(*tls));

    tls->level = TLS_LEV_NONE;
    tls->protocols = 0;
    tls->grade = 0;
    tls->exclusions = 0;
    tls->matchargv = 0;

    if (!dest)
	return (tls);

    /*
     * Compute the global TLS policy. This is the default policy level when
     * no per-site policy exists. It also is used to override a wild-card
     * per-site policy.
     */
    if (*var_smtp_tls_level) {
	/* Require that var_smtp_tls_level is sanitized upon startup. */
	global_level = tls_level_lookup(var_smtp_tls_level);
	if (global_level == TLS_LEV_INVALID)
	    msg_panic("%s: invalid TLS security level: \"%s\"",
		      myname, var_smtp_tls_level);
    } else if (var_smtp_enforce_tls) {
	global_level = var_smtp_tls_enforce_peername ?
	    TLS_LEV_VERIFY : TLS_LEV_ENCRYPT;
    } else {
	global_level = var_smtp_use_tls ?
	    TLS_LEV_MAY : TLS_LEV_NONE;
    }
    if (msg_verbose)
	msg_info("%s TLS level: %s", "global", policy_name(global_level));

    /*
     * Compute the per-site TLS enforcement level. For compatibility with the
     * original TLS patch, this algorithm is gives equal precedence to host
     * and next-hop policies.
     */
    site_level = TLS_LEV_NOTFOUND;

    if (tls_policy) {
	tls_policy_lookup(tls, &site_level, dest, "next-hop destination", why);
    } else if (tls_per_site) {
	tls_site_lookup(tls, &site_level, dest, "next-hop destination", why);
	if (site_level != TLS_LEV_INVALID
	    && strcasecmp(dest, host) != 0)
	    tls_site_lookup(tls, &site_level, host, "server hostname", why);

	/*
	 * Override a wild-card per-site policy with a more specific global
	 * policy.
	 * 
	 * With the original TLS patch, 1) a per-site ENCRYPT could not override
	 * a global VERIFY, and 2) a combined per-site (NONE+MAY) policy
	 * produced inconsistent results: it changed a global VERIFY into
	 * NONE, while producing MAY with all weaker global policy settings.
	 * 
	 * With the current implementation, a combined per-site (NONE+MAY)
	 * consistently overrides global policy with NONE, and global policy
	 * can override only a per-site MAY wildcard. That is, specific
	 * policies consistently override wildcard policies, and
	 * (non-wildcard) per-site policies consistently override global
	 * policies.
	 */
	if (site_level == TLS_LEV_MAY && global_level > TLS_LEV_MAY)
	    site_level = global_level;
    }
    switch (site_level) {
    case TLS_LEV_INVALID:
	return (smtp_tls_sess_free(tls));
    case TLS_LEV_NOTFOUND:
	tls->level = global_level;
	break;
    default:
	tls->level = site_level;
	break;
    }

    /*
     * Use main.cf protocols setting if not set in per-destination table.
     */
    if (tls->level > TLS_LEV_NONE && tls->protocols == 0)
	tls->protocols =
	    mystrdup((tls->level == TLS_LEV_MAY) ?
		     var_smtp_tls_proto : var_smtp_tls_mand_proto);

    /*
     * Compute cipher grade (if set in per-destination table, else
     * set_cipher() uses main.cf settings) and security level dependent
     * cipher exclusion list.
     */
    set_cipher_grade(tls);

    /*
     * Use main.cf cert_match setting if not set in per-destination table.
     */
    switch (tls->level) {
    case TLS_LEV_INVALID:
    case TLS_LEV_NONE:
    case TLS_LEV_MAY:
    case TLS_LEV_ENCRYPT:
    case TLS_LEV_DANE:
	break;
    case TLS_LEV_FPRINT:
	if (tls->matchargv == 0)
	    tls->matchargv =
		argv_split(var_smtp_tls_fpt_cmatch, "\t\n\r, |");
	break;
    case TLS_LEV_VERIFY:
	if (tls->matchargv == 0)
	    tls->matchargv =
		argv_split(var_smtp_tls_vfy_cmatch, "\t\n\r, :");
	break;
    case TLS_LEV_SECURE:
	if (tls->matchargv == 0)
	    tls->matchargv =
		argv_split(var_smtp_tls_sec_cmatch, "\t\n\r, :");
	break;
    default:
	msg_panic("unexpected TLS security level: %d",
		  tls->level);
    }

    if (msg_verbose && (tls_policy || tls_per_site))
	msg_info("%s TLS level: %s", "effective", policy_name(tls->level));

    return (tls);
}

/* smtp_sess_tls_free - free and return null pointer of same type */

SMTP_TLS_SESS *smtp_tls_sess_free(SMTP_TLS_SESS *tls)
{

    if (tls->protocols)
	myfree(tls->protocols);
    if (tls->grade)
	myfree(tls->grade);
    if (tls->exclusions)
	vstring_free(tls->exclusions);
    if (tls->matchargv)
	argv_free(tls->matchargv);

    myfree((char *) tls);
    return (0);
}

#endif
