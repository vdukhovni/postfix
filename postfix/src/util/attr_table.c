/*++
/* NAME
/*	attr_table 3
/* SUMMARY
/*	recover attributes from byte stream
/* SYNOPSIS
/*	#include <attr.h>
/*
/*	ATTR_TABLE *attr_table_create(size)
/*	int	size;
/*
/*	void	attr_table_free(attr)
/*	ATTR_TABLE *attr;
/*
/*	int	attr_table_read(attr, flags, fp)
/*	ATTR_TABLE *attr;
/*	int	flags;
/*	VSTREAM	fp;
/*
/*	int	attr_table_get(attr, flags, type, name, ...)
/*	ATTR_TABLE *attr;
/*	int	flags;
/*	int	type;
/*	char	*name;
/* DESCRIPTION
/*	This module provides an alternative process for recovering
/*	attribute lists from a byte stream. The process involves the
/*	storage in an intermediate attribute table that is subsequently
/*	queried. This procedure gives more control to the application,
/*	at the cost of complexity and of memory resources.
/*
/*	attr_table_create() creates an empty table for storage of
/*	the intermediate result from attr_table_read().
/*
/*	attr_table_free() destroys its argument.
/*
/*	attr_table_read() reads an attribute list from a byte stream
/*	and stores the intermediate result into a table that can be
/*	queried with attr_table_get().
/*
/*	attr_table_get() takes zero or more (name, value) scalar or array
/*	attribute arguments, and recovers the attribute values from the
/*	intermediate attribute table produced by attr_table_read().
/*
/*	Arguments:
/* .IP fp
/*	Stream to recover the attributes from.
/* .IP flags
/*	The bit-wise OR of zero or more of the following.
/* .RS
/* .IP ATTR_FLAG_MISSING
/*	Log a warning when the input attribute list terminates before all
/*	requested attributes are recovered. It is always an error when the
/*	input stream ends without the newline attribute list terminator.
/*	This flag has no effect with attr_table_read().
/* .IP ATTR_FLAG_EXTRA
/*	Log a warning and stop attribute recovery when the input stream
/*	contains multiple instances of an attribute.
/*	This flag has no effect with attr_table_get().
/* .IP ATTR_FLAG_NONE
/*	For convenience, this value requests none of the above.
/* .RE
/* .IP type
/*	The type determines the arguments that follow.
/* .RS
/* .IP "ATTR_TYPE_NUM (char *, int *)"
/*	This argument is followed by an attribute name and an integer pointer.
/*	This is used for recovering an integer attribute value.
/* .IP "ATTR_TYPE_STR (char *, VSTRING *)"
/*	This argument is followed by an attribute name and a VSTRING pointer.
/*	This is used for recovering a string attribute value.
/* .IP "ATTR_TYPE_NUM_ARRAY (char *, INTV *)"
/*	This argument is followed by an attribute name and an INTV pointer.
/*	This is used for recovering an integer array attribute value.
/* .IP "ATTR_TYPE_NUM_ARRAY (char *, ARGV *)"
/*	This argument is followed by an attribute name and an ARGV pointer.
/*	This is used for recovering a string array attribute value.
/* .IP ATTR_TYPE_END
/*	This terminates the requested attribute list.
/* .RE
/* DIAGNOSTICS
/*	The result value from attr_table_read() and from attr_table_get()
/*	is the number of attributes that were successfully recovered from
/*	the input stream (an array-valued attribute counts as one attribute).
/*
/*	Panic: interface violation. All system call errors are fatal.
/* SEE ALSO
/*	attr_print(3) send attributes over byte stream.
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
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

/* Utility library. */

#include <msg.h>
#include <htable.h>
#include <mymalloc.h>
#include <vstring.h>
#include <vstream.h>
#include <vstring_vstream.h>
#include <argv.h>
#include <intv.h>
#include <attr.h>

/* Application-specific. */

#define STR(x)	vstring_str(x)
#define LEN(x)	VSTRING_LEN(x)

static VSTRING *base64_buf;
static VSTRING *plain_buf;

/* attr_table_create - create attribute table */

ATTR_TABLE *attr_table_create(int size)
{
    return (htable_create(size));
}

/* attr_table_free - destroy attribute table */

void    attr_table_free(ATTR_TABLE *table)
{
    htable_free(table, myfree);
}

/* attr_table_read - read attribute stream into table */

int     attr_table_read(ATTR_TABLE *table, int flags, VSTREAM *stream)
{
    char   *attr_value;
    int     attr_count;
    int     ch;

    if (base64_buf == 0) {
	base64_buf = vstring_alloc(10);
	plain_buf = vstring_alloc(10);
    }
    for (attr_count = 0; /* void */ ; attr_count++) {

	/*
	 * Unexpected end-of-file is always an error.
	 */
	if ((ch = vstring_get_nonl(base64_buf, stream)) == VSTREAM_EOF) {
	    msg_warn("unexpected EOF while reading attributes from %s",
		     VSTREAM_PATH(stream));
	    return (attr_count);
	}

	/*
	 * A legitimate end of attribute list is OK.
	 */
	if (LEN(base64_buf) == 0)
	    return (attr_count);

	/*
	 * Split into name and value, but keep the ':' separator so that we
	 * can distinguish between no value or a zero-length value; decode
	 * the name but keep the value in one piece, so that we can process
	 * array-valued attributes.
	 */
	if ((attr_value = strchr(STR(base64_buf), ':')) != 0)
	    *attr_value = 0;
	if (BASE64_DECODE(plain_buf, STR(base64_buf), attr_value ?
		   (attr_value - STR(base64_buf)) : LEN(base64_buf)) == 0) {
	    msg_warn("malformed base64 data from %s: %.100s",
		     VSTREAM_PATH(stream), STR(base64_buf));
	    return (attr_count);
	}
	if (attr_value != 0)
	    *attr_value = ':';
	else
	    attr_value = "";

	/*
	 * Stop if there are multiple instances of the same attribute name
	 * and extra attributes are to be treated as an error. We can
	 * remember only one instance.
	 */
	if (htable_locate(table, STR(plain_buf)) != 0) {
	    if (flags & ATTR_FLAG_EXTRA) {
		msg_warn("multiple instances of attribute %s from %s",
			 STR(plain_buf), VSTREAM_PATH(stream));
		return (attr_count);
	    }
	} else {
	    htable_enter(table, STR(plain_buf), mystrdup(attr_value));
	}
    }
}

/* attr_conv_string - convert attribute value field to string */

static int attr_conv_string(char **src, VSTRING *plain_buf,
			            const char *attr_name)
{
    char   *myname = "attr_table_get";
    extern int var_line_limit;
    int     limit = var_line_limit * 5 / 4;
    char   *cp = *src;
    int     len = 0;
    int     ch;

    for (;;) {
	if ((ch = *(unsigned char *) cp) == 0)
	    break;
	cp++;
	if (ch == ':')
	    break;
	len++;
	if (len > limit) {
	    msg_warn("%s: string length > %d characters in attribute %s",
		     myname, limit, attr_name);
	    return (-1);
	}
    }
    if (BASE64_DECODE(plain_buf, *src, len) == 0) {
	msg_warn("%s: malformed base64 data in attribute %s: %.*s",
		 myname, attr_name, len > 100 ? 100 : len, *src);
	return (-1);
    }
    if (msg_verbose)
	msg_info("%s: name %s value %s", myname, attr_name, STR(plain_buf));

    *src = cp;
    return (ch);
}

/* attr_conv_number - convert attribute value field to number */

static int attr_conv_number(char **src, unsigned *dst, VSTRING *plain_buf,
			            const char *attr_name)
{
    char   *myname = "attr_table_get";
    char    junk = 0;
    int     ch;

    if ((ch = attr_conv_string(src, plain_buf, attr_name)) < 0)
	return (-1);
    if (sscanf(STR(plain_buf), "%u%c", dst, &junk) != 1 || junk != 0) {
	msg_warn("%s: malformed numerical data in attribute %s: %.100s",
		 myname, attr_name, STR(plain_buf));
	return (-1);
    }
    return (ch);
}

/* attr_table_vget - recover attributes from table */

int     attr_table_vget(ATTR_TABLE *attr, int flags, va_list ap)
{
    char   *myname = "attr_table_get";
    int     attr_count;
    char   *attr_name;
    char   *attr_value;
    int     attr_type;
    int    *number;
    VSTRING *string;
    INTV   *number_array;
    ARGV   *string_array;
    unsigned num_val;
    int     ch;

    if (base64_buf == 0) {
	base64_buf = vstring_alloc(10);
	plain_buf = vstring_alloc(10);
    }

    /*
     * Iterate over all (type, name, value) triples.
     */
    for (attr_count = 0; /* void */ ; attr_count++) {

	/*
	 * Determine the next attribute name on the caller's wish list.
	 */
	attr_type = va_arg(ap, int);
	if (attr_type == ATTR_TYPE_END)
	    return (attr_count);
	attr_name = va_arg(ap, char *);

	/*
	 * Look up the attribute value. Peel off, but keep, the separator
	 * between name and value.
	 */
	if ((attr_value = htable_find(attr, attr_name)) == 0) {
	    if (attr_type != ATTR_TYPE_END
		&& (flags & ATTR_FLAG_MISSING) != 0)
		msg_warn("%s: missing attribute %s", myname, attr_name);
	    return (attr_count);
	}
	if ((ch = *(unsigned char *) attr_value) != 0)
	    attr_value++;

	/*
	 * Do the requested conversion. If the target attribute is a
	 * non-array type, disallow sending a multi-valued attribute, and
	 * disallow sending no value. If the target attribute is an array
	 * type, allow the sender to send a zero-element array (i.e. no value
	 * at all). XXX Need to impose a bound on the number of array
	 * elements.
	 */
	switch (attr_type) {
	case ATTR_TYPE_NUM:
	    if (ch != ':') {
		msg_warn("%s: missing value for attribute %s",
			 myname, attr_name);
		return (attr_count);
	    }
	    number = va_arg(ap, int *);
	    if ((ch = attr_conv_number(&attr_value, number, plain_buf, attr_name)) < 0)
		return (attr_count);
	    if (ch != '\0') {
		msg_warn("%s: too many values for attribute %s",
			 myname, attr_name);
		return (attr_count);
	    }
	    break;
	case ATTR_TYPE_STR:
	    if (ch != ':') {
		msg_warn("%s: missing value for attribute %s",
			 myname, attr_name);
		return (attr_count);
	    }
	    string = va_arg(ap, VSTRING *);
	    if ((ch = attr_conv_string(&attr_value, string, attr_name)) < 0)
		return (attr_count);
	    if (ch != '\0') {
		msg_warn("%s: too many values for attribute %s",
			 myname, attr_name);
		return (attr_count);
	    }
	    break;
	case ATTR_TYPE_NUM_ARRAY:
	    number_array = va_arg(ap, INTV *);
	    while (ch != '\0') {
		if ((ch = attr_conv_number(&attr_value, &num_val, plain_buf, attr_name)) < 0)
		    return (attr_count);
		intv_add(number_array, 1, num_val);
	    }
	    break;
	case ATTR_TYPE_STR_ARRAY:
	    string_array = va_arg(ap, ARGV *);
	    while (ch != '\0') {
		if ((ch = attr_conv_string(&attr_value, plain_buf, attr_name)) < 0)
		    return (attr_count);
		argv_add(string_array, STR(plain_buf), (char *) 0);
	    }
	    break;
	default:
	    msg_panic("%s: unknown type code: %d", myname, attr_type);
	}
    }
}

/* attr_table_get - recover attributes from table */

int     attr_table_get(ATTR_TABLE *attr, int flags,...)
{
    va_list ap;
    int     ret;

    va_start(ap, flags);
    ret = attr_table_vget(attr, flags, ap);
    va_end(ap);

    return (ret);
}

#ifdef TEST

 /*
  * Proof of concept test program.  Mirror image of the attr_scan test
  * program.
  */
#include <msg_vstream.h>

int     var_line_limit = 2048;

int     main(int unused_argc, char **used_argv)
{
    ATTR_TABLE *attr;
    INTV   *intv = intv_alloc(1);
    ARGV   *argv = argv_alloc(1);
    VSTRING *str_val = vstring_alloc(1);
    int     int_val;
    int     ret;
    int     i;

    msg_verbose = 1;
    msg_vstream_init(used_argv[0], VSTREAM_ERR);
    attr = attr_table_create(1);
    if (attr_table_read(attr, ATTR_FLAG_EXTRA, VSTREAM_IN) > 0
	&& (ret = attr_table_get(attr,
				 ATTR_FLAG_MISSING,
				 ATTR_TYPE_NUM, ATTR_NAME_NUM, &int_val,
				 ATTR_TYPE_STR, ATTR_NAME_STR, str_val,
			     ATTR_TYPE_NUM_ARRAY, ATTR_NAME_NUM_ARRAY, intv,
			     ATTR_TYPE_STR_ARRAY, ATTR_NAME_STR_ARRAY, argv,
				 ATTR_TYPE_END)) == 4) {
	vstream_printf("%s %d\n", ATTR_NAME_NUM, int_val);
	vstream_printf("%s %s\n", ATTR_NAME_STR, STR(str_val));
	vstream_printf("%s", ATTR_NAME_NUM_ARRAY);
	for (i = 0; i < intv->intc; i++)
	    vstream_printf(" %d", intv->intv[i]);
	vstream_printf("\n");
	vstream_printf("%s", ATTR_NAME_STR_ARRAY);
	for (i = 0; i < argv->argc; i++)
	    vstream_printf(" %s", argv->argv[i]);
	vstream_printf("\n");
    } else {
	vstream_printf("return: %d\n", ret);
    }
    if (vstream_fflush(VSTREAM_OUT) != 0)
	msg_fatal("write error: %m");

    attr_table_free(attr);
    intv_free(intv);
    argv_free(argv);
    vstring_free(str_val);

    return (0);
}

#endif
