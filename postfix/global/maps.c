/*++
/* NAME
/*	maps 3
/* SUMMARY
/*	multi-dictionary search
/* SYNOPSIS
/*	#include <maps.h>
/*
/*	MAPS	*maps_create(title, map_names)
/*	const char *title;
/*	const char *map_names;
/*
/*	const char *maps_find(maps, key)
/*	MAPS	*maps;
/*	const char *key;
/*
/*	void	maps_free(maps)
/*	MAPS	*maps;
/* DESCRIPTION
/*	This module implements multi-dictionary searches. it goes
/*	through the high-level dictionary interface and does file
/*	locking. Dictionaries are opened read-only, and in-memory
/*	dictionary instances are shared.
/*
/*	maps_create() takes list of type:name pairs and opens the
/*	named dictionaries.
/*	The result is a handle that must be specified along with all
/*	other maps_xxx() operations.
/*
/*	maps_find() searches the specified list of dictionaries
/*	in the specified order for the named key. The result is in
/*	memory that is overwritten upon each call.
/*
/*	maps_free() releases storage claimed by maps_create()
/*	and conveniently returns a null pointer.
/*
/*	Arguments:
/* .IP title
/*	String used for diagnostics. Typically one specifies the
/*	type of information stored in the lookup tables.
/* .IP map_names
/*	Null-terminated string with type:name dictionary specifications,
/*	separated by whitespace or commas.
/* .IP maps
/*	A result from maps_create().
/* .IP key
/*	Null-terminated string with a lookup key. Table lookup is case
/*	sensitive.
/* DIAGNOSTICS
/*	Panic: inappropriate use; fatal errors: out of memory, unable
/*	to open database.
/*
/*	maps_find() returns a null pointer when the requested
/*	information was not found. The global \fIdict_errno\fR
/*	variable indicates if the last lookup failed due to a problem.
/* BUGS
/*	The dictionary name space is flat, so dictionary names allocated
/*	by maps_create() may collide with dictionary names allocated by
/*	other methods.
/*
/*	This functionality could be implemented by allowing the user to
/*	specify dictionary search paths to dict_lookup() or dict_eval().
/*	However, that would either require that the dict(3) module adopts
/*	someone else's list notation syntax, or that the dict(3) module
/*	imposes syntax restrictions onto other software, neither of which
/*	is desirable.
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

#include <argv.h>
#include <mymalloc.h>
#include <msg.h>
#include <dict.h>
#include <stringops.h>
#include <split_at.h>

/* Global library. */

#include "config.h"
#include "maps.h"

/* maps_create - initialize */

MAPS   *maps_create(const char *title, const char *map_names)
{
    char   *myname = "maps_create";
    char   *temp = mystrdup(map_names);
    char   *bufp = temp;
    static char sep[] = " \t,\r\n";
    MAPS   *maps;
    DICT   *dict;
    char   *map_type_name;

    /*
     * Initialize.
     */
    maps = (MAPS *) mymalloc(sizeof(*maps));
    maps->title = mystrdup(title);
    maps->argv = argv_alloc(2);

    /*
     * For each specified type:name pair, either register a new dictionary,
     * or increment the reference count of an existing one.
     */
    while ((map_type_name = mystrtok(&bufp, sep)) != 0) {
	if (msg_verbose)
	    msg_info("%s: %s", myname, map_type_name);
	if ((dict = dict_handle(map_type_name)) == 0)
	    dict = dict_open(map_type_name, O_RDONLY, 0);
	dict_register(map_type_name, dict);
	argv_add(maps->argv, map_type_name, ARGV_END);
    }
    myfree(temp);
    argv_terminate(maps->argv);
    return (maps);
}

/* maps_find - search a list of dictionaries */

const char *maps_find(MAPS *maps, const char *name)
{
    char   *myname = "maps_find";
    char  **map_name;
    const char *expansion;

    for (map_name = maps->argv->argv; *map_name; map_name++) {
	if ((expansion = dict_lookup(*map_name, name)) != 0) {
	    if (msg_verbose)
		msg_info("%s: %s: %s = %s", myname, *map_name, name, expansion);
	    return (expansion);
	} else if (dict_errno != 0) {
	    break;
	}
    }
    if (msg_verbose)
	msg_info("%s: %s: %s", myname, name, dict_errno ?
		 "search aborted" : "not found");
    return (0);
}

/* maps_free - release storage */

MAPS   *maps_free(MAPS *maps)
{
    char  **map_name;

    for (map_name = maps->argv->argv; *map_name; map_name++) {
	if (msg_verbose)
	    msg_info("maps_free: %s", *map_name);
	dict_unregister(*map_name);
    }
    myfree(maps->title);
    argv_free(maps->argv);
    myfree((char *) maps);
    return (0);
}

#ifdef TEST

#include <vstring.h>
#include <vstream.h>
#include <vstring_vstream.h>

main(int argc, char **argv)
{
    VSTRING *buf = vstring_alloc(100);
    MAPS   *maps;
    const char *result;

    if (argc != 2)
	msg_fatal("usage: %s maps", argv[0]);
    msg_verbose = 2;
    maps = maps_create("whatever", argv[1]);

    while (vstring_fgets_nonl(buf, VSTREAM_IN)) {
	if ((result = maps_find(maps, vstring_str(buf))) != 0) {
	    vstream_printf("%s\n", result);
	} else if (dict_errno != 0) {
	    msg_fatal("lookup error: %m");
	} else {
	    vstream_printf("not found\n");
	}
	vstream_fflush(VSTREAM_OUT);
    }
    maps_free(maps);
    vstring_free(buf);
}

#endif
