/*++
/* NAME
/*	attr_print 3
/* SUMMARY
/*	send attributes over byte stream
/* SYNOPSIS
/*	#include <attr.h>
/*
/*	int	attr_print(fp, flags, type, name, ...)
/*	VSTREAM	fp;
/*	int	flags;
/*	int	type;
/*	char	*name;
/*
/*	int	attr_vprint(fp, flags, ap)
/*	VSTREAM	fp;
/*	int	flags;
/*	va_list	ap;
/* DESCRIPTION
/*	attr_print() takes zero or more (name, value) simple attributes
/*	or (name, count, value) list attributes, and converts its input
/*	to a byte stream that can be recovered with attr_scan(). The stream
/*	is not flushed.
/*
/*	attr_vprint() provides an alternate interface that is convenient
/*	for calling from within variadoc functions.
/*
/*	Attributes are sent in the requested order as specified with the
/*	attr_print() argument list. This routine satisfies the formatting
/*	rules as outlined in attr_scan(3).
/*
/*	Arguments:
/* .IP fp
/*	Stream to write the result to.
/* .IP flags
/*	The bit-wise OR of zero or more of the following.
/* .RS
/* .IP ATTR_FLAG_MORE
/*	After sending the requested attributes, leave the output stream in
/*	a state that is usable for more attribute sending operations on
/*	the same output attribute list.
/*	By default, attr_print() automatically appends an attribute list
/*	terminator when it has sent the last requested attribute.
/* .RE
/* .IP type
/*	The type determines the arguments that follow.
/* .RS
/* .IP "ATTR_TYPE_NUM (char *, int)"
/*	This argument is followed by an attribute name and an integer.
/* .IP "ATTR_TYPE_STR (char *, char *)"
/*	This argument is followed by an attribute name and a null-terminated
/*	string.
/* .IP "ATTR_TYPE_NUM_ARRAY (char *, int, int *)"
/*	This argument is followed by an attribute name, an integer array
/*	element count, and a pointer to integer.
/* .IP "ATTR_TYPE_NUM_ARRAY (char *, int, char **)"
/*	This argument is followed by an attribute name, an integer array
/*	element count, and a pointer to a null-terminated array of
/*	null-terminated strings.
/* .IP "ATTR_TYPE_HASH (HTABLE *)"
/*	The content of the hash table is sent as a sequence of string-valued
/*	attributes with names equal to the hash table lookup key.
/* .IP ATTR_TYPE_END
/*	This terminates the attribute list.
/* .RE
/* DIAGNOSTICS
/*	The result value is 0 in case of success, VSTREAM_EOF in case
/*	of trouble.
/*
/*	Panic: interface violation. All system call errors are fatal.
/* SEE ALSO
/*	attr_scan(3) recover attributes from byte stream
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

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <vstream.h>
#include <htable.h>
#include <attr.h>

/* attr_fprintf - encode attribute information on the fly */

static void PRINTFLIKE(2, 3) attr_fprintf(VSTREAM *fp, const char *format,...)
{
    va_list ap;

    va_start(ap, format);
    vstream_vfprintf(fp, format, ap);
    va_end(ap);
}

/* attr_vprint - send attribute list to stream */

int     attr_vprint(VSTREAM *fp, int flags, va_list ap)
{
    const char *myname = "attr_print";
    int     attr_type;
    char   *attr_name;
    unsigned int_val;
    char   *str_val;
    char  **cpp_val;
    unsigned *ip_val;
    int     count_val;
    int     i;
    HTABLE_INFO **ht_info_list;
    HTABLE_INFO **ht;

    /*
     * Sanity check.
     */
    if (flags & ~ATTR_FLAG_ALL)
	msg_panic("%s: bad flags: 0x%x",
		  myname, flags);

    /*
     * Iterate over all (type, name, value) triples, and produce output on
     * the fly.
     */
    while ((attr_type = va_arg(ap, int)) != ATTR_TYPE_END) {
	switch (attr_type) {
	case ATTR_TYPE_NUM:
	    attr_name = va_arg(ap, char *);
	    attr_fprintf(fp, "%s", attr_name);
	    int_val = va_arg(ap, int);
	    attr_fprintf(fp, ":%u", (unsigned) int_val);
	    if (msg_verbose)
		msg_info("send attr name %s value %u", attr_name, int_val);
	    break;
	case ATTR_TYPE_STR:
	    attr_name = va_arg(ap, char *);
	    attr_fprintf(fp, "%s", attr_name);
	    str_val = va_arg(ap, char *);
	    attr_fprintf(fp, ":%s", str_val);
	    if (msg_verbose)
		msg_info("send attr name %s value %s", attr_name, str_val);
	    break;
	case ATTR_TYPE_NUM_ARRAY:
	    attr_name = va_arg(ap, char *);
	    attr_fprintf(fp, "%s", attr_name);
	    ip_val = va_arg(ap, int *);
	    count_val = va_arg(ap, int);
	    for (i = 0; i < count_val; i++)
		attr_fprintf(fp, ":%u", (unsigned) *ip_val++);
	    if (msg_verbose)
		msg_info("send attr name %s values %d", attr_name, count_val);
	    break;
	case ATTR_TYPE_STR_ARRAY:
	    attr_name = va_arg(ap, char *);
	    attr_fprintf(fp, "%s", attr_name);
	    cpp_val = va_arg(ap, char **);
	    count_val = va_arg(ap, int);
	    for (i = 0; i < count_val; i++) {
		str_val = *cpp_val++;
		attr_fprintf(fp, ":%s", str_val);
	    }
	    if (msg_verbose)
		msg_info("send attr name %s values %d", attr_name, count_val);
	    break;
	case ATTR_TYPE_HASH:
	    ht_info_list = htable_list(va_arg(ap, HTABLE *));
	    for (ht = ht_info_list; *ht; ht++) {
		attr_fprintf(fp, "%s:%s", ht[0]->key, ht[0]->value);
		if (msg_verbose)
		    msg_info("send attr name %s value %s",
			     ht[0]->key, ht[0]->value);
		if (ht[1] != 0)
		    VSTREAM_PUTC('\n', fp);
	    }
	    myfree((char *) ht_info_list);
	    break;
	default:
	    msg_panic("%s: unknown type code: %d", myname, attr_type);
	}
	VSTREAM_PUTC('\n', fp);
    }
    if ((flags & ATTR_FLAG_MORE) == 0)
	VSTREAM_PUTC('\n', fp);
    return (vstream_ferror(fp));
}

int     attr_print(VSTREAM *fp, int flags,...)
{
    va_list ap;
    int     ret;

    va_start(ap, flags);
    ret = attr_vprint(fp, flags, ap);
    va_end(ap);
    return (ret);
}

#ifdef TEST

 /*
  * Proof of concept test program.  Mirror image of the attr_scan test
  * program.
  */
#include <msg_vstream.h>

int     main(int argc, char **argv)
{
    static int int_array[] = {0, 1, 2, 3, 4, 5, 6, 7};
    static char *str_array[] = {"a", "b", "c", "d", "e", "f", "g", "h"};
    HTABLE *table = htable_create(1);

    msg_vstream_init(argv[0], VSTREAM_ERR);
    htable_enter(table, "foo-name", "foo-value");
    htable_enter(table, "bar-name", "bar-value");
    attr_print(VSTREAM_OUT, ATTR_FLAG_NONE,
	       ATTR_TYPE_NUM, ATTR_NAME_NUM, 4711,
	       ATTR_TYPE_STR, ATTR_NAME_STR, "whoopee",
	       ATTR_TYPE_NUM_ARRAY, ATTR_NAME_NUM_ARRAY,
	       int_array, sizeof(int_array) / sizeof(int_array[0]),
	       ATTR_TYPE_STR_ARRAY, ATTR_NAME_STR_ARRAY,
	       str_array, sizeof(str_array) / sizeof(str_array[0]),
	       ATTR_TYPE_HASH, table,
	       ATTR_TYPE_END);
    attr_print(VSTREAM_OUT, ATTR_FLAG_NONE,
	       ATTR_TYPE_NUM, ATTR_NAME_NUM, 4711,
	       ATTR_TYPE_STR, ATTR_NAME_STR, "whoopee",
	       ATTR_TYPE_NUM_ARRAY, ATTR_NAME_NUM_ARRAY,
	       int_array, sizeof(int_array) / sizeof(int_array[0]),
	       ATTR_TYPE_STR_ARRAY, ATTR_NAME_STR_ARRAY,
	       str_array, sizeof(str_array) / sizeof(str_array[0]),
	       ATTR_TYPE_END);
    if (vstream_fflush(VSTREAM_OUT) != 0)
	msg_fatal("write error: %m");
    return (0);
}

#endif
