/*++
/* NAME
/*	mac_parse 3
/* SUMMARY
/*	locate macro references in string
/* SYNOPSIS
/*	#include <mac_parse.h>
/*
/*	void	mac_parse(string, action, context)
/*	const char *string;
/*	void	(*action)(int type, VSTRING *buf, char *context);
/* DESCRIPTION
/*	This module recognizes macro references in null-terminated
/*	strings.  Macro references have the form $name, $(name) or
/*	${name}. A macro name consists of alphanumerics and/or
/*	underscore. Other text is treated as literal text.
/*
/*	mac_parse() breaks up its string argument into macro references
/*	and other text, and invokes the \fIaction\fR routine for each item
/*	found.  With each action routine call, the \fItype\fR argument
/*	indicates what was found, \fIbuf\fR contains a copy of the text
/*	found, and \fIcontext\fR is passed on unmodified from the caller.
/*	The application is at liberty to clobber \fIbuf\fR.
/* .IP MAC_PARSE_LITERAL
/*	The text in \fIbuf\fR is literal text.
/* .IP MAC_PARSE_VARNAME
/*	The text in \fIbuf\fR is a macro name.
/* SEE ALSO
/*	dict(3) dictionary interface.
/* DIAGNOSTICS
/*	Fatal errors: out of memory, malformed macro name.
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

/* Utility library. */

#include <msg.h>
#include <mac_parse.h>

 /*
  * Helper macro for consistency. Null-terminate the temporary buffer,
  * execute the action, and reset the temporary buffer for re-use.
  */
#define MAC_PARSE_ACTION(type, buf, context) \
	{ \
	    VSTRING_TERMINATE(buf); \
	    action(type, buf, context); \
	    VSTRING_RESET(buf); \
	}

/* mac_parse - split string into literal text and macro references */

void    mac_parse(const char *value, MAC_PARSE_FN action, char *context)
{
    char   *myname = "mac_parse";
    VSTRING *buf = vstring_alloc(1);	/* result buffer */
    const char *vp;			/* value pointer */
    const char *pp;			/* open_paren pointer */
    const char *ep;			/* string end pointer */
    static char open_paren[] = "({";
    static char close_paren[] = ")}";

#define SKIP(start, var, cond) \
        for (var = start; *var && (cond); var++);

    if (msg_verbose > 1)
	msg_info("%s: %s", myname, value);

    for (vp = value; *vp;) {
	if (*vp != '$') {			/* ordinary character */
	    VSTRING_ADDCH(buf, *vp);
	    vp += 1;
	} else if (vp[1] == '$') {		/* $$ becomes $ */
	    VSTRING_ADDCH(buf, *vp);
	    vp += 2;
	} else {				/* found bare $ */
	    if (VSTRING_LEN(buf) > 0)
		MAC_PARSE_ACTION(MAC_PARSE_LITERAL, buf, context);
	    vp += 1;
	    pp = open_paren;
	    if (*vp == *pp || *vp == *++pp) {	/* ${x} or $(x) */
		vp += 1;
		SKIP(vp, ep, *ep != close_paren[pp - open_paren]);
		if (*ep == 0)
		    msg_fatal("incomplete macro: %s", value);
		vstring_strncat(buf, vp, ep - vp);
		vp = ep + 1;
	    } else {				/* plain $x */
		SKIP(vp, ep, ISALNUM(*ep) || *ep == '_');
		vstring_strncat(buf, vp, ep - vp);
		vp = ep;
	    }
	    if (VSTRING_LEN(buf) == 0)
		msg_fatal("empty macro name: %s", value);
	    MAC_PARSE_ACTION(MAC_PARSE_VARNAME, buf, context);
	}
    }
    if (VSTRING_LEN(buf) > 0)
	MAC_PARSE_ACTION(MAC_PARSE_LITERAL, buf, context);

    /*
     * Cleanup.
     */
    vstring_free(buf);
}

#ifdef TEST

 /*
  * Proof-of-concept test program. Read strings from stdin, print parsed
  * result to stdout.
  */
#include <vstring_vstream.h>

/* mac_parse_print - print parse tree */

static void mac_parse_print(int type, VSTRING *buf, char *unused_context)
{
    char   *type_name;

    switch (type) {
    case MAC_PARSE_VARNAME:
	type_name = "MAC_PARSE_VARNAME";
	break;
    case MAC_PARSE_LITERAL:
	type_name = "MAC_PARSE_LITERAL";
	break;
    default:
	msg_panic("unknown token type %d", type);
    }
    vstream_printf("%s \"%s\"\n", type_name, vstring_str(buf));
}

int     main(int unused_argc, char **unused_argv)
{
    VSTRING *buf = vstring_alloc(1);

    while (vstring_fgets_nonl(buf, VSTREAM_IN)) {
	mac_parse(vstring_str(buf), mac_parse_print, (char *) 0);
	vstream_fflush(VSTREAM_OUT);
    }
    vstring_free(buf);
}

#endif
