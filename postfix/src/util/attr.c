/*++
/* NAME
/*	attr 3
/* SUMMARY
/*	simple attribute list manager
/* SYNOPSIS
/*	#include <attr.h>
/*
/*	void	attr_enter(table, flags, name, value, ..., (char *) 0)
/*	HTABLE	*table;
/*	int	flags;
/*	const char *name;
/*	const char *value;
/*
/*	int	attr_find(table, flags, name, value, ..., (char *) 0)
/*	HTABLE	*table;
/*	int	flags;
/*	const char *name;
/*	const char **vptr;
/* DESCRIPTION
/*	This module maintains open attribute lists of string-valued
/*	names and values. The module is built on top of the generic
/*	htable(3) hash table manager.
/*
/*	attr_enter() adds or updates zero or more attribute-value pairs.
/*	Both the name and the value are copied. The argument list is
/*	terminated with a null character pointer.
/*
/*	attr_find() looks up zero or more named attribute values. The
/*	result value is the number of attributes found; the search
/*	stops at the first error. The values are not copied out; it
/*	is up to the caller to save copies of values where needed.
/*
/*	Arguments:
/* .IP table
/*	Pointer to hash table.
/* .IP flags
/*	Bit-wise OR of the following:
/* .RS
/* .IP ATTR_FLAG_MISSING
/*      Log a warning when attr_find() cannot find an attribute. The
/*	search always terminates when a request cannot be satisfied.
/* .IP ATTR_FLAG_EXTRA
/*      Log a warning and return immediately when attr_enter() finds
/*	that a specified attribute already exists in the table.
/*      By default, attr_enter() replaces an existing attribute value
/*	by the specified one.
/* .IP ATTR_FLAG_NONE
/*      For convenience, this value requests none of the above.
/* .RE
/* .IP name
/*	Attribute name. Specify a null character pointer to terminate
/*	the argument list.
/* .IP value
/*	Attribute value. attr_enter() makes a copy.
/* .IP vptr
/*	Pointer to character pointer. attr_find() makes no copy of the
/*	value.
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
#include <htable.h>
#include <attr.h>

/* attr_enter - add or update attribute values */

void    attr_enter(HTABLE *table, int flags,...)
{
    const char *myname = "attr_enter";
    HTABLE_INFO *ht;
    va_list ap;
    const char *name;
    const char *value;

    va_start(ap, flags);
    while ((name = va_arg(ap, char *)) != 0) {
	value = va_arg(ap, char *);
	if ((ht = htable_locate(table, name)) != 0) {	/* replace */
	    if ((flags & ATTR_FLAG_EXTRA) != 0) {
		msg_warn("%s: duplicate attribute %s in table", myname, name);
		break;
	    }
	    myfree(ht->value);
	    ht->value = mystrdup(value);
	} else {				/* add attribute */
	    (void) htable_enter(table, name, mystrdup(value));
	}
    }
    va_end(ap);
}

/* attr_enter - look up attribute values */

int     attr_find(HTABLE *table, int flags,...)
{
    const char *myname = "attr_find";
    va_list ap;
    const char *name;
    const char **vptr;
    int     attr_count;

    va_start(ap, flags);
    for (attr_count = 0; (name = va_arg(ap, char *)) != 0; attr_count++) {
	vptr = va_arg(ap, const char **);
	if ((*vptr = htable_find(table, name)) == 0) {
	    if ((flags & ATTR_FLAG_MISSING) != 0)
		msg_warn("%s: missing attribute %s in table", myname, name);
	    break;
	}
    }
    return (attr_count);
}
