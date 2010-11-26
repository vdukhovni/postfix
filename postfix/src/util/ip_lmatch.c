/*++
/* NAME
/*	ip_lmatch 3
/* SUMMARY
/*	lazy IP address pattern matching
/* SYNOPSIS
/*	#include <ip_lmatch.h>
/*
/*	int	ip_lmatch(pattern, addr, why)
/*	char	*pattern;
/*	const char *addr;
/*	VSTRING	**why;
/* DESCRIPTION
/*	This module supports IP address pattern matching. See below
/*	for a description of the supported address pattern syntax.
/*
/*	This version optimizes for implementation convenience.  The
/*	lazy parser stops as soon as the address does not match the
/*	pattern. This results in a poor user interface: a pattern
/*	syntax error at the end will be reported ONLY when an address
/*	matches the entire pattern before the syntax error.
/*
/*	Use the ip_match() module for an implementation that has
/*	separate parsing and matching stages. That implementation
/*	reports a syntax error immediately, and provides faster
/*	matching at the cost of a more complex programming interface.
/*
/*	ip_lmatch_parse() matches the address bytes while parsing
/*	the pattern, and terminates as soon as a non-match or syntax
/*	error is found.  The result is -1 in case of syntax error,
/*	0 in case of no match, 1 in case of a match.
/*
/*	Arguments
/* .IP addr
/*	Network address in printable form.
/* .IP pattern
/*	Address pattern. This argument may be modified.
/* .IP why
/*	Pointer to storage for error reports (result value -1). If
/*	the target is a null pointer, ip_lmatch() will allocate a
/*	buffer that should be freed by the application.
/* IPV4 PATTERN SYNTAX
/* .ad
/* .fi
/*	An IPv4 address pattern has four fields separated by ".".
/*	Each field is either a decimal number, or a sequence inside
/*	"[]" that contains one or more comma-separated decimal
/*	numbers or number..number ranges.
/*
/*	Examples of patterns are 1.2.3.4 (matches itself, as one
/*	would expect) and 1.2.3.[2,4,6..8] (matches 1.2.3.2, 1.2.3.4,
/*	1.2.3.6, 1.2.3.7, 1.2.3.8).
/*
/*	Thus, any pattern field can be a sequence inside "[]", but
/*	a "[]" sequence cannot span multiple address fields, and
/*	a pattern field cannot contain both a number and a "[]"
/*	sequence at the same time.
/*
/*	This means that the pattern 1.2.[3.4] is not valid (the
/*	sequence [3.4] cannot span two address fields) and the
/*	pattern 1.2.3.3[6..9] is also not valid (the last field
/*	cannot be both number 3 and sequence [6..9] at the same
/*	time).
/*
/*	The syntax for IPv4 patterns is as follows:
/*
/* .in +5
/*	v4pattern = v4field "." v4field "." v4field "." v4field
/* .br
/*	v4field = v4octet | "[" v4sequence "]
/* .br
/*	v4octet = any decimal number in the range 0 through 255
/* .br
/*	v4sequence = v4seq_member | v4sequence "," v4seq_member
/* .br
/*	v4seq_member = v4octet | v4octet ".." v4octet
/* .in
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this
/*	software.
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
#include <ctype.h>
#include <string.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <vstring.h>
#include <myaddrinfo.h>
#include <ip_lmatch.h>

 /*
  * Token values.
  */
#define IP_LMATCH_CODE_OPEN	'['	/* start of set */
#define IP_LMATCH_CODE_CLOSE	']'	/* end of set */
#define IP_LMATCH_CODE_OVAL	'N'	/* octet value */
#define IP_LMATCH_CODE_EOF	'\0'	/* oops */
#define IP_LMATCH_CODE_ERR	256	/* oops */

 /*
  * Address length is protocol dependent. Find out how large our address byte
  * strings should be.
  */
#ifdef HAS_IPV6
#define IP_LMATCH_ABYTES	MAI_V6ADDR_BYTES
#else
#define IP_LMATCH_ABYTES	MAI_V4ADDR_BYTES
#endif

 /*
  * SLMs.
  */
#define STR	vstring_str
#define LEN	VSTRING_LEN

/* ip_lmatch_next_token - carve out the next token from user input */

static int ip_lmatch_next_token(char **pstart, char **psaved_start, int *poval)
{
    unsigned char *cp;
    unsigned char *next;
    int     oval;

    /*
     * Return a value-less token (i.e. a literal, error, or EOF.
     */
#define IP_LMATCH_RETURN_TOK(next, type) \
    do { *pstart = (char *) (next); return (type); } while (0)

    /*
     * Return a token that contains an IPv4 address octet value.
     */
#define IP_LMATCH_RETURN_TOK_OVAL(next, oval) do { \
	*poval = (oval); IP_LMATCH_RETURN_TOK((next), IP_LMATCH_CODE_OVAL); \
    } while (0)

    /*
     * Light-weight tokenizer. Each result is an IPv4 address octet value, a
     * literal character value, error, or EOF.
     */
    *psaved_start = *pstart;
    cp = (unsigned char *) *pstart;
    if (ISDIGIT(*cp)) {
	oval = *cp - '0';
	for (next = cp + 1; ISDIGIT(*next); next++) {
	    oval *= 10;
	    oval += *next - '0';
	    if (oval > 255)
		IP_LMATCH_RETURN_TOK(next + 1, IP_LMATCH_CODE_ERR);
	}
	IP_LMATCH_RETURN_TOK_OVAL(next, oval);
    } else {
	IP_LMATCH_RETURN_TOK(*cp ? cp + 1 : cp, *cp);
    }
}

/* ip_lmatch_print_parse_error - report parsing error in context */

static void PRINTFLIKE(5, 6) ip_lmatch_print_parse_error(VSTRING **why,
							         char *start,
							         char *here,
							         char *next,
						        const char *fmt,...)
{
    va_list ap;
    int     start_width;
    int     here_width;

    /*
     * On-the-fly allocation.
     */
    if (*why == 0)
	*why = vstring_alloc(20);

    /*
     * Format the error type.
     */
    va_start(ap, fmt);
    vstring_vsprintf(*why, fmt, ap);
    va_end(ap);

    /*
     * Format the error context. The syntax is complex enough that it is
     * worth the effort to precisely indicate what input is in error.
     * 
     * XXX Workaround for %.*s to avoid output when a zero width is specified.
     */
#define IP_LMATCH_NO_ERROR_CONTEXT (char *) 0, (char *) 0, (char *) 0

    if (start != 0) {
	start_width = here - start;
	here_width = next - here;
	vstring_sprintf_append(*why, " at \"%.*s>%.*s<%s\"",
			       start_width, start_width == 0 ? "" : start,
			     here_width, here_width == 0 ? "" : here, next);
    }
}

/* ip_lmatch - match an address pattern */

int     ip_lmatch(char *pattern, const char *addr, VSTRING **why)
{
    const char *myname = "ip_lmatch";
    char    addr_bytes[IP_LMATCH_ABYTES];
    const unsigned char *ap;
    int     octet_count;
    char   *saved_cp;
    char   *cp;
    int     token_type;
    int     look_ahead;
    int     oval;
    int     saved_oval;
    int     matched;

    /*
     * For now, IPv4 support only. Use different parser loops for IPv4 and
     * IPv6.
     */
    switch (inet_pton(AF_INET, addr, addr_bytes)) {
    case -1:
	msg_fatal("%s: address conversion error: %m", myname);
    case 0:
	msg_warn("%s: unexpected address form: %s", myname, addr);
	return (0);
    }

    /*
     * Simplify this if we change to {} for "octet set" notation.
     */
#define FIND_TERMINATOR(start, cp) do { \
	int _level = 1; \
	for (cp = (start) ; *cp; cp++) { \
	    if (*cp == '[') _level++; \
	    if (*cp != ']') continue; \
	    if (--_level == 0) break; \
	} \
    } while (0)

    /*
     * Strip [] around the entire pattern.
     */
    if (*pattern == '[') {
	FIND_TERMINATOR(pattern, cp);
	if (cp[0] == 0) {
	    ip_lmatch_print_parse_error(why, IP_LMATCH_NO_ERROR_CONTEXT,
					"missing \"]\" character");
	    return (-1);
	}
	if (cp[1] == 0) {
	    *cp = 0;
	    pattern += 1;
	}
    }

    /*
     * Sanity check. In this case we can't show any error context.
     */
    if (*pattern == 0) {
	ip_lmatch_print_parse_error(why, IP_LMATCH_NO_ERROR_CONTEXT,
				    "empty address pattern");
	return (-1);
    }

    /*
     * Simple on-the-fly pattern matching.
     */
    octet_count = 0;
    cp = pattern;

    /*
     * Require four address fields separated by ".", each field containing a
     * numeric octet value or a sequence inside []. The loop head has no test
     * and does not step the loop variable. The tokenizer advances the loop
     * variable, and the loop termination logic is inside the loop.
     */
    for (ap = (const unsigned char *) addr_bytes; /* void */ ; ap++) {
	switch (token_type = ip_lmatch_next_token(&cp, &saved_cp, &oval)) {

	    /*
	     * Numeric address field.
	     */
	case IP_LMATCH_CODE_OVAL:
	    if (*ap == oval)
		break;
	    return (0);

	    /*
	     * Wild-card address field.
	     */
	case IP_LMATCH_CODE_OPEN:
	    matched = 0;
	    /* Require comma-separated numbers or numeric ranges. */
	    for (;;) {
		token_type = ip_lmatch_next_token(&cp, &saved_cp, &oval);
		if (token_type == IP_LMATCH_CODE_OVAL) {
		    saved_oval = oval;
		    look_ahead = ip_lmatch_next_token(&cp, &saved_cp, &oval);
		    /* Numeric range. */
		    if (look_ahead == '.') {
			/* Brute-force parsing. */
			if (ip_lmatch_next_token(&cp, &saved_cp, &oval) == '.'
			    && ip_lmatch_next_token(&cp, &saved_cp, &oval)
			    == IP_LMATCH_CODE_OVAL
			    && saved_oval <= oval) {
			    if (!matched)
				matched = (*ap >= saved_oval && *ap <= oval);
			    look_ahead =
				ip_lmatch_next_token(&cp, &saved_cp, &oval);
			} else {
			    ip_lmatch_print_parse_error(why, pattern,
							saved_cp, cp,
						     "numeric range error");
			    return (-1);
			}
		    }
		    /* Single number. */
		    else {
			if (!matched)
			    matched = (*ap == oval);
		    }
		    /* Require "," or end-of-wildcard. */
		    token_type = look_ahead;
		    if (token_type == ',') {
			continue;
		    } else if (token_type == IP_LMATCH_CODE_CLOSE) {
			break;
		    } else {
			ip_lmatch_print_parse_error(why, pattern, saved_cp, cp,
						    "need \",\" or \"%c\"",
						    IP_LMATCH_CODE_CLOSE);
			return (-1);
		    }
		} else {
		    ip_lmatch_print_parse_error(why, pattern, saved_cp, cp,
					      "need decimal number 0..255");
		    return (-1);
		}
	    }
	    if (matched == 0)
		return (0);
	    break;

	    /*
	     * Invalid field.
	     */
	default:
	    ip_lmatch_print_parse_error(why, pattern, saved_cp, cp,
				     "need decimal number 0..255 or \"%c\"",
					IP_LMATCH_CODE_OPEN);
	    return (-1);
	}
	octet_count += 1;

	/*
	 * Require four address fields. Not one more, not one less.
	 */
	if (octet_count == 4) {
	    if (*cp != 0) {
		(void) ip_lmatch_next_token(&cp, &saved_cp, &oval);
		ip_lmatch_print_parse_error(why, pattern, saved_cp, cp,
					    "garbage after pattern");
		return (-1);
	    }
	    return (1);
	}

	/*
	 * Require "." before the next address field.
	 */
	if (ip_lmatch_next_token(&cp, &saved_cp, &oval) != '.') {
	    ip_lmatch_print_parse_error(why, pattern, saved_cp, cp,
					"need \".\"");
	    return (-1);
	}
    }
}

#ifdef TEST

 /*
  * Dummy main program for regression tests.
  */
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <vstream.h>
#include <vstring_vstream.h>
#include <stringops.h>

int     main(int argc, char **argv)
{
    VSTRING *why = vstring_alloc(100);
    VSTRING *line_buf = vstring_alloc(100);
    char   *bufp;
    char   *user_pattern;
    char   *user_address;
    int     echo_input = !isatty(0);
    int     match_status;

    /*
     * Iterate over the input stream. The input format is a pattern, followed
     * by addresses to match against.
     */
    while (vstring_fgets_nonl(line_buf, VSTREAM_IN)) {
	bufp = STR(line_buf);
	if (echo_input) {
	    vstream_printf("> %s\n", bufp);
	    vstream_fflush(VSTREAM_OUT);
	}
	if (*bufp == '#')
	    continue;
	if ((user_pattern = mystrtok(&bufp, " \t")) == 0)
	    continue;

	/*
	 * Match the patterns.
	 */
	while ((user_address = mystrtok(&bufp, " \t")) != 0) {
	    match_status = ip_lmatch(user_pattern, user_address, &why);
	    if (match_status < 0) {
		vstream_printf("Error: %s\n", STR(why));
	    } else {
		vstream_printf("Match %s: %s\n", user_address,
			       match_status ? "yes" : "no");
	    }
	    vstream_fflush(VSTREAM_OUT);
	}
    }
    vstring_free(line_buf);
    vstring_free(why);
    exit(0);
}

#endif
