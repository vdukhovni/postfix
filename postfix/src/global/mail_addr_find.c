/*++
/* NAME
/*	mail_addr_find 3
/* SUMMARY
/*	generic address-based lookup
/* SYNOPSIS
/*	#include <mail_addr_find.h>
/*
/*	const char *mail_addr_find_int_to_ext(maps, address, extension)
/*	MAPS	*maps;
/*	const char *address;
/*	char	**extension;
/*
/*	const char *mail_addr_find_opt(maps, address, extension,
/*					in_form, out_form)
/*	MAPS	*maps;
/*	const char *address;
/*	char	**extension;
/*	int	in_form;
/*	int	out_form;
/* LEGACY SUPPORT
/*	const char *mail_addr_find(maps, address, extension)
/*	MAPS	*maps;
/*	const char *address;
/*	char	**extension;
/*
/*	const char *mail_addr_find_trans(maps, address, extension)
/*	MAPS	*maps;
/*	const char *address;
/*	char	**extension;
/* DESCRIPTION
/*	mail_addr_find_int_to_ext() searches the specified maps for
/*	an entry with as key the specified address, and derivations
/*	from that address.  It is up to the caller to specify its
/*	case sensitivity preferences when it opens the maps. The
/*	search address is in internal (unquoted) form. The result
/*	is in the form found in the table (it is not necessarily
/*	an email address). This version avoids internal/external
/*	(unquoted/quoted) conversions of the query, extension, or
/*	result.
/*
/*	mail_addr_find_opt() gives more control, at the cost of
/*	additional conversions between internal and external forms.
/*	In particular, the output conversion to internal form assumes
/*	that the lookup result is an email address.
/*
/*	mail_addr_find() is used by legacy code that is not
/*	yet aware of internal versus external addres formats.
/*
/*	mail_addr_find_trans() implements transitional functionality.
/*	It behaves like mail_addr_find_opt(...INTERNAL, ...NOCONV) and
/*	searches a table with the quoted form of the address, but
/*	if the lookup produces no result, and the quoted address
/*	differs from the unquoted form, it also tries
/*	mail_addr_find_opt(...NOCONV, ...NOCONV).
/*
/*	An address that is in the form \fIuser\fR matches itself.
/*
/*	Given an address of the form \fIuser@domain\fR, the following
/*	lookups are done in the given order until one returns a result:
/* .IP user@domain
/*	Look up the entire address.
/* .IP user
/*	Look up \fIuser\fR when \fIdomain\fR is equal to $myorigin,
/*	when \fIdomain\fR matches $mydestination, or when it matches
/*	$inet_interfaces or $proxy_interfaces.
/* .IP @domain
/*	Look for an entry that matches the domain specified in \fIaddress\fR.
/* .PP
/*	With address extension enabled, the table lookup order is:
/*	\fIuser+extension\fR@\fIdomain\fR, \fIuser\fR@\fIdomain\fR,
/*	\fIuser+extension\fR, \fIuser\fR, and @\fIdomain\fR.
/* .PP
/*	Arguments:
/* .IP maps
/*	Dictionary search path (see maps(3)).
/* .IP address
/*	The address to be looked up.
/* .IP extension
/*	A null pointer, or the address of a pointer that is set to
/*	the address of a dynamic memory copy of the address extension
/*	that had to be chopped off in order to match the lookup tables.
/*	The copy includes the recipient address delimiter.
/*	The copy is in internal (unquoted) form.
/*	The caller is expected to pass the copy to myfree().
/* .IP in_form
/* .IP out_form
/*	Input and output address forms, either MAIL_ADDR_FORM_INTERNAL
/*	(unquoted form) or MAIL_ADDR_FORM_EXTERNAL (quoted form).
/* DIAGNOSTICS
/*	The maps->error value is non-zero when the lookup
/*	should be tried again.
/* SEE ALSO
/*	maps(3), multi-dictionary search
/*	resolve_local(3), recognize local system
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
#include <unistd.h>

/* Utility library. */

#include <msg.h>
#include <dict.h>
#include <stringops.h>
#include <mymalloc.h>
#include <vstring.h>

/* Global library. */

#include <mail_params.h>
#include <strip_addr.h>
#include <mail_addr_find.h>
#include <resolve_local.h>
#include <quote_822_local.h>

/* Application-specific. */

#define STR	vstring_str

/* mail_addr_find_trans - transitional support (migration tool) */

const char *mail_addr_find_trans(MAPS *path, const char *address, char **extp)
{
    const char *result;
    static VSTRING *quoted_addr;

    /*
     * First, let mail_addr_find_opt() search with the address converted to
     * external form. Fall back to a search with the address in internal
     * (unconverted) form, if no match was found and the internal and
     * external forms differ.
     */
    if ((result = mail_addr_find_opt(path, address, extp,
		    MAIL_ADDR_FORM_INTERNAL, MAIL_ADDR_FORM_NOCONV)) == 0) {
	if (quoted_addr == 0)
	    quoted_addr = vstring_alloc(100);
	quote_822_local(quoted_addr, address);
	if (strcmp(STR(quoted_addr), address) != 0)
	    result = mail_addr_find_opt(path, address, extp,
			      MAIL_ADDR_FORM_NOCONV, MAIL_ADDR_FORM_NOCONV);
    }
    return (result);
}

/* find_addr - helper to search map with external-form address */

static const char *find_addr(MAPS *path, const char *address, int flags,
			             int in_form, VSTRING *ext_addr_buf)
{
    if (in_form == MAIL_ADDR_FORM_INTERNAL) {
	quote_822_local(ext_addr_buf, address);
	address = STR(ext_addr_buf);
    }
    return (maps_find(path, address, flags));
}

/* mail_addr_find - map a canonical address */

const char *mail_addr_find_opt(MAPS *path, const char *address, char **extp,
			               int in_form, int out_form)
{
    const char *myname = "mail_addr_find";
    VSTRING *ext_addr_buf = 0;
    VSTRING *int_addr_buf = 0;
    const char *int_addr;
    static VSTRING *int_result = 0;
    const char *result;
    char   *ratsign = 0;
    char   *int_full_key;
    char   *int_bare_key;
    char   *saved_ext;
    int     rc = 0;

    /*
     * Optionally convert input from external form.
     */
    if (in_form == MAIL_ADDR_FORM_EXTERNAL) {
	int_addr_buf = vstring_alloc(100);
	unquote_822_local(int_addr_buf, address);
	int_addr = STR(int_addr_buf);
	in_form = MAIL_ADDR_FORM_INTERNAL;
    } else {
	int_addr = address;
    }
    if (in_form == MAIL_ADDR_FORM_INTERNAL)
	ext_addr_buf = vstring_alloc(100);

    /*
     * Initialize.
     */
    int_full_key = mystrdup(int_addr);
    if (*var_rcpt_delim == 0) {
	int_bare_key = saved_ext = 0;
    } else {
	int_bare_key =
	    strip_addr(int_full_key, &saved_ext, var_rcpt_delim);
    }

    /*
     * Try user+foo@domain and user@domain.
     * 
     * Specify what keys are partial or full, to avoid matching partial
     * addresses with regular expressions.
     */
#define FULL	0
#define PARTIAL	DICT_FLAG_FIXED

    if ((result = find_addr(path, int_full_key, FULL,
			    in_form, ext_addr_buf)) == 0
	&& path->error == 0 && int_bare_key != 0
	&& (result = find_addr(path, int_bare_key, PARTIAL,
			       in_form, ext_addr_buf)) != 0
	&& extp != 0) {
	*extp = saved_ext;
	saved_ext = 0;
    }

    /*
     * Try user+foo@$myorigin, user+foo@$mydestination or
     * user+foo@[${proxy,inet}_interfaces]. Then try with +foo stripped off.
     */
    if (result == 0 && path->error == 0
	&& (ratsign = strrchr(int_full_key, '@')) != 0
	&& (strcasecmp_utf8(ratsign + 1, var_myorigin) == 0
	    || (rc = resolve_local(ratsign + 1)) > 0)) {
	*ratsign = 0;
	result = find_addr(path, int_full_key, PARTIAL, in_form, ext_addr_buf);
	if (result == 0 && path->error == 0 && int_bare_key != 0) {
	    if ((ratsign = strrchr(int_bare_key, '@')) == 0)
		msg_panic("%s: bare key botch", myname);
	    *ratsign = 0;
	    if ((result = find_addr(path, int_bare_key, PARTIAL,
				    in_form, ext_addr_buf)) != 0
		&& extp != 0) {
		*extp = saved_ext;
		saved_ext = 0;
	    }
	}
	*ratsign = '@';
    } else if (rc < 0)
	path->error = rc;

    /*
     * Try @domain.
     */
    if (result == 0 && path->error == 0 && ratsign)
	result = maps_find(path, ratsign, PARTIAL);	/* addr form is OK */

    /*
     * Optionally convert the result to internal form. The lookup result is
     * supposed to be in external form.
     */
    if (result != 0 && out_form == MAIL_ADDR_FORM_INTERNAL) {
	if (int_result == 0)
	    int_result = vstring_alloc(100);
	unquote_822_local(int_result, result);
	result = STR(int_result);
    }

    /*
     * Clean up.
     */
    if (msg_verbose)
	msg_info("%s: %s -> %s", myname, address,
		 result ? result :
		 path->error ? "(try again)" :
		 "(not found)");
    myfree(int_full_key);
    if (int_bare_key)
	myfree(int_bare_key);
    if (saved_ext)
	myfree(saved_ext);
    if (int_addr_buf)
	vstring_free(int_addr_buf);
    if (ext_addr_buf)
	vstring_free(ext_addr_buf);
    return (result);
}

#ifdef TEST

 /*
  * Proof-of-concept test program. Read an address from stdin, and spit out
  * the lookup result.
  */
#include <vstream.h>
#include <vstring_vstream.h>
#include <name_code.h>
#include <mail_conf.h>

static NORETURN usage(const char *progname)
{
    msg_fatal("usage: %s [-v] database", progname);
}

int     main(int argc, char **argv)
{
    VSTRING *buffer = vstring_alloc(100);
    char   *bp;
    MAPS   *path;
    const char *result;
    char   *extent;
    char   *in_field;
    char   *out_field;
    char   *key_field;
    char   *expect_res;
    char   *expect_ext;
    int     in_form;
    int     out_form;
    int     ch;
    int     errs = 0;

    /*
     * Parse JCL.
     */
    while ((ch = GETOPT(argc, argv, "v")) > 0) {
	switch (ch) {
	case 'v':
	    msg_verbose++;
	    break;
	default:
	    usage(argv[0]);
	}
    }
    if (argc != optind + 1)
	usage(argv[0]);

    /*
     * Initialize.
     */
    mail_conf_read();				/* XXX eliminate dependency. */
    myfree(var_rcpt_delim);
    var_rcpt_delim = mystrdup("+");
    myfree(var_myorigin);
    var_myorigin = mystrdup("localhost.localdomain");
    myfree(var_mydest);
    var_mydest = mystrdup("localhost.localdomain");
    path = maps_create(argv[0], argv[optind], DICT_FLAG_LOCK
		       | DICT_FLAG_FOLD_FIX | DICT_FLAG_UTF8_REQUEST);
    while (vstring_fgets_nonl(buffer, VSTREAM_IN)) {
	bp = STR(buffer);
	if ((in_field = mystrtok(&bp, ":")) == 0)
	    msg_fatal("no input form");
	if ((in_form = mail_addr_form_from_string(in_field)) < 0)
	    msg_fatal("bad input form: '%s'", in_field);
	if ((out_field = mystrtok(&bp, ":")) == 0)
	    msg_fatal("no output form");
	if ((out_form = mail_addr_form_from_string(out_field)) < 0)
	    msg_fatal("bad output form: '%s'", out_field);
	if ((key_field = mystrtok(&bp, ":")) == 0)
	    msg_fatal("no search key");
	expect_res = mystrtok(&bp, ":");
	expect_ext = mystrtok(&bp, ":");
	extent = 0;
	result = mail_addr_find_opt(path, key_field, &extent, in_form, out_form);
	vstream_printf("%s:%s -> %s:%s (%s)\n",
		       in_field, key_field, out_field, result ? result :
		       path->error ? "(try again)" :
		       "(not found)", extent ? extent : "null extension");
	if (expect_res && result) {
	    if (strcmp(expect_res, result) != 0) {
		msg_warn("expect result '%s' but got '%s'", expect_res, result);
		errs = 1;
		if (expect_ext && extent) {
		    if (strcmp(expect_ext, extent) != 0)
			msg_warn("expect extension '%s' but got '%s'",
				 expect_ext, extent);
		    errs = 1;
		} else if (expect_ext && !extent) {
		    msg_warn("expect extension '%s' but got none", expect_ext);
		    errs = 1;
		} else if (!expect_ext && extent) {
		    msg_warn("expect no extension but got '%s'", extent);
		    errs = 1;
		}
	    }
	} else if (expect_res && !result) {
	    msg_warn("expect result '%s' but got none", expect_res);
	    errs = 1;
	} else if (!expect_res && result) {
	    msg_warn("expected no result but got '%s'", result);
	    errs = 1;
	}
	vstream_fflush(VSTREAM_OUT);
	if (extent)
	    myfree(extent);
    }
    vstring_free(buffer);

    maps_free(path);
    return (errs != 0);
}

#endif
