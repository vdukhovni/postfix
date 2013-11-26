/*++
/* NAME
/*	postconf_match 3
/* SUMMARY
/*	pattern-matching support
/* SYNOPSIS
/*	#include <postconf.h>
/*
/*	int	parse_field_pattern(field_expr)
/*	char	*field_expr;
/*
/*	const char *str_field_pattern(field_pattern)
/*	int	field_pattern;
/*
/*	int	is_magic_field_pattern(field_pattern)
/*	int	field_pattern;
/*
/*	ARGV	*parse_service_pattern(service_expr, min_expr, max_expr)
/*	const char *service_expr;
/*	int	min_expr;
/*	int	max_expr;
/*
/*	int	IS_MAGIC_SERVICE_PATTERN(service_pattern)
/*	const ARGV *service_pattern;
/*
/*	int	MATCH_SERVICE_PATTERN(service_pattern, service_name,
/*					service_type)
/*	const ARGV *service_pattern;
/*	const char *service_name;
/*	const char *service_type;
/*
/*	const char *str_field_pattern(field_pattern)
/*	int	field_pattern;
/*
/*	int	IS_MAGIC_PARAM_PATTERN(param_pattern)
/*	const char *param_pattern;
/*
/*	int	MATCH_PARAM_PATTERN(param_pattern, param_name)
/*	const char *param_pattern;
/*	const char *param_name;
/* DESCRIPTION
/*	parse_service_pattern() takes an expression and splits it
/*	up on '/' into an array of sub-expressions, This function
/*	returns null if the input does fewer than "min" or more
/*	than "max" sub-expressions.
/*
/*	IS_MAGIC_SERVICE_PATTERN() returns non-zero if any of the
/*	service name or service type sub-expressions contains a
/*	matching operator (as opposed to string literals that only
/*	match themselves). This is an unsafe macro that evaluates
/*	its arguments more than once.
/*
/*	MATCH_SERVICE_PATTERN() matches a service name and type
/*	from master.cf against the parsed pattern. This is an unsafe
/*	macro that evaluates its arguments more than once.
/*
/*	parse_field_pattern() converts a field sub-expression, and
/*	returns the conversion result.
/*
/*	str_field_pattern() converts a result from parse_field_pattern()
/*	into string form.
/*
/*	is_magic_field_pattern() returns non-zero if the field
/*	pattern sub-expression contained a matching operator (as
/*	opposed to a string literal that only matches itself).
/*
/*	IS_MAGIC_PARAM_PATTERN() returns non-zero if the parameter
/*	sub-expression contains a matching operator (as opposed to
/*	a string literal that only matches itself). This is an
/*	unsafe macro that evaluates its arguments more than once.
/*
/*	MATCH_PARAM_PATTERN() matches a parameter name from master.cf
/*	against the parsed pattern. This is an unsafe macro that
/*	evaluates its arguments more than once.
/*
/*	Arguments
/* .IP field_expr
/*	A field expression.
/* .IP service_expr
/*	This argument is split on '/' into its constituent
/*	sub-expressions.
/* .IP min_expr
/*	The minimum number of sub-expressions in service_expr.
/* .IP max_expr
/*	The maximum number of sub-expressions in service_expr.
/* .IP service_name
/*	Service name from master.cf.
/* .IP service_type
/*	Service type from master.cf.
/* .IP param_pattern
/*	A parameter name expression.
/* DIAGNOSTICS
/*	Fatal errors: invalid syntax.
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

/* Global library. */

#include <split_at.h>

/* Application-specific. */

#include <postconf.h>

 /*
  * Conversion table. Each PC_MASTER_NAME_XXX name entry must be stored at
  * table offset PC_MASTER_FIELD_XXX. So don't mess it up.
  */
NAME_CODE field_name_offset[] = {
    PC_MASTER_NAME_SERVICE, PC_MASTER_FIELD_SERVICE,
    PC_MASTER_NAME_TYPE, PC_MASTER_FIELD_TYPE,
    PC_MASTER_NAME_PRIVATE, PC_MASTER_FIELD_PRIVATE,
    PC_MASTER_NAME_UNPRIV, PC_MASTER_FIELD_UNPRIV,
    PC_MASTER_NAME_CHROOT, PC_MASTER_FIELD_CHROOT,
    PC_MASTER_NAME_WAKEUP, PC_MASTER_FIELD_WAKEUP,
    PC_MASTER_NAME_MAXPROC, PC_MASTER_FIELD_MAXPROC,
    PC_MASTER_NAME_CMD, PC_MASTER_FIELD_CMD,
    "*", PC_MASTER_FIELD_WILDC,
    0, PC_MASTER_FIELD_NONE,
};

/* parse_field_pattern - parse service attribute pattern */

int     parse_field_pattern(const char *field_name)
{
    int     field_pattern;

    if ((field_pattern = name_code(field_name_offset,
				   NAME_CODE_FLAG_STRICT_CASE,
				   field_name)) == PC_MASTER_FIELD_NONE)
	msg_fatal("invalid service attribute name: \"%s\"", field_name);
    return (field_pattern);
}

/* parse_service_pattern - parse service pattern */

ARGV   *parse_service_pattern(const char *pattern, int min_expr, int max_expr)
{
    ARGV   *argv;
    char  **cpp;

    /*
     * Work around argv_split() lameness.
     */
    if (*pattern == '/')
	return (0);
    argv = argv_split(pattern, PC_NAMESP_SEP_STR);
    if (argv->argc < min_expr || argv->argc > max_expr) {
	argv_free(argv);
	return (0);
    }

    /*
     * Allow '*' only all by itself.
     */
    for (cpp = argv->argv; *cpp; cpp++) {
	if (!PC_MATCH_ANY(*cpp) && strchr(*cpp, PC_MATCH_WILDC_STR[0]) != 0) {
	    argv_free(argv);
	    return (0);
	}
    }

    /*
     * Provide defaults for missing fields.
     */
    while (argv->argc < max_expr)
	argv_add(argv, PC_MATCH_WILDC_STR, ARGV_END);
    return (argv);
}
