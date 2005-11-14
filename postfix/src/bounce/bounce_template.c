/*++
/* NAME
/*	bounce_template 3
/* SUMMARY
/*	bounce template processing
/* SYNOPSIS
/*	#include "bounce_service.h"
/*
/*	void	bounce_template_load(path)
/*	const char *path;
/*
/*	const BOUNCE_TEMPLATE *FAIL_TEMPLATE()
/*
/*	const BOUNCE_TEMPLATE *DELAY_TEMPLATE()
/*
/*	const BOUNCE_TEMPLATE *SUCCESS_TEMPLATE()
/*
/*	const BOUNCE_TEMPLATE *VERIFY_TEMPLATE()
/*
/*	void	bounce_template_expand(stream, template)
/*	VSTREAM	*stream;
/*	BOUNCE_TEMPLATE *template;
/* AUXILIARY FUNCTIONS
/*	void	bounce_template_dump_all(stream)
/*	VSTREAM	*stream;
/*
/*	void	bounce_template_expand_all(stream)
/*	VSTREAM	*stream;
/* DESCRIPTION
/*	This module implements the built-in and external bounce
/*	message template support.
/*
/*	bounce_template_load() reads bounce templates from the
/*	specified file.
/*
/*	FAIL_TEMPLATE() etc. look up the corresponding bounce
/*	template from file, or use a built-in template when no
/*	template was specified externally.
/*
/*	bounce_template_expand() expands the body text of the
/*	specified template and writes the result to the specified
/*	queue file record stream.
/*
/*	bounce_template_dump_default() dumps the built-in default templates
/*	to the specified stream. This can be used to generate input
/*	for the default bounce service configuration file.
/*
/*	bounce_template_dump_actual() dumps the actually-used templates
/*	to the specified stream. This can be used to verify that
/*	the bounce server correctly reads its own bounce_template_dump_default()
/*	output.
/*
/*	bounce_template_expand_actual() expands the template message
/*	text and dumps the result to the specified stream. This can
/*	be used to verify that templates produce the desired text.
/* DIAGNOSTICS
/*	Fatal error: error opening template file, out of memory,
/*	undefined macro name in template.
/* BUGS
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
#include <ctype.h>

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

/* Utility library. */

#include <msg.h>
#include <mac_expand.h>
#include <split_at.h>
#include <stringops.h>
#include <mymalloc.h>
#include <dict_ml.h>

/* Global library. */

#include <mail_params.h>
#include <mail_conf.h>
#include <mail_addr.h>
#include <post_mail.h>
#include <is_header.h>
#include <mail_proto.h>

/* Application-specific. */

#include <bounce_service.h>

 /*
  * The fail template is for permanent failure.
  */
static const char *def_bounce_fail_body[];

const BOUNCE_TEMPLATE def_bounce_fail_template = {
    BOUNCE_TMPL_CLASS_FAIL,
    "us-ascii",
    MAIL_ATTR_ENC_7BIT,
    MAIL_ADDR_MAIL_DAEMON " (Mail Delivery System)",
    "Undelivered Mail Returned to Sender",
    "Postmaster Copy: Undelivered Mail",
    def_bounce_fail_body,
};

static const char *def_bounce_fail_body[] = {
    "This is the $mail_name program at host $myhostname.",
    "",
    "I'm sorry to have to inform you that your message could not",
    "be delivered to one or more recipients. It's attached below.",
    "",
    "For further assistance, please send mail to <" MAIL_ADDR_POSTMASTER ">",
    "",
    "If you do so, please include this problem report. You can",
    "delete your own text from the attached returned message.",
    "",
    "                   The $mail_name program",
    0,
};

 /*
  * The delay template is for delayed mail notifications.
  */
static const char *def_bounce_delay_body[];

const BOUNCE_TEMPLATE def_bounce_delay_template = {
    BOUNCE_TMPL_CLASS_DELAY,
    "us-ascii",
    MAIL_ATTR_ENC_7BIT,
    MAIL_ADDR_MAIL_DAEMON " (Mail Delivery System)",
    "Delayed Mail (still being retried)",
    "Postmaster Warning: Delayed Mail",
    def_bounce_delay_body,
};

static const char *def_bounce_delay_body[] = {
    "This is the $mail_name program at host $myhostname.",
    "",
    "####################################################################",
    "# THIS IS A WARNING ONLY.  YOU DO NOT NEED TO RESEND YOUR MESSAGE. #",
    "####################################################################",
    "",
    "Your message could not be delivered for $delay_warning_time_hours hour(s).",
    "It will be retried until it is $maximal_queue_lifetime_days day(s) old.",
    "",
    "For further assistance, please send mail to <" MAIL_ADDR_POSTMASTER ">",
    "",
    "If you do so, please include this problem report. You can",
    "delete your own text from the attached returned message.",
    "",
    "                   The $mail_name program",
    0,
};

 /*
  * The success template is for "delivered", "expanded" and "relayed" success
  * notifications.
  */
static const char *def_bounce_success_body[];

const BOUNCE_TEMPLATE def_bounce_success_template = {
    BOUNCE_TMPL_CLASS_SUCCESS,
    "us-ascii",
    MAIL_ATTR_ENC_7BIT,
    MAIL_ADDR_MAIL_DAEMON " (Mail Delivery System)",
    "Successful Mail Delivery Report",
    0,
    def_bounce_success_body,
};

static const char *def_bounce_success_body[] = {
    "This is the $mail_name program at host $myhostname.",
    "",
    "Your message was successfully delivered to the destination(s)",
    "listed below. If the message was delivered to mailbox you will",
    "receive no further notifications. Otherwise you may still receive",
    "notifications of mail delivery errors from other systems.",
    "",
    "                   The $mail_name program",
    0,
};

 /*
  * The "verify" template is for verbose delivery (sendmail -v) and for
  * address verification (sendmail -bv).
  */
static const char *def_bounce_verify_body[];

const BOUNCE_TEMPLATE def_bounce_verify_template = {
    BOUNCE_TMPL_CLASS_VERIFY,
    "us-ascii",
    MAIL_ATTR_ENC_7BIT,
    MAIL_ADDR_MAIL_DAEMON " (Mail Delivery System)",
    "Mail Delivery Status Report",
    0,
    def_bounce_verify_body,
};

static const char *def_bounce_verify_body[] = {
    "This is the $mail_name program at host $myhostname.",
    "",
    "Enclosed is the mail delivery report that you requested.",
    "",
    "                   The $mail_name program",
    0,
};

 /*
  * Pointers, so that we can override a built-in template with one from file
  * without clobbering the built-in template.
  */
const BOUNCE_TEMPLATE *bounce_fail_template;
const BOUNCE_TEMPLATE *bounce_delay_template;
const BOUNCE_TEMPLATE *bounce_success_template;
const BOUNCE_TEMPLATE *bounce_verify_template;

 /*
  * The following tables implement support for bounce template expansions of
  * $<parameter_name>_days ($<parameter_name>_hours, etc.). The expansion of
  * these is the actual parameter value divided by the number of seconds in a
  * day (hour, etc.), so that we can produce nicely formatted bounce messages
  * with time values converted into the appropriate units.
  * 
  * Ideally, the bounce template processor would strip the _days etc. suffix
  * from the parameter name, and use the parameter name to look up the actual
  * parameter value and its default value (the default value specifies the
  * default time unit of that parameter (seconds, minutes, etc.), and allows
  * us to convert the parameter string value into the corresponding number of
  * seconds). The bounce template processor would then use the _hours etc.
  * suffix from the bounce template to divide this number by the number of
  * seconds in an hour, etc. and produce the number that is needed for the
  * template.
  * 
  * Unfortunately, there exists no code to look up default values by parameter
  * name. If such code existed, then we could do the _days, _hours, etc.
  * conversion with every main.cf time parameter without having to know in
  * advance what time parameter names exist.
  * 
  * So we have to either maintain our own table of all time related main.cf
  * parameter names and defaults (like the postconf command does) or we make
  * a special case for a few parameters of special interest.
  * 
  * We go for the second solution. There are only a few parameters that need
  * this treatment, and there will be more special cases when individual
  * queue files get support for individual expiration times, and when other
  * queue file information needs to be reported in bounce template messages.
  * 
  * A really lame implementation would simply strip the optional s, h, d, etc.
  * suffix from the actual (string) parameter value and not do any conversion
  * at all to hours, days or weeks. But then the information in delay warning
  * notices could be seriously incorrect.
  */
typedef struct {
    const char *suffix;			/* days, hours, etc. */
    int     suffix_len;			/* byte count */
    int     divisor;			/* divisor */
} BOUNCE_TIME_DIVISOR;

#define STRING_AND_LEN(x) (x), (sizeof(x) - 1)

static BOUNCE_TIME_DIVISOR time_divisors[] = {
    STRING_AND_LEN("seconds"), 1,
    STRING_AND_LEN("minutes"), 60,
    STRING_AND_LEN("hours"), 60 * 60,
    STRING_AND_LEN("days"), 24 * 60 * 60,
    STRING_AND_LEN("weeks"), 7 * 24 * 60 * 60,
    0, 0,
};

 /*
  * The few special-case main.cf parameters that have support for _days, etc.
  * suffixes for automatic conversion when expanded into a bounce template.
  */
typedef struct {
    const char *param_name;		/* parameter name */
    int     param_name_len;		/* name length */
    int    *value;			/* parameter value */
} BOUNCE_TIME_PARAMETER;

static BOUNCE_TIME_PARAMETER time_parameter[] = {
    STRING_AND_LEN(VAR_DELAY_WARN_TIME), &var_delay_warn_time,
    STRING_AND_LEN(VAR_MAX_QUEUE_TIME), &var_max_queue_time,
    0, 0,
};

 /*
  * SLMs.
  */
#define STR(x) vstring_str(x)

/* bounce_template_lookup - lookup $name value */

static const char *bounce_template_lookup(const char *key, int unused_mode,
					          char *context)
{
    BOUNCE_TEMPLATE *template = (BOUNCE_TEMPLATE *) context;
    BOUNCE_TIME_PARAMETER *bp;
    BOUNCE_TIME_DIVISOR *bd;
    static VSTRING *buf;
    int     result;

    /*
     * Look for parameter names that can have a time unit suffix, and scale
     * the time value according to the suffix.
     */
    for (bp = time_parameter; bp->param_name; bp++) {
	if (strncmp(key, bp->param_name, bp->param_name_len) == 0
	    && key[bp->param_name_len] == '_') {
	    for (bd = time_divisors; bd->suffix; bd++) {
		if (strcmp(key + bp->param_name_len + 1, bd->suffix) == 0) {
		    result = bp->value[0] / bd->divisor;
		    if (result > 999 && bd->divisor < 86400) {
			msg_warn("%s: excessive result \"%d\" in %s "
				 "template conversion of parameter \"%s\"",
			  *var_bounce_tmpl ? var_bounce_tmpl : "[built-in]",
				 result, template->class, key);
			msg_warn("please increase time unit \"%s\" of \"%s\" "
				 "in %s template", bd->suffix, key,
				 template->class);
		    } else if (result == 0 && bp->value[0] && bd->divisor > 1) {
			msg_warn("%s: zero result in %s template "
				 "conversion of parameter \"%s\"",
			  *var_bounce_tmpl ? var_bounce_tmpl : "[built-in]",
				 template->class, key);
			msg_warn("please reduce time unit \"%s\" of \"%s\" "
				 "in %s template", bd->suffix, key,
				 template->class);
		    }
		    if (buf == 0)
			buf = vstring_alloc(10);
		    vstring_sprintf(buf, "%d", result);
		    return (STR(buf));
		}
	    }
	    msg_fatal("%s: unrecognized suffix \"%s\" in parameter \"%s\"",
		      *var_bounce_tmpl ? var_bounce_tmpl : "[built-in]",
		      key + bp->param_name_len + 1, key);
	}
    }
    return (mail_conf_lookup_eval(key));
}

/* bounce_template_expand - expand template body */

void    bounce_template_expand(BOUNCE_OUT_FN out_fn, VSTREAM *stream,
			               const BOUNCE_TEMPLATE *template)
{
    VSTRING *buf = vstring_alloc(100);
    const char **cpp;
    int     stat;
    const char *filter = "\t !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

#define NO_CONTEXT      ((char *) 0)

    for (cpp = template->message_text; *cpp; cpp++) {
	stat = mac_expand(buf, *cpp, MAC_EXP_FLAG_NONE, filter,
			  bounce_template_lookup, (char *) template);
	if (stat & MAC_PARSE_ERROR)
	    msg_fatal("%s: bad $name syntax in %s template: %s",
		      *var_bounce_tmpl ? var_bounce_tmpl : "[built-in]",
		      template->class, *cpp);
	if (stat & MAC_PARSE_UNDEF)
	    msg_fatal("%s: undefined $name in %s template: %s",
		      *var_bounce_tmpl ? var_bounce_tmpl : "[built-in]",
		      template->class, *cpp);
	out_fn(stream, STR(buf));
    }
    vstring_free(buf);
}

/* bounce_template_load - load template(s) from file */

void    bounce_template_load(const char *path)
{
    static int once = 0;

    /*
     * Split the input stream into chunks between begin/end markers, ignoring
     * comment lines.
     */
    if (once++ > 0)
	msg_panic("bounce_template_load: multiple calls");
    dict_ml_load_file(BOUNCE_TEMPLATE_DICT, path);
}

/* bounce_template_find - return default or user-specified template */

const BOUNCE_TEMPLATE *bounce_template_find(const char *template_name,
				        const BOUNCE_TEMPLATE *def_template)
{
    BOUNCE_TEMPLATE *tp;
    char   *tval;
    char   *cp;
    char  **cpp;
    int     cpp_len;
    int     cpp_used;
    int     hlen;
    char   *hval;

    /*
     * Look up a non-default template. Once we found it we are going to
     * destroy it; no-one will access this data again.
     */
    if (*var_bounce_tmpl == 0
	|| (tval = (char *) dict_lookup(BOUNCE_TEMPLATE_DICT, template_name)) == 0)
	return (def_template);

    /*
     * Initialize a new template. We're not going to use the message text
     * from the default template.
     */
    tp = (BOUNCE_TEMPLATE *) mymalloc(sizeof(*tp));
    *tp = *def_template;

#define CLEANUP_AND_RETURN(x) do { \
	myfree((char *) tp); \
	return (x); \
    } while (0)


    /*
     * Parse pseudo-header labels and values.
     */
#define GETLINE(line, buf) \
	(((line) = (buf)) ? ((buf) = split_at((buf), '\n'), (line)) : 0)

    while ((GETLINE(cp, tval)) != 0 && (hlen = is_header(cp)) > 0) {
	for (hval = cp + hlen; *hval && (*hval == ':' || ISSPACE(*hval)); hval++)
	    *hval = 0;
	if (*hval == 0) {
	    msg_warn("%s: empty \"%s\" header value in %s template "
		     "-- ignoring this template",
		     *var_bounce_tmpl ? var_bounce_tmpl : "[built-in]",
		     cp, template_name);
	    CLEANUP_AND_RETURN(def_template);
	}
	if (!allascii(hval)) {
	    msg_warn("%s: non-ASCII \"%s\" header value in %s template "
		     "-- ignoring this template",
		     *var_bounce_tmpl ? var_bounce_tmpl : "[built-in]",
		     cp, template_name);
	    CLEANUP_AND_RETURN(def_template);
	}
	if (strcasecmp("charset", cp) == 0) {
	    tp->charset = hval;
	} else if (strcasecmp("from", cp) == 0) {
	    tp->from = hval;
	} else if (strcasecmp("subject", cp) == 0) {
	    tp->subject = hval;
	} else if (strcasecmp("postmaster-subject", cp) == 0) {
	    if (tp->postmaster_subject == 0) {
		msg_warn("%s: inapplicable \"%s\" header label in %s template "
			 "-- ignoring this template",
			 *var_bounce_tmpl ? var_bounce_tmpl : "[built-in]",
			 cp, template_name);
		CLEANUP_AND_RETURN(def_template);
	    }
	    tp->postmaster_subject = hval;
	} else {
	    msg_warn("%s: unknown \"%s\" header label in %s template "
		     "-- ignoring this template",
		     *var_bounce_tmpl ? var_bounce_tmpl : "[built-in]",
		     cp, template_name);
	    CLEANUP_AND_RETURN(def_template);
	}
    }

    /*
     * Skip blank lines between header and message text.
     */
    while (cp && (*cp == 0 || allspace(cp)))
	(void) GETLINE(cp, tval);
    if (cp == 0) {
	msg_warn("%s: missing message text in %s template "
		 "-- ignoring this template",
		 *var_bounce_tmpl ? var_bounce_tmpl : "[built-in]",
		 template_name);
	CLEANUP_AND_RETURN(def_template);
    }

    /*
     * Is this 7bit or 8bit text? If the character set is US-ASCII, then
     * don't allow 8bit text.
     */
#define NON_ASCII(p) (*(p) && !allascii((p)))

    if (NON_ASCII(cp) || NON_ASCII(tval)) {
	if (strcasecmp(tp->charset, "us-ascii") == 0) {
	    msg_warn("%s: 8-bit message text in %s template",
		     *var_bounce_tmpl ? var_bounce_tmpl : "[built-in]",
		     template_name);
	    msg_warn("please specify a charset value other than us-ascii");
	    msg_warn("-- ignoring this template for now");
	    CLEANUP_AND_RETURN(def_template);
	}
	tp->mime_encoding = MAIL_ATTR_ENC_8BIT;
    }

    /*
     * Collect the message text and null-terminate the result.
     */
    cpp_len = 10;
    cpp_used = 0;
    cpp = (char **) mymalloc(sizeof(*cpp) * cpp_len);
    while (cp) {
	cpp[cpp_used++] = cp;
	if (cpp_used >= cpp_len) {
	    cpp = (char **) myrealloc((char *) cpp,
				      sizeof(*cpp) * 2 * cpp_len);
	    cpp_len *= 2;
	}
	(void) GETLINE(cp, tval);
    }
    cpp[cpp_used] = 0;
    tp->message_text = (const char **) cpp;

    return (tp);
}

/* print_template - dump one template */

static void print_template(VSTREAM *stream, const BOUNCE_TEMPLATE *tp)
{
    const char **cpp;

    vstream_fprintf(stream, "%s_template = <<EOF\n", tp->class);
    vstream_fprintf(stream, "Charset: %s\n", tp->charset);
    vstream_fprintf(stream, "From: %s\n", tp->from);
    vstream_fprintf(stream, "Subject: %s\n", tp->subject);
    if (tp->postmaster_subject)
	vstream_fprintf(stream, "Postmaster-Subject: %s\n",
			tp->postmaster_subject);
    vstream_fprintf(stream, "\n");
    for (cpp = tp->message_text; *cpp; cpp++)
	vstream_fprintf(stream, "%s\n", *cpp);
    vstream_fprintf(stream, "EOF\n");
    vstream_fflush(stream);
}

/* bounce_template_dump_all - dump bounce templates to stream */

void    bounce_template_dump_all(VSTREAM *stream)
{
    print_template(VSTREAM_OUT, FAIL_TEMPLATE());
    vstream_fprintf(stream, "\n");
    print_template(VSTREAM_OUT, DELAY_TEMPLATE());
    vstream_fprintf(stream, "\n");
    print_template(VSTREAM_OUT, SUCCESS_TEMPLATE());
    vstream_fprintf(stream, "\n");
    print_template(VSTREAM_OUT, VERIFY_TEMPLATE());
}

/* bounce_plain_out - output line as plain text */

static int bounce_plain_out(VSTREAM *stream, const char *text)
{
    vstream_fprintf(stream, "%s\n", text);
    return (0);
}

/* bounce_template_expand_all - dump expanded template text to stream */

void    bounce_template_expand_all(VSTREAM *stream)
{
    const BOUNCE_TEMPLATE *tp;

    tp = FAIL_TEMPLATE();
    vstream_fprintf(VSTREAM_OUT, "expanded_%s_text = <<EOF\n", tp->class);
    bounce_template_expand(bounce_plain_out, VSTREAM_OUT, tp);
    tp = DELAY_TEMPLATE();
    vstream_fprintf(VSTREAM_OUT, "EOF\n\nexpanded_%s_text = <<EOF\n", tp->class);
    bounce_template_expand(bounce_plain_out, VSTREAM_OUT, tp);
    tp = SUCCESS_TEMPLATE();
    vstream_fprintf(VSTREAM_OUT, "EOF\n\nexpanded_%s_text = <<EOF\n", tp->class);
    bounce_template_expand(bounce_plain_out, VSTREAM_OUT, tp);
    tp = VERIFY_TEMPLATE();
    vstream_fprintf(VSTREAM_OUT, "EOF\n\nexpanded_%s_text = <<EOF\n", tp->class);
    bounce_template_expand(bounce_plain_out, VSTREAM_OUT, tp);
    vstream_fprintf(VSTREAM_OUT, "EOF\n");
}
