/*++
/* NAME
/*	vstream_attr 3
/* SUMMARY
/*	per-stream attribute list management
/* SYNOPSIS
/*	#include <vstream.h>
/*
/*	void	vstream_attr_set(stream, name, value, free_fn)
/*	VSTREAM	*stream;
/*	const char *name;
/*	char	*value;
/*	void	(*free_fn)(char *);
/*
/*	char	*vstream_attr_get(stream, name)
/*	VSTREAM	*stream;
/*	const char *name;
/*
/*	void	vstream_attr_unset(stream, name)
/*	VSTREAM	*stream;
/*	const char *name;
/* DESCRIPTION
/*	This module maintains an optional per-stream open attribute
/*	list for arbitrary binary values. It is in fact a convienience
/*	interface built on top of the binattr(3) module.
/*
/*	vstream_attr_set() adds or replaces the named attribute.
/*
/*	vstream_attr_get() looks up the named attribute. The result
/*	is the value stored with vstream_attr_set() or a null pointer
/*	when the requested information is not found.
/*
/*	vstream_attr_unset() removes the named attribute. This operation
/*	is undefined for attributes that do not exist.
/*
/*	Arguments:
/* .IP stream
/*	Open VSTREAM.
/* .IP name
/*	Attribute name, in the form of a null-terminated list.
/*	The name is copied.
/* .IP value
/*	Arbitrary binary value. The value is not copied.
/* .IP free_fn
/*	Null pointer, or pointer to function that destroys the value
/*	that was stored with vstream_attr_set().
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

#include <vstream.h>
#include <htable.h>

/* vstream_attr_set - add or replace per-stream attribute */

void    vstream_attr_set(VSTREAM *stream, const char *name, char *value, BINATTR_FREE_FN free_fn)
{
    if (stream->attr == 0)
	stream->attr = binattr_create(1);
    binattr_set(stream->attr, name, value, free_fn);
}

/* vstream_attr_get - look up per-stream attribute */

char   *vstream_attr_get(VSTREAM *stream, const char *name)
{
    if (stream->attr == 0)
	return (0);
    else
	return (binattr_get(stream->attr, name));
}

/* vstream_attr_unset - unset per-stream attribute */

void    vstream_attr_unset(VSTREAM *stream, const char *name)
{
    if (stream->attr)
	binattr_unset(stream->attr, name);
}
