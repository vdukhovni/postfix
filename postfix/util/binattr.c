/*++
/* NAME
/*	binattr 3
/* SUMMARY
/*	binary-valued attribute list manager
/* SYNOPSIS
/*	#include <binattr.h>
/*
/*	typedef struct {
/* .in +4
/*		char    *key;
/*		char    *value;
/*		/* private fields... */
/* .in -4
/*	} BINATTR_ENTRY;
/*
/*	BINATTR *binattr_create(size)
/*	int	size;
/*
/*	char	*binattr_get(table, name)
/*	BINATTR *table;
/*	const char *name;
/*
/*	BINATTR_ENTRY *binattr_set(table, name, value, free_fn)
/*	BINATTR *table;
/*	const char *name;
/*	void	(*free_fn)(char *);
/*
/*	void	binattr_unset(table, name)
/*	BINATTR *table;
/*	const char *name;
/*
/*	void	binattr_free(table)
/*	BINATTR *table;
/* DESCRIPTION
/*	This module maintains open attribute lists of arbitrary
/*	binary values. Each attribute has a string-valued name.
/*	The caller specifies the memory management policy for
/*	attribute values, which can be arbitrary binary data.
/*	Attribute lists grow on demand as entries are added.
/*
/*	binattr_create() creates a table of the specified size,
/*	or whatever the underlying code deems suitable.
/*
/*	binattr_get() looks up the named attribute in the specified
/*	attribute list, and returns the value that was stored with
/*	binattr_set(). The result is a null pointer when the requested
/*	information is not found.
/*
/*	binattr_set() adds or replaces the named entry in the specified
/*	attribute list.  This function expects as attribute value a
/*	generic character pointer. Use proper casts when storing data of
/*	a different type, or the result will be undefined.
/*
/*	binattr_unset() removes the named attribute from the specified
/*	attribute list. This operation is undefined for non-existing
/*	attributes.
/*
/*	binattr_free() destroys the specified attribute list including
/*	the information that is stored in it.
/*
/*	Arguments:
/* .IP table
/*	Open attribute list created with binattr_create.
/* .IP name
/*	Attribute name. Attribute names are unique within a list.
/* .IP value
/*	The value stored under the named attribute.
/* .IP free_fn
/*	Pointer to function that destroys the value stored under the
/*	named attribute, or a null pointer.
/* DIAGNOSTICS
/*	Panic: interface violations and internal consistency problems.
/*	Fatal errors: out of memory.
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

/* Utility library. */

#include <mymalloc.h>
#include <htable.h>
#include "binattr.h"

/* binattr_create - create binary attribute list */

BINATTR *binattr_create(int size)
{
    return (htable_create(size));
}

/* binattr_free - destroy binary attribute list */

void    binattr_free(BINATTR *table)
{
    HTABLE_INFO **info;
    HTABLE_INFO **ht;
    BINATTR_INFO *h;

    for (ht = info = htable_list(table); *ht != 0; ht++) {
	h = (BINATTR_INFO *) (ht[0]->value);
	if (h->free_fn)
	    h->free_fn(h->value);
    }
    myfree((char *) info);
}

/* binattr_get - look up binary attribute */

char   *binattr_get(BINATTR *table, const char *name)
{
    BINATTR_INFO *info;

    return ((info = (BINATTR_INFO *) htable_find(table, name)) == 0 ? 0 : info->value);
}

/* binattr_set - set or replace binary attribute */

BINATTR_INFO *binattr_set(BINATTR *table, const char *name, char *value, BINATTR_FREE_FN free_fn)
{
    BINATTR_INFO *info;

    if ((info = (BINATTR_INFO *) htable_find(table, name)) != 0) {
	if (info->value != value) {
	    if (info->free_fn)
		info->free_fn(info->value);
	    info->value = value;
	}
	info->free_fn = free_fn;
    } else {
	info = (BINATTR_INFO *) mymalloc(sizeof(*info));
	info->value = value;
	info->free_fn = free_fn;
	htable_enter(table, name, (char *) info);
    }
    return (info);
}

/* binattr_free_callback - destructor callback */

static void binattr_free_callback(char *ptr)
{
    BINATTR_INFO *info = (BINATTR_INFO *) ptr;

    if (info->free_fn)
	info->free_fn(info->value);
}

/* binattr_unset - remove attribute */

void    binattr_unset(BINATTR *table, const char *name)
{
    htable_delete(table, name, binattr_free_callback);
}
