/*++
/* NAME
/*	smtp_reqtls_policy 3
/* SUMMARY
/*	requiretls next-hop policy
/* SYNOPSIS
/*	#include <smtp_reqtls_policy.h>
/*
/*	SMTP_REQTLS_POLICY *smtp_reqtls_policy_parse(
/*	const char *origin,
/*	const char *extern_policy)
/*
/*	int	smtp_reqtls_policy_eval(
/*	SMTP_REQTLS_POLICY *intern_policy,
/*	const char *nexthop_name)
/*
/*	void	smtp_reqtls_policy_free(
/*	SMTP_REQTLS_POLICY *intern_policy)
/* DESCRIPTION
/*	This module determines the REQUIRETLS enforcement level for
/*	connections to a next-hop destination.
/*
/*	smtp_reqtls_policy_parse() converts a policy from human-readable
/*	form to internal form. It should be called as part of
/*	before-chroot initialization. A policy is a list of elements
/*	that will be matched in the specified order. A policy element must
/*	be an atom ("enforce", "opportunistic+starttls", "opportunistic",
/*	"error") or a type:table. A table lookup result must be an
/*	atom, not a type:table. To match a parent domain name with a
/*	table that wants and exact match, specify an explicit '.' before
/*	the parent domain name.
/*
/*	smtp_reqtls_policy_eval() evaluates an internal-form
/*	policy for the specified next-hop destination. The
/*	result is SMTP_REQTLS_POLICY_ACT_ENFORCE (enforce),
/*	SMTP_REQTLS_POLICY_ACT_OPP_TLS (opportunistic+starttls),
/*	SMTP_REQTLS_POLICY_ACT_OPPORTUNISTIC (opportunistic),
/*	SMTP_REQTLS_POLICY_ACT_DISABLE, or SMTP_REQTLS_POLICY_ACT_ERROR
/*	(unknown command or database access error).
/*
/*	smtp_reqtls_policy_free() destroys an internal-form policy.
/*
/*	Arguments:
/* .IP origin
/*	The configuration parameter name for the policy from main.cf.
/*	This is used for error reporting only.
/* .IP extern_policy
/*	External policy representation.
/* .IP intern_policy
/*	Internal policy representation.
/* .IP nexthop_name
/*	Next-hop destination without [ ], :service,  or :port.
/* HISTORY
/*	This as derived from the Postfix server_policy(3) module.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>
#include <string.h>

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

 /*
  * Utility library.
  */
#include <msg.h>
#include <mymalloc.h>
#include <stringops.h>
#include <dict.h>

 /*
  * Global library.
  */
#include <mail_params.h>

 /*
  * Application-specific.
  */
#include <smtp_reqtls_policy.h>

/* smtp_reqtls_policy_parse - parse policy */

SMTP_REQTLS_POLICY *smtp_reqtls_policy_parse(const char *origin,
					          const char *extern_policy)
{
    char   *saved_policy = mystrdup(extern_policy);
    SMTP_REQTLS_POLICY *intern_policy = argv_alloc(1);
    char   *bp = saved_policy;
    char   *item;

#define STREQ(x,y) (strcasecmp((x), (y)) == 0)

    /*
     * Keep it simple, and prepend the origin to the actual policy.
     */
    argv_add(intern_policy, origin, (char *) 0);

    /*
     * Nested tables are not allowed. Tables are opened before entering the
     * chroot jail, while policies are evaluated after entering the chroot
     * jail.
     */
    while ((item = mystrtokq(&bp, CHARS_COMMA_SP, CHARS_BRACE)) != 0) {
	if (strchr(item, ':') != 0) {
	    if (strchr(origin, ':') != 0) {
		msg_warn("table %s: nested lookup result \"%s\" is not allowed"
			 " -- ignoring remainder of policy",
			 origin, item);
		argv_add(intern_policy, SMTP_REQTLS_POLICY_NAME_ERROR, (char *) 0);
		break;
	    } else {
		item = dict_open(item, O_RDONLY, DICT_FLAG_LOCK
				 | DICT_FLAG_FOLD_FIX
				 | DICT_FLAG_UTF8_REQUEST)->reg_name;
	    }
	}
	argv_add(intern_policy, item, (char *) 0);
    }
    argv_terminate(intern_policy);

    /*
     * Cleanup.
     */
    myfree(saved_policy);
    return (intern_policy);
}

/* smtp_reqtls_policy_eval - evaluate policy */

int     smtp_reqtls_policy_eval(SMTP_REQTLS_POLICY *intern_policy,
				        const char *nexthop_name)
{
    char  **cpp;
    DICT   *dict;
    SMTP_REQTLS_POLICY *argv;
    const char *item;
    const char *dict_val;
    int     ret;
    const char *name;
    const char *next;
    const char *origin;

    origin = intern_policy->argv[0];
    for (cpp = intern_policy->argv + 1; (item = *cpp) != 0; cpp++) {
	if (msg_verbose)
	    msg_info("origin=%s name=%s item=%s", origin, nexthop_name, item);
	if (STREQ(item, SMTP_REQTLS_POLICY_NAME_OPP_TLS)) {
	    return (SMTP_REQTLS_POLICY_ACT_OPP_TLS);
	} else if (STREQ(item, SMTP_REQTLS_POLICY_NAME_ENFORCE)) {
	    return (SMTP_REQTLS_POLICY_ACT_ENFORCE);
	} else if (STREQ(item, SMTP_REQTLS_POLICY_NAME_OPPORTUNISTIC)) {
	    return (SMTP_REQTLS_POLICY_ACT_OPPORTUNISTIC);
	} else if (STREQ(item, SMTP_REQTLS_POLICY_NAME_DISABLE)) {
	    return (SMTP_REQTLS_POLICY_ACT_DISABLE);
	} else if (strchr(item, ':') != 0) {
	    if ((dict = dict_handle(item)) == 0)
		msg_panic("%s: unexpected dictionary: %s", __func__, item);
	    for (name = nexthop_name; name != 0; name = next) {
		if ((dict_val = dict_get(dict, name)) != 0) {
		    /* Simple atom. Avoid four malloc() and free() calls. */
		    if (dict_val[strcspn(dict_val, ":" CHARS_COMMA_SP)] == 0) {
			ARGV_FAKE2_BEGIN(fake_argv, item, dict_val);
			ret = smtp_reqtls_policy_eval(&fake_argv, name);
			ARGV_FAKE_END;
		    }
		    /* Composite or dictionary. */
		    else {
			argv = smtp_reqtls_policy_parse(item, dict_val);
			ret = smtp_reqtls_policy_eval(argv, name);
			argv_free(argv);
		    }
		    return (ret);
		} else if (dict->error != 0) {
		    msg_warn("%s: %s:%s: table lookup error -- ignoring the "
			     "remainder of this policy", origin, dict->type,
			     dict->name);
		    return (SMTP_REQTLS_POLICY_ACT_ERROR);
		}
		/* Look up .parent if the table wants an exact match. */
		if ((dict->flags & DICT_FLAG_FIXED) == 0)
		    break;
		next = strchr(name + 1, '.');
	    }
	} else if (STREQ(item, SMTP_REQTLS_POLICY_NAME_ERROR)) {
	    return (SMTP_REQTLS_POLICY_ACT_ERROR);
	} else {
	    msg_warn("%s: unknown policy action: '%s' -- ignoring the "
		     "remainder of this policy", origin, item);
	    return (SMTP_REQTLS_POLICY_ACT_ERROR);
	}
    }
    if (msg_verbose)
	msg_info("origin=%s name=%s - no match", origin, nexthop_name);
    return (SMTP_REQTLS_POLICY_ACT_ENFORCE);
}

/* smtp_reqtls_policy_free - release storage for policy and lookup tables */

void    smtp_reqtls_policy_free(SMTP_REQTLS_POLICY *intern_policy)
{
    char  **cpp;
    DICT   *dict;
    const char *item;

    for (cpp = intern_policy->argv + 1; (item = *cpp) != 0; cpp++) {
	if (strchr(item, ':') != 0) {
	    if ((dict = dict_handle(item)) == 0)
		msg_panic("%s: unexpected dictionary: %s", __func__, item);
	    dict_unregister(item);
	}
    }
    argv_free(intern_policy);
}
