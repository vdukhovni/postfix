/*++
/* NAME
/*	smtpd_token 3
/* SUMMARY
/*	tokenize SMTPD command
/* SYNOPSIS
/*	#include <smtpd_token.h>
/*
/*	typedef struct {
	    int	tokval;
	    char *strval;
/*	} SMTPD_TOKEN;
/*
/*	int	smtpd_token(str, argvp)
/*	char	*str;
/*	SMTPD_TOKEN **argvp;
/* DESCRIPTION
/*	smtpd_token() routine converts the string in \fIstr\fR to an
/*	array of tokens in \fIargvp\fR. The number of tokens is returned
/*	via the function return value.
/*
/*	Token types:
/* .IP SMTPD_TOK_ADDR
/*	The token is of the form <text>, not including the angle brackets.
/* .IP SMTPD_TOK_OTHER
/*	The token is something else.
/* .IP SMTPD_TOK_ERROR
/*	A malformed token.
/* BUGS
/*	This tokenizer understands just enough to tokenize SMTPD commands.
/*	It understands backslash escapes, white space, quoted strings,
/*	and addresses (including quoted text) enclosed by < and >. Any
/*	other sequence of characters is lumped together as one token.
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

#include <mymalloc.h>
#include <mvect.h>

/* Application-specific. */

#include "smtpd_token.h"

 /*
  * Macros to make complex code more readable.
  */
#define COLLECT(cp,vp,c,cond) { \
	while ((c = *cp) != 0) { \
	    if (c == '\\') { \
		cp++; \
		if ((c = *cp) == 0) \
		    break; \
	    } else if (!(cond)) { \
		break; \
	    } \
	    cp++; \
	    VSTRING_ADDCH(vp, c); \
	} \
    }

/* smtp_quoted - read until closing quote */

static char *smtp_quoted(char *cp, SMTPD_TOKEN *arg, int last)
{
    int     c;

    while ((c = *cp) != 0) {
	cp++;
	if (c == '\\') {			/* parse escape sequence */
	    if ((c = *cp) == 0)
		break;				/* end of input, punt */
	    cp++;
	    VSTRING_ADDCH(arg->vstrval, c);	/* store escaped character */
	} else if (c == last) {
	    return (cp);			/* closing quote */
	} else if (c == '"') {
	    cp = smtp_quoted(cp, arg, c);	/* recurse */
	} else {
	    VSTRING_ADDCH(arg->vstrval, c);	/* store character */
	}
    }
    arg->tokval = SMTPD_TOK_ERROR;		/* missing end */
    return (cp);
}

/* smtp_next_token - extract next token from input, update cp */

static char *smtp_next_token(char *cp, SMTPD_TOKEN *arg)
{
    char   *special = "<[\">]:";
    int     c;

    VSTRING_RESET(arg->vstrval);
    arg->tokval = SMTPD_TOK_OTHER;

    for (;;) {
	if ((c = *cp++) == 0) {
	    return (0);
	} else if (ISSPACE(c)) {		/* whitespace, skip */
	    while (ISSPACE(*cp))
		cp++;
	    continue;
	} else if (c == '<') {			/* <stuff> */
	    arg->tokval = SMTPD_TOK_ADDR;
	    cp = smtp_quoted(cp, arg, '>');
	    break;
	} else if (c == '[') {			/* [stuff], keep [] */
	    VSTRING_ADDCH(arg->vstrval, c);
	    cp = smtp_quoted(cp, arg, ']');
	    if (cp[-1] == ']')
		VSTRING_ADDCH(arg->vstrval, ']');
	    break;
	} else if (c == '"') {			/* string */
	    cp = smtp_quoted(cp, arg, c);
	    break;
	} else if (ISCNTRL(c) || strchr(special, c)) {
	    VSTRING_ADDCH(arg->vstrval, c);
	    break;
	} else {				/* other */
	    if (c == '\\')
		if ((c = *cp) == 0)
		    break;
	    VSTRING_ADDCH(arg->vstrval, c);
	    COLLECT(cp, arg->vstrval, c,
		    !ISSPACE(c) && !ISCNTRL(c) && !strchr(special, c));
	    break;
	}
    }
    VSTRING_TERMINATE(arg->vstrval);
    arg->strval = vstring_str(arg->vstrval);
    return (cp);
}

/* smtpd_token_init - initialize token structures */

static void smtpd_token_init(char *ptr, int count)
{
    SMTPD_TOKEN *arg;
    int     n;

    for (arg = (SMTPD_TOKEN *) ptr, n = 0; n < count; arg++, n++)
	arg->vstrval = vstring_alloc(10);
}

/* smtpd_token - tokenize SMTPD command */

int     smtpd_token(char *cp, SMTPD_TOKEN **argvp)
{
    static SMTPD_TOKEN *smtp_argv;
    static MVECT mvect;
    int     n;

    if (smtp_argv == 0)
	smtp_argv = (SMTPD_TOKEN *) mvect_alloc(&mvect, sizeof(*smtp_argv), 1,
					     smtpd_token_init, (MVECT_FN) 0);
    for (n = 0; /* void */ ; n++) {
	smtp_argv = (SMTPD_TOKEN *) mvect_realloc(&mvect, n + 1);
	if ((cp = smtp_next_token(cp, smtp_argv + n)) == 0)
	    break;
    }
    *argvp = smtp_argv;
    return (n);
}

#ifdef TEST

 /*
  * Test program for the SMTPD command tokenizer.
  */

#include <stdlib.h>
#include <vstream.h>
#include <vstring_vstream.h>

main(int unused_argc, char **unused_argv)
{
    VSTRING *vp = vstring_alloc(10);
    int     tok_argc;
    SMTPD_TOKEN *tok_argv;
    int     i;

    for (;;) {
	vstream_printf("enter SMTPD command: ");
	vstream_fflush(VSTREAM_OUT);
	if (vstring_fgets(vp, VSTREAM_IN) == 0)
	    break;
	tok_argc = smtpd_token(vstring_str(vp), &tok_argv);
	for (i = 0; i < tok_argc; i++) {
	    vstream_printf("Token type:  %s\n",
			   tok_argv[i].tokval == SMTPD_TOK_ADDR ?
			   "address" : "other");
	    vstream_printf("Token value: %s\n", tok_argv[i].strval);
	}
    }
    exit(0);
}

#endif
