/*++
/* NAME
/*	config_bool 3
/* SUMMARY
/*	boolean-valued configuration parameter support
/* SYNOPSIS
/*	#include <config.h>
/*
/*	int	get_config_bool(name, defval)
/*	const char *path;
/*	const char *name;
/*	int	defval;
/*
/*	int	get_config_bool_fn(name, defval)
/*	const char *path;
/*	const char *name;
/*	int	(*defval)();
/*
/*	void	set_config_bool(name, value)
/*	const char *name;
/*	int	value;
/*
/*	void	get_config_bool_table(table)
/*	CONFIG_BOOL_TABLE *table;
/*
/*	void	get_config_bool_fn_table(table)
/*	CONFIG_BOOL_TABLE *table;
/* DESCRIPTION
/*	This module implements configuration parameter support for
/*	boolean values. The internal representation is zero (false)
/*	and non-zero (true). The external representation is "no"
/*	(false) and "yes" (true). The conversion from external
/*	representation is case insensitive.
/*
/*	get_config_bool() looks up the named entry in the global
/*	configuration dictionary. The specified default value is
/*	returned when no value was found.
/*
/*	get_config_bool_fn() is similar but specifies a function that
/*	provides the default value. The function is called only
/*	when the default value is needed.
/*
/*	set_config_bool() updates the named entry in the global
/*	configuration dictionary. This has no effect on values that
/*	have been looked up earlier via the get_config_XXX() routines.
/*
/*	get_config_bool_table() and get_config_int_fn_table() initialize
/*	lists of variables, as directed by their table arguments. A table
/*	must be terminated by a null entry.
/* DIAGNOSTICS
/*	Fatal errors: malformed boolean value.
/* SEE ALSO
/*	config(3) general configuration
/*	config_str(3) string-valued configuration parameters
/*	config_int(3) integer-valued configuration parameters
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

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

/* Utility library. */

#include <msg.h>
#include <dict.h>

/* Global library. */

#include "config.h"

/* convert_config_bool - look up and convert boolean parameter value */

static int convert_config_bool(const char *name, int *intval)
{
    const char *strval;

    if ((strval = config_lookup_eval(name)) == 0) {
	return (0);
    } else {
	if (strcasecmp(strval, CONFIG_BOOL_YES) == 0) {
	    *intval = 1;
	} else if (strcasecmp(strval, CONFIG_BOOL_NO) == 0) {
	    *intval = 0;
	} else {
	    msg_fatal("bad boolean configuration: %s = %s", name, strval);
	}
	return (1);
    }
}

/* get_config_bool - evaluate boolean-valued configuration variable */

int     get_config_bool(const char *name, int defval)
{
    int     intval;

    if (convert_config_bool(name, &intval) == 0)
	set_config_bool(name, intval = defval);
    return (intval);
}

/* get_config_bool_fn - evaluate boolean-valued configuration variable */

typedef int (*stupid_indent_int) (void);

int     get_config_bool_fn(const char *name, stupid_indent_int defval)
{
    int     intval;

    if (convert_config_bool(name, &intval) == 0)
	set_config_bool(name, intval = defval());
    return (intval);
}

/* set_config_bool - update boolean-valued configuration dictionary entry */

void    set_config_bool(const char *name, int value)
{
    config_update(name, value ? CONFIG_BOOL_YES : CONFIG_BOOL_NO);
}

/* get_config_bool_table - look up table of booleans */

void    get_config_bool_table(CONFIG_BOOL_TABLE *table)
{
    while (table->name) {
	table->target[0] = get_config_bool(table->name, table->defval);
	table++;
    }
}

/* get_config_bool_fn_table - look up booleans, defaults are functions */

void    get_config_bool_fn_table(CONFIG_BOOL_FN_TABLE *table)
{
    while (table->name) {
	table->target[0] = get_config_bool_fn(table->name, table->defval);
	table++;
    }
}
