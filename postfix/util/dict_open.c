/*++
/* NAME
/*	dict_open 3
/* SUMMARY
/*	low-level dictionary interface
/* SYNOPSIS
/*	#include <dict.h>
/*
/*	DICT	*dict_open(dict_spec, flags)
/*	const char *dict_spec;
/*	int	flags;
/*
/*	DICT	*dict_open3(dict_type, dict_name, flags)
/*	const char *dict_type;
/*	const char *dict_name;
/*	int	flags;
/*
/*	void	dict_put(dict, key, value)
/*	DICT	*dict;
/*	const char *key;
/*	const char *value;
/*
/*	char	*dict_get(dict, key)
/*	DICT	*dict;
/*	const char *key;
/*
/*	void	dict_close(dict)
/*	DICT	*dict;
/* DESCRIPTION
/*	This module implements a low-level interface to multiple
/*	physical dictionary types.
/*
/*	dict_open() takes a type:name pair that specifies a dictionary type
/*	and dictionary name, opens the dictionary, and returns a dictionary
/*	handle.  The \fIflags\fR arguments are as in open(2). The dictionary
/*	types are as follows:
/* .IP environ
/*	The process environment array. The \fIdict_name\fR argument is ignored.
/* .IP dbm
/*	DBM file.
/* .IP hash
/*	Berkeley DB file in hash format.
/* .IP btree
/*	Berkeley DB file in btree format.
/* .IP nis
/*	NIS map. Only read access is supported.
/* .IP nisplus
/*	NIS+ map. Only read access is supported.
/* .IP netinfo
/*	NetInfo table. Only read access is supported.
/* .IP ldap
/*	LDAP ("light-weight" directory access protocol) database access.
/*	The support is still incomplete.
/* .PP
/*	dict_open3() takes separate arguments for dictionary type and
/*	name, but otherwise performs the same functions as dict_open().
/*
/*	dict_get() retrieves the value stored in the named dictionary
/*	under the given key. A null pointer means the value was not found.
/*	This routine does not manipulate any locks.
/*
/*	dict_put() stores the specified key and value into the named
/*	dictionary. This routine does not manipulate any locks.
/*
/*	dict_close() closes the specified dictionary and cleans up the
/*	associated data structures.
/* DIAGNOSTICS
/*	Fatal error: open error, unsupported dictionary type, attempt to
/*	update non-writable dictionary.
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

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

/* Utility library. */

#include <argv.h>
#include <mymalloc.h>
#include <msg.h>
#include <dict.h>
#include <dict_env.h>
#include <dict_dbm.h>
#include <dict_db.h>
#include <dict_nis.h>
#include <dict_nisplus.h>
#include <dict_ni.h>
#include <dict_ldap.h>
#include <stringops.h>
#include <split_at.h>

 /*
  * lookup table for available map types.
  */
typedef struct {
    char   *type;
    struct DICT *(*open) (const char *, int);
} DICT_OPEN_INFO;

static DICT_OPEN_INFO dict_open_info[] = {
    "environ", dict_env_open,
#ifdef HAS_DBM
    "dbm", dict_dbm_open,
#endif
#ifdef HAS_DB
    "hash", dict_hash_open,
    "btree", dict_btree_open,
#endif
#ifdef HAS_NIS
    "nis", dict_nis_open,
#endif
#ifdef HAS_NISPLUS
    "nisplus", dict_nisplus_open,
#endif
#ifdef HAS_NETINFO
    "netinfo", dict_ni_open,
#endif
#ifdef HAS_LDAP
    "ldap", dict_ldap_open,
#endif
    0,
};

/* dict_open - open dictionary */

DICT   *dict_open(const char *dict_spec, int flags)
{
    char   *saved_dict_spec = mystrdup(dict_spec);
    char   *dict_name;
    DICT   *dict;

    if ((dict_name = split_at(saved_dict_spec, ':')) == 0)
	msg_fatal("open dictionary: need \"type:name\" form: %s", dict_spec);

    dict = dict_open3(saved_dict_spec, dict_name, flags);
    myfree(saved_dict_spec);
    return (dict);
}


/* dict_open3 - open dictionary */

DICT   *dict_open3(const char *dict_type, const char *dict_name, int flags)
{
    char   *myname = "dict_open";
    DICT_OPEN_INFO *dp;
    DICT   *dict = 0;

    for (dp = dict_open_info; dp->type; dp++) {
	if (strcasecmp(dp->type, dict_type) == 0) {
	    if ((dict = dp->open(dict_name, flags)) == 0)
		msg_fatal("opening %s:%s %m", dict_type, dict_name);
	    if (msg_verbose)
		msg_info("%s: %s:%s", myname, dict_type, dict_name);
	    break;
	}
    }
    if (dp->type == 0)
	msg_fatal("unsupported dictionary type: %s", dict_type);
    return (dict);
}

#ifdef TEST

 /*
  * Proof-of-concept test program. Create, update or read a database. When
  * the input is a name=value pair, the database is updated, otherwise the
  * program assumes that the input specifies a lookup key and prints the
  * corresponding value.
  */

/* System library. */

#include <stdlib.h>
#include <fcntl.h>

/* Utility library. */

#include "vstring.h"
#include "vstream.h"
#include "msg_vstream.h"
#include "vstring_vstream.h"

main(int argc, char **argv)
{
    VSTRING *keybuf = vstring_alloc(1);
    DICT   *dict;
    char   *dict_name;
    int     dict_flags;
    char   *key;
    const char *value;

    msg_vstream_init(argv[0], VSTREAM_ERR);
    if (argc != 3)
	msg_fatal("usage: %s type:file read|write|create", argv[0]);
    if (strcasecmp(argv[2], "create") == 0)
	dict_flags = O_CREAT | O_RDWR | O_TRUNC;
    else if (strcasecmp(argv[2], "write") == 0)
	dict_flags = O_RDWR;
    else if (strcasecmp(argv[2], "read") == 0)
	dict_flags = O_RDONLY;
    else
	msg_fatal("unknown access mode: %s", argv[2]);
    dict_name = argv[1];
    dict = dict_open(dict_name, dict_flags);
    while (vstring_fgets_nonl(keybuf, VSTREAM_IN)) {
	if ((key = strtok(vstring_str(keybuf), " =")) == 0)
	    continue;
	if ((value = strtok((char *) 0, " =")) == 0) {
	    if ((value = dict_get(dict, key)) == 0) {
		vstream_printf("not found\n");
	    } else {
		vstream_printf("%s\n", value);
	    }
	} else {
	    dict_put(dict, key, value);
	}
	vstream_fflush(VSTREAM_OUT);
    }
    vstring_free(keybuf);
    dict_close(dict);
}

#endif
