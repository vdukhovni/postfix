/*++
/* NAME
/*	postconf_lookup 3
/* SUMMARY
/*	parameter lookup routines
/* SYNOPSIS
/*	#include <postconf.h>
/*
/*	const char *lookup_parameter_value(mode, name, local_scope, node)
/*	int	mode;
/*	const char *name;
/*	PC_MASTER_ENT *local_scope;
/*	PC_PARAM_NODE *node;
/*
/*	const char *expand_parameter_value(mode, value, local_scope)
/*	int	mode;
/*	const char *value;
/*	PC_MASTER_ENT *local_scope;
/* DESCRIPTION
/*	These functions perform parameter value lookups.  The order
/*	of decreasing precedence is:
/* .IP \(bu
/*	Search name=value parameter settings in master.cf.
/*	These lookups are disabled with the SHOW_DEFS flag.
/* .IP \(bu
/*	Search name=value parameter settings in main.cf.
/*	These lookups are disabled with the SHOW_DEFS flag.
/* .IP \(bu
/*	Search built-in default parameter settings. These lookups
/*	are disabled with the SHOW_NONDEF flag.
/* .PP
/*	lookup_parameter_value() looks up the value for the named
/*	parameter, and returns null if the name was not found.
/*
/*	expand_parameter_value() expands $name in the specified
/*	parameter value. This function ignores the SHOW_NONDEF flag.
/*	The result is in static memory that is overwritten with
/*	each call.
/*
/*	Arguments:
/* .IP mode
/*	Bit-wise OR of zero or one of the following (other flags
/*	are ignored):
/* .RS
/* .IP SHOW_DEFS
/*	Search built-in default parameter settings only.
/* .IP SHOW_NONDEF
/*	Search local (master.cf) or global (main.cf) name=value
/*	parameter settings only.
/* .RE
/* .IP name
/*	The name of a parameter to be looked up.
/* .IP value
/*	The parameter value where $name should be expanded.
/* .IP local_scope
/*	Null pointer, or pointer to master.cf entry with local
/*	parameter definitions.
/* .IP node
/*	Null pointer, or global default settings for the named
/*	parameter.
/* DIAGNOSTICS
/*	Problems are reported to the standard error stream.
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
#include <string.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <vstring.h>
#include <dict.h>
#include <stringops.h>
#include <mac_expand.h>

/* Global library. */

#include <mail_conf.h>

/* Application-specific. */

#include <postconf.h>

#define STR(x) vstring_str(x)

/* lookup_parameter_value - look up specific parameter value */

const char *lookup_parameter_value(int mode, const char *name,
				           PC_MASTER_ENT *local_scope,
				           PC_PARAM_NODE *node)
{
    const char *value = 0;

    /*
     * Use the actual or built-in default parameter value. Local name=value
     * entries in master.cf take precedence over global name=value entries in
     * main.cf. Built-in defaults have the lowest precedence.
     */
    if ((mode & SHOW_DEFS) != 0
	|| ((local_scope == 0 || local_scope->all_params == 0
	     || (value = dict_get(local_scope->all_params, name)) == 0)
	    && (value = dict_lookup(CONFIG_DICT, name)) == 0
	    && (mode & SHOW_NONDEF) == 0)) {
	if (node != 0 || (node = PC_PARAM_TABLE_FIND(param_table, name)) != 0)
	    value = convert_param_node(SHOW_DEFS, name, node);
    }
    return (value);
}

 /*
  * Data structure to pass private state while recursively expanding $name in
  * parameter values.
  */
typedef struct {
    int     mode;
    PC_MASTER_ENT *local_scope;
} PC_EVAL_CTX;

/* expand_parameter_value_helper - macro parser call-back routine */

static const char *expand_parameter_value_helper(const char *key,
						         int unused_type,
						         char *context)
{
    PC_EVAL_CTX *cp = (PC_EVAL_CTX *) context;

    return (lookup_parameter_value(cp->mode, key, cp->local_scope,
				   (PC_PARAM_NODE *) 0));
}

/* expand_parameter_value - expand $name in parameter value */

const char *expand_parameter_value(int mode, const char *value,
				           PC_MASTER_ENT *local_scope)
{
    const char *myname = "expand_parameter_value";
    static VSTRING *buf;
    int     status;
    PC_EVAL_CTX eval_ctx;

    /*
     * Initialize.
     */
    if (buf == 0)
	buf = vstring_alloc(10);

    /*
     * Expand macros recursively.
     * 
     * When expanding $name in "postconf -n" parameter values, don't limit the
     * search to only non-default parameter values.
     * 
     * When expanding $name in "postconf -d" parameter values, do limit the
     * search to only default parameter values.
     */
#define DONT_FILTER (char *) 0

    eval_ctx.mode = (mode & ~SHOW_NONDEF);
    eval_ctx.local_scope = local_scope;
    status = mac_expand(buf, value, MAC_EXP_FLAG_RECURSE, DONT_FILTER,
			expand_parameter_value_helper, (char *) &eval_ctx);
    if (status & MAC_PARSE_ERROR)
	msg_fatal("macro processing error");
    if (msg_verbose > 1) {
	if (strcmp(value, STR(buf)) != 0)
	    msg_info("%s: expand %s -> %s", myname, value, STR(buf));
	else
	    msg_info("%s: const  %s", myname, value);
    }
    return (STR(buf));
}
