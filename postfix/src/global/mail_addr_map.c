/*++
/* NAME
/*	mail_addr_map 3
/* SUMMARY
/*	generic address mapping
/* SYNOPSIS
/*	#include <mail_addr_map.h>
/*
/*	ARGV	*mail_addr_map_internal(path, address, propagate)
/*	MAPS	*path;
/*	const char *address;
/*	int	propagate;
/*
/*	ARGV	*mail_addr_map(path, address, propagate, in_form, out_form)
/*	MAPS	*path;
/*	const char *address;
/*	int	propagate;
/*	int	how;
/* DESCRIPTION
/*	mail_addr_map_internal() returns the translation for the
/*	named address, or a null pointer if none is found.  The
/*	search address and results are in internal (unquoted) form.
/*
/*	mail_addr_map() gives more control, at the cost of additional
/*	conversions between internal and external forms.
/*
/*	When the \fBpropagate\fR argument is non-zero,
/*	address extensions that aren't explicitly matched in the lookup
/*	table are propagated to the result addresses. The caller is
/*	expected to pass the result to argv_free().
/*
/*	Lookups are performed by mail_addr_find_internal(). When
/*	the result has the form \fI@otherdomain\fR, the result is
/*	the original user in
/*	\fIotherdomain\fR.
/*
/*	mail_addr_map() gives additional control over whether the
/*	input is in internal (unquoted) or external (quoted) form.
/*	to internal form and invokes mail_addr_map_int_to_ext().
/*	This may introduce additional unqoute822_local() and
/*	quote_833_local() calls.
/*
/*	Arguments:
/* .IP path
/*	Dictionary search path (see maps(3)).
/* .IP address
/*	The address to be looked up in external (quoted) form, or
/*	in the form specified with the in_form argument.
/* .IP in_form
/* .IP out_form
/*	Input and output address forms, either MAIL_ADDR_FORM_INTERNAL
/*	(unquoted form) or MAIL_ADDR_FORM_EXTERNAL (quoted form).
/* DIAGNOSTICS
/*	Warnings: map lookup returns a non-address result.
/*
/*	The path->error value is non-zero when the lookup
/*	should be tried again.
/* SEE ALSO
/*	mail_addr_find(3), mail address matching
/*	mail_addr_crunch(3), mail address parsing and rewriting
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
#include <vstring.h>
#include <dict.h>
#include <argv.h>
#include <mymalloc.h>

/* Global library. */

#include <quote_822_local.h>
#include <mail_addr_find.h>
#include <mail_addr_crunch.h>
#include <mail_addr_map.h>

/* Application-specific. */

#define STR	vstring_str
#define LEN	VSTRING_LEN

/* mail_addr_map - map a canonical address */

ARGV   *mail_addr_map(MAPS *path, const char *address, int propagate,
		              int in_form, int out_form)
{
    VSTRING *buffer = 0;
    const char *myname = "mail_addr_map";
    const char *string;
    char   *ratsign;
    char   *extension = 0;
    ARGV   *argv = 0;
    int     i;
    VSTRING *int_address = 0;
    VSTRING *ext_address = 0;
    const char *int_addr;

    /* Crutch until we can retire MAIL_ADDR_FORM_NOCONV. */
    int     mid_form = (out_form == MAIL_ADDR_FORM_NOCONV ?
			MAIL_ADDR_FORM_NOCONV : MAIL_ADDR_FORM_EXTERNAL);

    /*
     * Optionally convert input from external form. We prefer internal-form
     * input to avoid an unnecessary input conversion in mail_addr_find().
     * But the consequence is that we have to convert the internal-form
     * input's localpart to external form when mapping @domain -> @domain.
     */
    if (in_form == MAIL_ADDR_FORM_EXTERNAL) {
	int_address = vstring_alloc(100);
	unquote_822_local(int_address, address);
	int_addr = STR(int_address);
	in_form = MAIL_ADDR_FORM_INTERNAL;
    } else {
	int_addr = address;
    }

    /*
     * Look up the full address; if no match is found, look up the address
     * with the extension stripped off, and remember the unmatched extension.
     * We explicitly call the mail_addr_find() variant that does not convert
     * the lookup result.
     */
    if ((string = mail_addr_find(path, int_addr, &extension,
				 in_form, mid_form)) != 0) {

	/*
	 * Prepend the original user to @otherdomain, but do not propagate
	 * the unmatched address extension.
	 */
	if (*string == '@') {
	    buffer = vstring_alloc(100);
	    if ((ratsign = strrchr(int_addr, '@')) != 0)
		vstring_strncpy(buffer, int_addr, ratsign - int_addr);
	    else
		vstring_strcpy(buffer, int_addr);
	    if (extension)
		vstring_truncate(buffer, LEN(buffer) - strlen(extension));
	    ext_address = vstring_alloc(100);
	    quote_822_local(ext_address, STR(buffer));
	    vstring_strcat(ext_address, string);
	    string = STR(ext_address);
	}

	/*
	 * Canonicalize the result, and propagate the unmatched extension to
	 * each address found.
	 */
	argv = mail_addr_crunch(string, propagate ? extension : 0,
				mid_form, out_form);
	if (buffer)
	    vstring_free(buffer);
	if (ext_address)
	    vstring_free(ext_address);
	if (msg_verbose)
	    for (i = 0; i < argv->argc; i++)
		msg_info("%s: %s -> %d: %s", myname, address, i, argv->argv[i]);
	if (argv->argc == 0) {
	    msg_warn("%s lookup of %s returns non-address result \"%s\"",
		     path->title, address, string);
	    argv = argv_free(argv);
	    path->error = DICT_ERR_RETRY;
	}
    }

    /*
     * No match found.
     */
    else {
	if (msg_verbose)
	    msg_info("%s: %s -> %s", myname, address,
		     path->error ? "(try again)" : "(not found)");
    }

    /*
     * Cleanup.
     */
    if (extension)
	myfree(extension);
    if (int_address)
	vstring_free(int_address);

    return (argv);
}
