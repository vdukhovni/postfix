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
/*	mac_expand() expands $name instances in \fBpattern\fR
/*	and stores the result into \fBresult\fR.
/*
/*	The following expansions are done:
/* .IP "$name, ${name}, $(name)"
/*	The result is the value of the named parameter. Optionally, the
/*	result is subjected to further expansions.
/* .IP "${name?text}, $(name?text)"
/*	If the named parameter is defined, the result is the given text,
/*	after another iteration of $name expansion. Otherwise, the result is
/*	empty.
/* .IP "${name:text}, $(name:text)"
/*	If the named parameter is undefined, the result is the given text,
/*	after another iteration of $name expansion. Otherwise, the result is
/*	empty.
/* .PP
/*	Arguments:
/* .IP result
/*	Storage for the result of expansion. The result is not truncated
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
/*	Attributes are defined left to right; only the last definition
/*	of an attribute is remembered.
/*	The following keys are understood (types of arguments indicated
/*	in parentheses):
/* .RS
/* .IP "MAC_EXP_ARG_ATTR (char *, char *)"
/*	The next two arguments specify an attribute name and its attribute
/*	string value.  Specify a null string value for an attribute that is
/*	known but unset.
/* .IP "MAC_EXP_ARG_TABLE (HTABLE *)"
/*	The next argument is a hash table with attribute names and values.
/*	Specify a null string value for an attribute that is known but unset.
/* .IP "MAC_EXP_ARG_FILTER (char *)"
/*	The next argument specifies a null-terminated list of characters
/*	that are allowed to appear in $name expansions. By default, illegal
/*	characters are replaced by underscore. Only the last specified
/*	filter takes effect.
/* .IP "MAC_EXP_ARG_CLOBBER (int)"
/*	Character value to be used when the result of expansion is not
/*	allowed according to the MAC_EXP_ARG_FILTER argument. Only the
/*	last specified replacement value takes effect.
/* .RE
/* .IP MAC_EXP_ARG_END
/*	A manifest constant that indicates the end of the argument list.
/* DIAGNOSTICS
/*	Fatal errors: out of memory.  Warnings: syntax errors, unreasonable
/*	macro nesting.
/*
/*	The result value is the binary OR of zero or more of the following:
/* .IP MAC_EXP_FLAG_UNDEF
/*	The pattern contains a reference to an unknown parameter or to
/*	a parameter whose value is not defined.
/*	A zero-length string was used as replacement.
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
#include <setjmp.h>
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
  * Little helper structure.
  */
typedef struct {
    HTABLE *table;			/* private symbol table */
    VSTRING *result;			/* result buffer */
    const char *filter;			/* safe character list */
    int     clobber;			/* safe replacement */
    int     flags;			/* findings, features */
    int     level;			/* nesting level */
    jmp_buf jbuf;			/* escape */
} MAC_EXP_CONTEXT;

/* mac_expand_callback - callback for mac_parse */

static void mac_expand_callback(int type, VSTRING *buf, char *ptr)
{
    char   *myname = "mac_expand_callback";
    MAC_EXP_CONTEXT *context = (MAC_EXP_CONTEXT *) ptr;
    HTABLE_INFO *ht;
    char   *text;
    char   *cp;
    int     ch;

    if (context->level++ > 100) {
	msg_warn("unreasonable macro call nesting: \"%s\"", vstring_str(buf));
	longjmp(context->jbuf, 1);
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
		longjmp(context->jbuf, 1);
	    }
	}

	/*
	 * Look up the named parameter.
	 */
	text = (ht = htable_locate(context->table, vstring_str(buf))) == 0 ?
	    0 : ht->value;

	/*
	 * Perform the requested substitution.
	 */
	switch (ch) {
	case '?':
	    if (text != 0)
		mac_parse(cp, mac_expand_callback, (char *) context);
	    break;
	case ':':
	    if (text == 0)
		mac_parse(cp, mac_expand_callback, (char *) context);
	    break;
	default:
	    if (text == 0) {
		context->flags |= MAC_EXP_FLAG_UNDEF;
		break;
	    }
	    if (context->filter) {
		vstring_strcpy(buf, text);
		text = vstring_str(buf);
		for (cp = text; (cp += strspn(cp, context->filter))[0];)
		    *cp++ = context->clobber;
	    }
	    if (context->flags & MAC_EXP_FLAG_RECURSE)
		mac_parse(text, mac_expand_callback, (char *) context);
	    else
		vstring_strcat(context->result, text);
	    break;
	}
    }

    /*
     * Literal text.
     */
    else {
	text = vstring_str(buf);
	vstring_strcat(context->result, text);
    }

    /*
     * Give the poor tester a clue of what is going on.
     */
    if (msg_verbose)
	msg_info("%s: %s = %s", myname, vstring_str(buf),
		 text ? text : "(undef)");

    context->level--;
}

/* mac_expand - expand $name instances */

int     mac_expand(VSTRING *result, const char *pattern, int flags, int key,...)
{
    MAC_EXP_CONTEXT context;
    va_list ap;
    HTABLE_INFO **ht_info;
    HTABLE_INFO **ht;
    HTABLE *table;
    char   *name;
    char   *value;

#define HTABLE_CLOBBER(t, n, v) do { \
	HTABLE_INFO *_ht; \
	if ((_ht = htable_locate(t, n)) != 0) \
	    _ht->value = v; \
	else \
	    htable_enter(t, n, v); \
    } while(0);

    /*
     * Inititalize.
     */
    context.table = htable_create(0);
    context.result = result;
    context.flags = flags;
    context.filter = 0;
    context.clobber = '_';
    context.level = 0;

    /*
     * Stash away the attributes.
     */
    for (va_start(ap, key); key != 0; key = va_arg(ap, int)) {
	switch (key) {
	case MAC_EXP_ARG_ATTR:
	    name = va_arg(ap, char *);
	    value = va_arg(ap, char *);
	    HTABLE_CLOBBER(context.table, name, value);
	    break;
	case MAC_EXP_ARG_TABLE:
	    table = va_arg(ap, HTABLE *);
	    ht_info = htable_list(table);
	    for (ht = ht_info; *ht; ht++)
		HTABLE_CLOBBER(context.table, ht[0]->key, ht[0]->value);
	    myfree((char *) ht_info);
	    break;
	case MAC_EXP_ARG_FILTER:
	    context.filter = va_arg(ap, char *);
	    break;
	case MAC_EXP_ARG_CLOBBER:
	    context.clobber = va_arg(ap, int);
	    break;
	}
    }
    va_end(ap);

    /*
     * Do the substitutions.
     */
    if (setjmp(context.jbuf) == 0)
	mac_parse(pattern, mac_expand_callback, (char *) &context);
    VSTRING_TERMINATE(result);

    /*
     * Clean up.
     */
    htable_free(context.table, (void (*) (char *)) 0);
    return (context.flags & MAC_EXP_FLAG_UNDEF);
}

#ifdef TEST

 /*
  * This code certainly deserves a stand-alone test program.
  */
#include <stringops.h>
#include <vstream.h>
#include <vstring_vstream.h>

static void nfree(char *ptr)
{
    if (ptr)
	myfree(ptr);
}

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
	htable_free(table, nfree);
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
