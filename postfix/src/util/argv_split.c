/*++
/* NAME
/*	argv_split 3
/* SUMMARY
/*	string array utilities
/* SYNOPSIS
/*	#include <argv.h>
/*
/*	ARGV	*argv_split(string, delim)
/*	const char *string;
/*	const char *delim;
/*
/*	ARGV	*argv_split_cw(string, delim, blame)
/*	const char *string;
/*	const char *delim;
/*	const char *blame;
/*
/*	ARGV	*argv_split_count(string, delim, count)
/*	const char *string;
/*	const char *delim;
/*	ssize_t	count;
/*
/*	ARGV	*argv_split_append(argv, string, delim)
/*	ARGV	*argv;
/*	const char *string;
/*	const char *delim;
/*
/*	ARGV	*argv_split_append_cw(argv, string, delim, blame)
/*	ARGV	*argv;
/*	const char *string;
/*	const char *delim;
/*	const char *blame;
/* DESCRIPTION
/*	argv_split() breaks up \fIstring\fR into tokens according
/*	to the delimiters specified in \fIdelim\fR. The result is
/*	a null-terminated string array.
/*
/*	argv_split_cw() is like argv_split(), but stops splitting
/*	input and logs a warning when it encounters text that looks
/*	like a trailing comment. The \fBblame\fR argument specifies
/*	context that is used in warning messages. Specify a null
/*	pointer to disable the trailing comment check.
/*
/*	argv_split_count() is like argv_split() but stops splitting
/*	input after at most \fIcount\fR -1 times and leaves the
/*	remainder, if any, in the last array element. It is an error
/*	to specify a count < 1.
/*
/*	argv_split_append() performs the same operation as argv_split(),
/*	but appends the result to an existing string array.
/*
/*	argv_split_append_cw() is like argv_split_append(), but
/*	stops splitting input and logs a warning when it encounters
/*	text that looks like a trailing comment. The \fBblame\fR
/*	argument specifies context that is used in warning messages.
/*	Specify a null pointer to disable the trailing comment
/*	check.
/* SEE ALSO
/*	mystrtok(), safe string splitter.
/* DIAGNOSTICS
/*	Fatal errors: memory allocation problem.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*
/*	Wietse Venema
/*--*/

/* System libraries. */

#include <sys_defs.h>
#include <string.h>

/* Application-specific. */

#include "mymalloc.h"
#include "stringops.h"
#include "argv.h"
#include "msg.h"

/* argv_split - ABI compatibility wrapper */

#undef argv_split

ARGV   *argv_split(const char *string, const char *delim)
{
    return (argv_split_cw(string, delim, (char *) 0));
}

/* argv_split_cw - split string into token array and complain about comment */

ARGV   *argv_split_cw(const char *string, const char *delim, const char *blame)
{
    ARGV   *argvp = argv_alloc(1);
    char   *saved_string = mystrdup(string);
    char   *bp = saved_string;
    char   *arg;

    while ((arg = mystrtok_cw(&bp, delim, blame)) != 0)
	argv_add(argvp, arg, (char *) 0);
    argv_terminate(argvp);
    myfree(saved_string);
    return (argvp);
}

/* argv_split_count - split string into token array */

ARGV   *argv_split_count(const char *string, const char *delim, ssize_t count)
{
    ARGV   *argvp = argv_alloc(1);
    char   *saved_string = mystrdup(string);
    char   *bp = saved_string;
    char   *arg;

    if (count < 1)
	msg_panic("argv_split_count: bad count: %ld", (long) count);
    while (count-- > 1 && (arg = mystrtok(&bp, delim)) != 0)
	argv_add(argvp, arg, (char *) 0);
    if (*bp)
	bp += strspn(bp, delim);
    if (*bp)
	argv_add(argvp, bp, (char *) 0);
    argv_terminate(argvp);
    myfree(saved_string);
    return (argvp);
}

/* argv_split_append - ABI compatibility wrapper */

#undef argv_split_append

ARGV   *argv_split_append(ARGV *argvp, const char *string, const char *delim)
{
    return (argv_split_append_cw(argvp, string, delim, (char *) 0));
}

/* argv_split_append_cw - split string into token array, append to array */

ARGV   *argv_split_append_cw(ARGV *argvp, const char *string, const char *delim,
			             const char *blame)
{
    char   *saved_string = mystrdup(string);
    char   *bp = saved_string;
    char   *arg;

    while ((arg = mystrtok_cw(&bp, delim, blame)) != 0)
	argv_add(argvp, arg, (char *) 0);
    argv_terminate(argvp);
    myfree(saved_string);
    return (argvp);
}
