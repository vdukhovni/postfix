/*++
/* NAME
/*	dict_regexp 3
/* SUMMARY
/*	dictionary manager interface to REGEXP regular expression library
/* SYNOPSIS
/*	#include <dict_regexp.h>
/*
/*	DICT	*dict_regexp_open(name, dummy, dict_flags)
/*	const char *name;
/*	int	dummy;
/*	int	dict_flags;
/* DESCRIPTION
/*	dict_regexp_open() opens the named file and compiles the contained
/*	regular expressions. The result object can be used to match strings
/*	against the table.
/* SEE ALSO
/*	dict(3) generic dictionary manager
/* AUTHOR(S)
/*	LaMont Jones
/*	lamont@hp.com
/*
/*	Based on PCRE dictionary contributed by Andrew McNamara
/*	andrewm@connect.com.au
/*	connect.com.au Pty. Ltd.
/*	Level 3, 213 Miller St
/*	North Sydney, NSW, Australia
/*
/*	Heavily rewritten by Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include "sys_defs.h"

#ifdef HAS_POSIX_REGEXP

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>

/* Utility library. */

#include "mymalloc.h"
#include "msg.h"
#include "safe.h"
#include "vstream.h"
#include "vstring.h"
#include "stringops.h"
#include "readlline.h"
#include "dict.h"
#include "dict_regexp.h"
#include "mac_parse.h"

 /*
  * Regular expression before compiling.
  */
typedef struct {
    char   *regexp;			/* regular expression */
    int     options;			/* regcomp() options */
} DICT_REGEXP_PATTERN;

 /*
  * Compiled regexp rule with replacement text.
  */
typedef struct dict_regexp_list {
    struct dict_regexp_list *next;	/* next regexp in dict */
    regex_t *primary_exp;		/* compiled primary pattern */
    regex_t *negated_exp;		/* compiled negated pattern */
    char   *replacement;		/* replacement text */
    size_t  max_nsub;			/* largest replacement $number */
    int     lineno;			/* source file line number */
} DICT_REGEXP_RULE;

 /*
  * Regexp map.
  */
typedef struct {
    DICT    dict;			/* generic members */
    regmatch_t *pmatch;			/* replacement substring storage */
    DICT_REGEXP_RULE *head;		/* first rule */
} DICT_REGEXP;

 /*
  * Context for $number expansion callback.
  */
typedef struct {
    DICT_REGEXP *dict;			/* the dictionary entry */
    DICT_REGEXP_RULE *rule;		/* the rule we matched */
    VSTRING *buf;			/* target string buffer */
    const char *subject;		/* matched text */
} DICT_REGEXP_EXPAND_CONTEXT;

 /*
  * Context for $number pre-scan callback.
  */
typedef struct {
    const char *map;			/* name of regexp map */
    int     lineno;			/* where in file */
    size_t  max_nsub;			/* largest $number seen */
} DICT_REGEXP_PRESCAN_CONTEXT;

 /*
  * Compatibility.
  */
#ifndef MAC_PARSE_OK
#define MAC_PARSE_OK 0
#endif

/* dict_regexp_expand - replace $number with substring from matched text */

static int dict_regexp_expand(int type, VSTRING *buf, char *ptr)
{
    DICT_REGEXP_EXPAND_CONTEXT *ctxt = (DICT_REGEXP_EXPAND_CONTEXT *) ptr;
    DICT_REGEXP_RULE *rule = ctxt->rule;
    DICT_REGEXP *dict = ctxt->dict;
    size_t  n;

    /*
     * Replace $number by the corresponding substring from the matched text.
     * We pre-scanned the replacement text at compile time, so any out of
     * range $number means that something impossible has happened.
     */
    if (type == MAC_PARSE_VARNAME) {
	n = atoi(vstring_str(buf));
	if (n < 1 || n > rule->max_nsub)
	    msg_panic("regexp map %s, line %d: out of range replacement index \"%s\"",
		      dict->dict.name, rule->lineno, vstring_str(buf));
	if (dict->pmatch[n].rm_so < 0 ||
	    dict->pmatch[n].rm_so == dict->pmatch[n].rm_eo) {
	    return (MAC_PARSE_UNDEF);		/* empty or not matched */
	}
	vstring_strncat(ctxt->buf, ctxt->subject + dict->pmatch[n].rm_so,
			dict->pmatch[n].rm_eo - dict->pmatch[n].rm_so);
    }

    /*
     * Straight text - duplicate with no substitution.
     */
    else
	vstring_strcat(ctxt->buf, vstring_str(buf));

    return (MAC_PARSE_OK);
}

/* dict_regexp_regerror - report regexp compile/execute error */

static void dict_regexp_regerror(const char *map, int lineno, int error,
				         const regex_t * expr)
{
    char    errbuf[256];

    (void) regerror(error, expr, errbuf, sizeof(errbuf));
    msg_warn("regexp map %s, line %d: %s", map, lineno, errbuf);
}

/* dict_regexp_lookup - match string and perform substitution */

static const char *dict_regexp_lookup(DICT *dict, const char *name)
{
    DICT_REGEXP *dict_regexp = (DICT_REGEXP *) dict;
    DICT_REGEXP_RULE *rule;
    DICT_REGEXP_EXPAND_CONTEXT ctxt;
    static VSTRING *buf;
    int     error;

    dict_errno = 0;

    if (msg_verbose)
	msg_info("dict_regexp_lookup: %s: %s", dict_regexp->dict.name, name);

    /*
     * Search for the first matching primary expression. Limit the overhead
     * for substring substitution to the bare minimum.
     */
    for (rule = dict_regexp->head; rule; rule = rule->next) {
	error = regexec(rule->primary_exp, name, rule->max_nsub + 1,
			rule->max_nsub ? dict_regexp->pmatch :
			(regmatch_t *) 0, 0);
	switch (error) {
	case REG_NOMATCH:
	    continue;
	default:
	    dict_regexp_regerror(dict_regexp->dict.name, rule->lineno,
				 error, rule->primary_exp);
	    continue;
	case 0:
	    break;
	}

	/*
	 * Primary expression match found. Require a negative match on the
	 * optional negated expression. In this case we're never going to do
	 * any string substitution.
	 */
	if (rule->negated_exp) {
	    error = regexec(rule->negated_exp, name, 0, (regmatch_t *) 0, 0);
	    switch (error) {
	    case 0:
		continue;
	    default:
		dict_regexp_regerror(dict_regexp->dict.name, rule->lineno,
				     error, rule->negated_exp);
		continue;
	    case REG_NOMATCH:
		break;
	    }
	}

	/*
	 * Match found. Skip $number substitutions when the replacement text
	 * contains no $number strings (as learned during the pre-scan).
	 */
	if (rule->max_nsub == 0)
	    return (rule->replacement);

	/*
	 * Perform $number substitutions on the replacement text. We
	 * pre-scanned the replacement text at compile time. Any macro
	 * expansion errors at this point mean something impossible has
	 * happened.
	 */
	if (!buf)
	    buf = vstring_alloc(10);
	VSTRING_RESET(buf);
	ctxt.buf = buf;
	ctxt.subject = name;
	ctxt.rule = rule;
	ctxt.dict = dict_regexp;

	if (mac_parse(rule->replacement, dict_regexp_expand, (char *) &ctxt) & MAC_PARSE_ERROR)
	    msg_panic("regexp map %s, line %d: bad replacement syntax",
		      dict_regexp->dict.name, rule->lineno);
	VSTRING_TERMINATE(buf);
	return (vstring_str(buf));
    }
    return (0);
}

/* dict_regexp_close - close regexp dictionary */

static void dict_regexp_close(DICT *dict)
{
    DICT_REGEXP *dict_regexp = (DICT_REGEXP *) dict;
    DICT_REGEXP_RULE *rule;
    DICT_REGEXP_RULE *next;

    for (rule = dict_regexp->head; rule; rule = next) {
	next = rule->next;
	if (rule->primary_exp) {
	    regfree(rule->primary_exp);
	    myfree((char *) rule->primary_exp);
	}
	if (rule->negated_exp) {
	    regfree(rule->negated_exp);
	    myfree((char *) rule->negated_exp);
	}
	myfree((char *) rule->replacement);
	myfree((char *) rule);
    }
    if (dict_regexp->pmatch)
	myfree((char *) dict_regexp->pmatch);
    dict_free(dict);
}

/* dict_regexp_get_pattern - extract one pattern with options from rule */

static int dict_regexp_get_pattern(const char *map, int lineno, char **bufp,
				           DICT_REGEXP_PATTERN *pat)
{
    char   *p = *bufp;
    char    re_delim;

    re_delim = *p++;
    pat->regexp = p;

    /*
     * Search for the closing delimiter, handling backslash escape.
     */
    while (*p) {
	if (*p == '\\') {
	    if (p[1])
		p++;
	    else
		break;
	} else if (*p == re_delim) {
	    break;
	}
	++p;
    }
    if (!*p) {
	msg_warn("regexp map %s, line %d: no closing regexp delimiter \"%c\": "
		 "skipping this rule", map, lineno, re_delim);
	return (0);
    }
    *p++ = 0;					/* null terminate */

    /*
     * Search for options.
     */
    pat->options = REG_EXTENDED | REG_ICASE;
    while (*p && !ISSPACE(*p) && *p != '!') {
	switch (*p) {
	case 'i':
	    pat->options ^= REG_ICASE;
	    break;
	case 'm':
	    pat->options ^= REG_NEWLINE;
	    break;
	case 'x':
	    pat->options ^= REG_EXTENDED;
	    break;
	default:
	    msg_warn("regexp map %s, line %d: unknown regexp option \"%c\": "
		     "skipping this rule", map, lineno, *p);
	    return (0);
	}
	++p;
    }
    *bufp = p;
    return (1);
}

/* dict_regexp_compile - compile one pattern */

static regex_t *dict_regexp_compile(const char *map, int lineno,
				            DICT_REGEXP_PATTERN *pat)
{
    int     error;
    regex_t *expr;

    expr = (regex_t *) mymalloc(sizeof(*expr));
    error = regcomp(expr, pat->regexp, pat->options);
    if (error != 0) {
	dict_regexp_regerror(map, lineno, error, expr);
	myfree((char *) expr);
	return (0);
    }
    return (expr);
}

/* dict_regexp_prescan - find largest $number in replacement text */

static int dict_regexp_prescan(int type, VSTRING *buf, char *context)
{
    DICT_REGEXP_PRESCAN_CONTEXT *ctxt = (DICT_REGEXP_PRESCAN_CONTEXT *) context;
    size_t  n;

    if (type == MAC_PARSE_VARNAME) {
	if (!alldig(vstring_str(buf))) {
	    msg_warn("regexp map %s, line %d: non-numeric replacement macro name \"%s\"",
		     ctxt->map, ctxt->lineno, vstring_str(buf));
	    return (MAC_PARSE_ERROR);
	}
	n = atoi(vstring_str(buf));
	if (n > ctxt->max_nsub)
	    ctxt->max_nsub = n;
    }
    return (MAC_PARSE_OK);
}

/* dict_regexp_parseline - parse one rule */

static DICT_REGEXP_RULE *dict_regexp_parseline(const char *map, int lineno,
					               char *line)
{
    DICT_REGEXP_RULE *rule;
    char   *p;
    regex_t *primary_exp;
    regex_t *negated_exp;
    DICT_REGEXP_PATTERN primary_pat;
    DICT_REGEXP_PATTERN negated_pat;
    DICT_REGEXP_PRESCAN_CONTEXT ctxt;

    p = line;

    /*
     * Get the primary and optional negated patterns and their flags.
     */
    if (dict_regexp_get_pattern(map, lineno, &p, &primary_pat) == 0)
	return (0);
    if (*p == '!' && p[1] && !ISSPACE(p[1])) {
	p++;
	if (dict_regexp_get_pattern(map, lineno, &p, &negated_pat) == 0)
	    return (0);
    } else {
	negated_pat.regexp = 0;
    }

    /*
     * Get the replacement text.
     */
    if (!ISSPACE(*p)) {
	msg_warn("regexp map %s, line %d: invalid expression: "
		 "skipping this rule", map, lineno);
	return (0);
    }
    while (*p && ISSPACE(*p))
	++p;
    if (!*p) {
	msg_warn("regexp map %s, line %d: using empty replacement string",
		 map, lineno);
    }

    /*
     * Find the highest-numbered $number substitution string. We can speed up
     * processing 1) by passing hints to the regexp compiler, setting the
     * REG_NOSUB flag when the replacement text contains no $number string;
     * 2) by passing hints to the regexp execution code, limiting the amount
     * of text that is made available for substitution.
     */
    ctxt.map = map;
    ctxt.lineno = lineno;
    ctxt.max_nsub = 0;
    if (mac_parse(p, dict_regexp_prescan, (char *) &ctxt) & MAC_PARSE_ERROR) {
	msg_warn("regexp map %s, line %d: bad replacement syntax: "
		 "skipping this rule", map, lineno);
	return (0);
    }

    /*
     * Compile the primary and the optional negated pattern. Speed up
     * execution when no matched text needs to be substituted into the result
     * string, or when the highest numbered substring is less than the total
     * number of () subpatterns.
     */
    if (ctxt.max_nsub == 0)
	primary_pat.options |= REG_NOSUB;
    if ((primary_exp = dict_regexp_compile(map, lineno, &primary_pat)) == 0)
	return (0);
    if (ctxt.max_nsub > primary_exp->re_nsub) {
	msg_warn("regexp map %s, line %d: out of range replacement index \"%d\": "
		 "skipping this rule", map, lineno, ctxt.max_nsub);
	regfree(primary_exp);
	myfree((char *) primary_exp);
	return (0);
    }
    if (negated_pat.regexp != 0) {
	negated_pat.options |= REG_NOSUB;
	if ((negated_exp = dict_regexp_compile(map, lineno, &negated_pat)) == 0) {
	    regfree(primary_exp);
	    myfree((char *) primary_exp);
	    return (0);
	}
    } else
	negated_exp = 0;

    /*
     * Package up the result.
     */
    rule = (DICT_REGEXP_RULE *) mymalloc(sizeof(DICT_REGEXP_RULE));
    rule->primary_exp = primary_exp;
    rule->negated_exp = negated_exp;
    rule->replacement = mystrdup(p);
    rule->max_nsub = ctxt.max_nsub;
    rule->lineno = lineno;
    rule->next = 0;
    return (rule);
}

/* dict_regexp_open - load and compile a file containing regular expressions */

DICT   *dict_regexp_open(const char *map, int unused_flags, int dict_flags)
{
    DICT_REGEXP *dict_regexp;
    VSTREAM *map_fp;
    VSTRING *line_buffer;
    DICT_REGEXP_RULE *rule;
    DICT_REGEXP_RULE *last_rule = 0;
    int     lineno = 0;
    size_t  max_nsub = 0;
    char   *p;

    line_buffer = vstring_alloc(100);

    dict_regexp = (DICT_REGEXP *) dict_alloc(DICT_TYPE_REGEXP, map,
					     sizeof(*dict_regexp));
    dict_regexp->dict.lookup = dict_regexp_lookup;
    dict_regexp->dict.close = dict_regexp_close;
    dict_regexp->dict.flags = dict_flags | DICT_FLAG_PATTERN;
    dict_regexp->head = 0;
    dict_regexp->pmatch = 0;

    /*
     * Parse the regexp table.
     */
    if ((map_fp = vstream_fopen(map, O_RDONLY, 0)) == 0)
	msg_fatal("open %s: %m", map);

    while (readlline(line_buffer, map_fp, &lineno)) {
	p = vstring_str(line_buffer);
	trimblanks(p, 0)[0] = 0;
	rule = dict_regexp_parseline(map, lineno, p);
	if (rule) {
	    if (rule->max_nsub > max_nsub)
		max_nsub = rule->max_nsub;

	    if (last_rule == 0)
		dict_regexp->head = rule;
	    else
		last_rule->next = rule;
	    last_rule = rule;
	}
    }

    /*
     * Allocate space for only as many matched substrings as used in the
     * replacement text.
     */
    if (max_nsub > 0)
	dict_regexp->pmatch =
	    (regmatch_t *) mymalloc(sizeof(regmatch_t) * (max_nsub + 1));

    /*
     * Clean up.
     */
    vstring_free(line_buffer);
    vstream_fclose(map_fp);

    return (DICT_DEBUG (&dict_regexp->dict));
}

#endif
