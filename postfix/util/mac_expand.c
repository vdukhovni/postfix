/*++
/* NAME
/*	mac_expand 3
/* SUMMARY
/*	attribute expansion
/* SYNOPSIS
/*	#include <mac_expand.h>
/*
/*	int	mac_expand(result, pattern, flags, key, ...)
/*	VSTRING *result;
/*	const char *pattern;
/*	int	flags;
/*	int	key;
/* DESCRIPTION
/*	This module implements parameter-less macro expansions, both
/*	conditional and unconditional, and both recursive and non-recursive.
/*	The algorithm can search multiple user-specified symbol tables.
/*	In the text below, an attribute is "defined" when its value is a
/*	string of non-zero length. In all other cases the attribute is
/*	considered "undefined".
/*
/*	The following expansions are implemented:
/* .IP "$name, ${name}, $(name)"
/*	Unconditional expansion. If the named attribute is defined, the
/*	expansion is the value of the named attribute,  optionally subjected
/*	to further $name expansions.  Otherwise, the expansion is empty.
/* .IP "${name?text}, $(name?text)"
/*	Conditional expansion. If the named attribute is defined, the
/*	expansion is the given text, subjected to another iteration of
/*	$name expansion.  Otherwise, the expansion is empty.
/* .IP "${name:text}, $(name:text)"
/*	Conditional expansion. If the named attribute is undefined, the
/*	the expansion is the given text, after another iteration of $name
/*	expansion.  Otherwise, the expansion is empty.
/* .PP
/*	mac_expand() replaces $name etc. instances in \fBpattern\fR
/*	and stores the result into \fBresult\fR.
/*
/*	Arguments:
/* .IP result
/*	Storage for the result of expansion. The result is truncated
/*	upon entry.
/* .IP pattern
/*	The string to be expanded.
/* .IP flags
/*	Bit-wise OR of zero or more of the following:
/* .RS
/* .IP MAC_EXP_FLAG_RECURSE
/*	Expand $name recursively.
/* .RE
/*	The constant MAC_EXP_FLAG_NONE specifies a manifest null value.
/* .IP key
/*	The attribute information is specified as a null-terminated list.
/*	Attributes may appear multiple times; the right-most definition
/*	of an attribute determines the result of attribute lookup.
/* .sp
/*	The following keys are understood (types of arguments indicated
/*	in parentheses):
/* .RS
/* .IP "MAC_EXP_ARG_ATTR (char *, char *)"
/*	The next two arguments specify an attribute name and its attribute
/*	string value.  Specify a null pointer or empty string for an
/*	attribute value that is unset. Attribute keys and string values
/*	are copied.
/* .IP "MAC_EXP_ARG_TABLE (HTABLE *)"
/*	The next argument is a hash table with attribute names and values.
/*	Specify a null pointer or empty string for an attribute value that
/*	is unset. Hash tables are not copied.
/* .IP "MAC_EXP_ARG_RECORD (HTABLE *)"
/*	Record in the specified table how many times an attribute was
/*	referenced.
/* .RE
/* .IP MAC_EXP_ARG_END
/*	A manifest constant that indicates the end of the argument list.
/* DIAGNOSTICS
/*	Fatal errors: out of memory.  Warnings: syntax errors, unreasonable
/*	macro nesting.
/*
/*	The result value is the binary OR of zero or more of the following:
/* .IP MAC_EXP_FLAG_ERROR
/*	A syntax error was foud in the \fBpattern\fR, or some macro had
/*	an unreasonable nesting depth.
/* .IP MAC_EXP_FLAG_UNDEF
/*	The pattern contains a reference to an undefined attribute.
/* SEE ALSO
/*	mac_parse(3) locate macro references in string.
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
#include <ctype.h>
#include <string.h>

/* Utility library. */

#include <msg.h>
#include <vstring.h>
#include <mymalloc.h>
#include <htable.h>
#include <mac_parse.h>
#include <mac_expand.h>

 /*
  * Little helper structure. Name-value pairs given as explicit arguments are
  * stored into private hash tables. Hash tables provided by the caller are
  * simply referenced. For now we do only hash tables. The structure can be
  * generalized when needed.
  */
struct table_info {
    int     status;			/* who owns this table */
    HTABLE *table;			/* table reference */
};

#define MAC_EXP_STAT_UNUSED	0	/* this slot is unused */
#define MAC_EXP_STAT_PRIVATE	1	/* we own this table */
#define MAC_EXP_STAT_EXTERN	2	/* caller owns this table */

#define MAC_EXP_MIN_LEN		2	/* min number of table slots */

struct MAC_EXP {
    VSTRING *result;			/* result buffer */
    const char *filter;			/* safe character list */
    int     clobber;			/* safe replacement */
    int     flags;			/* findings, features */
    int     level;			/* nesting level */
    HTABLE *record;			/* record of substitutions */
    int     len;			/* table list length */
    int     last;			/* last element used */
    struct table_info table_info[MAC_EXP_MIN_LEN];
};

/* mac_expand_callback - callback for mac_parse */

static void mac_expand_callback(int type, VSTRING *buf, char *ptr)
{
    char   *myname = "mac_expand_callback";
    MAC_EXP *mc = (MAC_EXP *) ptr;
    HTABLE_INFO *ht;
    char   *text;
    char   *cp;
    int     ch;
    int     n;

    /*
     * Sanity check.
     */
    if (mc->level++ > 100) {
	msg_warn("unreasonable macro call nesting: \"%s\"", vstring_str(buf));
	mc->flags |= MAC_EXP_FLAG_ERROR;
	return;
    }

    /*
     * $Name reference.
     */
    if (type == MAC_PARSE_VARNAME) {

	/*
	 * Look for the ? or : delimiter. In case of a syntax error, return
	 * without doing damage. We do not have enough context to produce an
	 * understandable error message, so don't try.
	 */
	for (cp = vstring_str(buf); (ch = *cp) != 0; cp++) {
	    if (ch == '?' || ch == ':') {
		*cp++ = 0;
		break;
	    }
	    if (!ISALNUM(ch) && ch != '_') {
		msg_warn("macro name syntax error: \"%s\"", vstring_str(buf));
		mc->flags |= MAC_EXP_FLAG_ERROR;
		return;
	    }
	}

	/*
	 * Look up the named parameter.
	 */
	for (text = 0, n = mc->last; n >= 0; n--) {
	    ht = htable_locate(mc->table_info[n].table, vstring_str(buf));
	    if (ht != 0) {
		text = ht->value;
		break;
	    }
	}

	/*
	 * Perform the requested substitution.
	 */
	switch (ch) {
	case '?':
	    if (text != 0)
		mac_parse(cp, mac_expand_callback, (char *) mc);
	    break;
	case ':':
	    if (text == 0)
		mac_parse(cp, mac_expand_callback, (char *) mc);
	    break;
	default:
	    if (text == 0) {
		mc->flags |= MAC_EXP_FLAG_UNDEF;
		break;
	    }
	    if (mc->filter) {
		vstring_strcpy(buf, text);
		text = vstring_str(buf);
		for (cp = text; (cp += strspn(cp, mc->filter))[0]; /* void */ )
		    *cp++ = mc->clobber;
	    }
	    if (mc->flags & MAC_EXP_FLAG_RECURSE)
		mac_parse(text, mac_expand_callback, (char *) mc);
	    else
		vstring_strcat(mc->result, text);
	    break;
	}

	/*
	 * Record keeping...
	 */
	if (mc->record) {
	    if ((ht = htable_locate(mc->record, vstring_str(buf))) == 0)
		ht = htable_enter(mc->record, vstring_str(buf), (char *) 0);
	    ht->value++;
	}
    }

    /*
     * Literal text.
     */
    else {
	text = vstring_str(buf);
	vstring_strcat(mc->result, text);
    }

    /*
     * Give the poor tester a clue of what is going on.
     */
    if (msg_verbose)
	msg_info("%s: %s = %s", myname, vstring_str(buf),
		 text ? text : "(undef)");

    mc->level--;
}

/* mac_expand_addtable - add table to expansion context */

static MAC_EXP *mac_expand_addtable(MAC_EXP *mc, HTABLE *table, int status)
{
    mc->last += 1;
    if (mc->last >= mc->len) {
	mc->len *= 2;
	mc = (MAC_EXP *) myrealloc((char *) mc, sizeof(*mc) + sizeof(mc->table_info[0]) * (mc->len - MAC_EXP_MIN_LEN));
    }
    mc->table_info[mc->last].table = table;
    mc->table_info[mc->last].status = status;
    return (mc);
}

/* mac_expand - expand $name instances */

int     mac_expand(VSTRING *result, const char *pattern, int flags, int key,...)
{
    MAC_EXP *mc;
    va_list ap;
    char   *name;
    char   *value;
    HTABLE *table;
    HTABLE_INFO *ht;
    int     status;

    /*
     * Initialize.
     */
    mc = (MAC_EXP *) mymalloc(sizeof(*mc));
    mc->result = result;
    mc->flags = (flags & MAC_EXP_FLAG_INMASK);
    mc->filter = 0;
    mc->clobber = '_';
    mc->record = 0;
    mc->level = 0;
    mc->len = MAC_EXP_MIN_LEN;
    mc->last = -1;

    /*
     * Stash away the attributes.
     */
    for (va_start(ap, key); key != 0; key = va_arg(ap, int)) {
	switch (key) {
	case MAC_EXP_ARG_ATTR:
	    name = va_arg(ap, char *);
	    value = va_arg(ap, char *);
	    if (mc->last < 0
		|| mc->table_info[mc->last].status != MAC_EXP_STAT_PRIVATE) {
		table = htable_create(0);
		mc = mac_expand_addtable(mc, table, MAC_EXP_STAT_PRIVATE);
	    } else
		table = mc->table_info[mc->last].table;
	    if ((ht = htable_locate(table, name)) == 0)
		ht = htable_enter(table, name, (char *) 0);
	    if (ht->value != 0)
		myfree(ht->value);
	    ht->value = (value ? mystrdup(value) : 0);
	    break;
	case MAC_EXP_ARG_TABLE:
	    table = va_arg(ap, HTABLE *);
	    mc = mac_expand_addtable(mc, table, MAC_EXP_STAT_EXTERN);
	    break;
	case MAC_EXP_ARG_FILTER:
	    mc->filter = va_arg(ap, char *);
	    break;
	case MAC_EXP_ARG_CLOBBER:
	    mc->clobber = va_arg(ap, int);
	    break;
	case MAC_EXP_ARG_RECORD:
	    mc->record = va_arg(ap, HTABLE *);
	    break;
	}
    }
    va_end(ap);

    /*
     * Do the substitutions.
     */
    VSTRING_RESET(result);
    mac_parse(pattern, mac_expand_callback, (char *) mc);
    VSTRING_TERMINATE(result);
    status = (mc->flags & MAC_EXP_FLAG_OUTMASK);

    /*
     * Clean up.
     */
    while (mc->last >= 0) {
	if (mc->table_info[mc->last].status == MAC_EXP_STAT_PRIVATE)
	    htable_free(mc->table_info[mc->last].table, myfree);
	mc->last--;
    }
    myfree((char *) mc);

    return (status);
}

#ifdef TEST

 /*
  * This code certainly deserves a stand-alone test program.
  */
#include <stringops.h>
#include <vstream.h>
#include <vstring_vstream.h>

int     main(int unused_argc, char **unused_argv)
{
    VSTRING *buf = vstring_alloc(100);
    VSTRING *result = vstring_alloc(100);
    char   *cp;
    char   *name;
    char   *value;
    HTABLE *table;
    int     stat;

    while (!vstream_feof(VSTREAM_IN)) {

	table = htable_create(0);

	/*
	 * Read a block of definitions, terminated with an empty line.
	 */
	while (vstring_get_nonl(buf, VSTREAM_IN) != VSTREAM_EOF) {
	    if (VSTRING_LEN(buf) == 0)
		break;
	    cp = vstring_str(buf);
	    name = mystrtok(&cp, " \t\r\n=");
	    value = mystrtok(&cp, " \t\r\n=");
	    htable_enter(table, name, value ? mystrdup(value) : 0);
	}

	/*
	 * Read a block of patterns, terminated with an empty line or EOF.
	 */
	while (vstring_get_nonl(buf, VSTREAM_IN) != VSTREAM_EOF) {
	    if (VSTRING_LEN(buf) == 0)
		break;
	    cp = vstring_str(buf);
	    VSTRING_RESET(result);
	    stat = mac_expand(result, vstring_str(buf), MAC_EXP_FLAG_NONE,
			      MAC_EXP_ARG_TABLE, table,
			      MAC_EXP_ARG_END);
	    vstream_printf("stat=%d result=%s\n", stat, vstring_str(result));
	    vstream_fflush(VSTREAM_OUT);
	}
	htable_free(table, myfree);
	vstream_printf("\n");
    }

    /*
     * Clean up.
     */
    vstring_free(buf);
    vstring_free(result);
    exit(0);
}

#endif
