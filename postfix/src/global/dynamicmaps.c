/*++
/* NAME
/*	dynamicmaps 3
/* SUMMARY
/*	load dictionaries dynamically
/* SYNOPSIS
/*	#include <dynamicmaps.h>
/*
/*	void dymap_init(const char *path)
/*
/*	ARGV *dymap_list(ARGV *map_names)
/* DESCRIPTION
/*	This module reads the dynamicmaps.cf file and performs
/*	run-time loading of Postfix dictionaries. Each dynamicmaps.cf
/*	entry specifies the name of a dictionary type, the pathname
/*	of a shared-library object, the name of an "open" function
/*	for access to individual dictionary entries, and optionally
/*	the name of a "mkmap" function for bulk-mode dictionary
/*	creation.
/*
/*	dymap_init() must be called at least once before any other
/*	functions in this module.  This function reads the specified
/*	configuration file which is in dynamicmaps.cf format, hooks
/*	itself into the dict_open(), dict_mapames(), and mkmap_open()
/*	functions, and may be called multiple times during a process
/*	lifetime, but only the last-read dynamicmaps content will be
/*	remembered.
/*
/*	dymap_list() appends to its argument the names of dictionary
/*	types available in dynamicmaps.cf.
/* SEE ALSO
/*	load_lib(3) low-level run-time linker adapter
/* DIAGNOSTICS
/*	Fatal errors: memory allocation problem, dictionary or
/*	dictionary function not available.  Panic: invalid use.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	LaMont Jones
/*	Hewlett-Packard Company
/*	3404 Harmony Road
/*	Fort Collins, CO 80528, USA
/*
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>
#include <sys/stat.h>
#include <string.h>

 /*
  * Utility library.
  */
#include <msg.h>
#include <mymalloc.h>
#include <argv.h>
#include <dict.h>
#include <load_lib.h>
#include <vstring.h>
#include <vstream.h>
#include <vstring_vstream.h>
#include <mvect.h>

 /*
  * Global library.
  */
#include <mkmap.h>
#include <dynamicmaps.h>

#ifdef USE_DYNAMIC_LIBS

 /*
  * Contents of one dynamicmaps.cf entry.
  */
typedef struct {
    const char *dict_type;		/* database type name */
    const char *soname;			/* shared-object file name */
    const char *open_name;		/* dict_xx_open() function name */
    const char *mkmap_name;		/* mkmap_xx_open() function name */
} DYMAP_INFO;

static DYMAP_INFO *dymap_info;
static DICT_OPEN_EXTEND_FN saved_dict_open_hook = 0;
static MKMAP_OPEN_EXTEND_FN saved_mkmap_open_hook = 0;
static DICT_MAPNAMES_EXTEND_FN saved_dict_mapnames_hook = 0;

#define STREQ(x, y) (strcmp((x), (y)) == 0)

/* dymap_find - find dynamicmaps.cf metadata */

static DYMAP_INFO *dymap_find(const char *dict_type)
{
    static const char myname[] = "dymap_find";
    DYMAP_INFO *dp;

    if (!dymap_info)
	msg_panic("%s: dlinfo==NULL", myname);

    for (dp = dymap_info; dp->dict_type; dp++) {
	if (STREQ(dp->dict_type, dict_type))
	    return dp;
    }
    return (0);
}

/* dymap_open_lookup - look up "dict_foo_open" function */

static DICT_OPEN_FN dymap_open_lookup(const char *dict_type)
{
    struct stat st;
    LIB_FN  fn[2];
    DICT_OPEN_FN dict_open_fn;
    DYMAP_INFO *dp;

    /*
     * Respect the hook nesting order.
     */
    if (saved_dict_open_hook != 0
	&& (dict_open_fn = saved_dict_open_hook(dict_type)) != 0)
	return (dict_open_fn);

    /*
     * Allow for graceful degradation when a database is unavailable. This
     * allows daemon processes to continue handling email with reduced
     * functionality.
     */
    if ((dp = dymap_find(dict_type)) == 0
	|| stat(dp->soname, &st) < 0
	|| dp->open_name == 0)
	return (0);
    if (st.st_uid != 0 || (st.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
	msg_warn("%s: file must be writable only by root", dp->soname);
	return (0);
    }
    fn[0].name = dp->open_name;
    fn[0].ptr = (void **) &dict_open_fn;
    fn[1].name = NULL;
    load_library_symbols(dp->soname, fn, NULL);
    return (dict_open_fn);
}

/* dymap_mkmap_lookup - look up "mkmap_foo_open" function */

static MKMAP_OPEN_FN dymap_mkmap_lookup(const char *dict_type)
{
    struct stat st;
    LIB_FN  fn[2];
    MKMAP_OPEN_FN mkmap_open_fn;
    DYMAP_INFO *dp;

    /*
     * Respect the hook nesting order.
     */
    if (saved_mkmap_open_hook != 0
	&& (mkmap_open_fn = saved_mkmap_open_hook(dict_type)) != 0)
	return (mkmap_open_fn);

    /*
     * All errors are fatal. If we can't create the requetsed database, then
     * graceful degradation is not useful.
     */
    dp = dymap_find(dict_type);
    if (!dp)
	msg_fatal("unsupported dictionary type: %s. "
		  "Is the postfix-%s package installed?",
		  dict_type, dict_type);
    if (!dp->mkmap_name)
	msg_fatal("unsupported dictionary type: %s does not support "
		  "bulk-mode creation.", dict_type);
    if (stat(dp->soname, &st) < 0)
	msg_fatal("unsupported dictionary type: %s (%s not found). "
		  "Is the postfix-%s package installed?",
		  dict_type, dp->soname, dict_type);
    if (st.st_uid != 0 || (st.st_mode & (S_IWGRP | S_IWOTH)) != 0)
	msg_fatal("%s: file must be writable only by root", dp->soname);
    fn[0].name = dp->mkmap_name;
    fn[0].ptr = (void **) &mkmap_open_fn;
    fn[1].name = NULL;
    load_library_symbols(dp->soname, fn, NULL);
    return (mkmap_open_fn);
}

/* dymap_list - enumerate dynamically-linked database type names */

static ARGV *dymap_list(ARGV *map_names)
{
    static const char myname[] = "dymap_list";
    DYMAP_INFO *dp;

    if (!dymap_info)
	msg_panic("%s: dlinfo==NULL", myname);

    /*
     * Respect the hook nesting order.
     */
    if (saved_dict_mapnames_hook != 0)
	map_names = saved_dict_mapnames_hook(map_names);

    if (map_names == 0)
	map_names = argv_alloc(2);
    for (dp = dymap_info; dp->dict_type; dp++) {
	argv_add(map_names, dp->dict_type, ARGV_END);
    }
    return (map_names);
}

/* dymap_init - initialize dictionary type to soname etc. mapping */

void    dymap_init(const char *path)
{
    VSTREAM *conf_fp;
    VSTRING *buf;
    char   *cp;
    ARGV   *argv;
    static MVECT vector;
    int     nelm = 0;
    int     linenum = 0;
    static int hooks_done = 0;
    struct stat st;

    if (dymap_info != 0)
	mvect_free(&vector);

    dymap_info =
	(DYMAP_INFO *) mvect_alloc(&vector, sizeof(DYMAP_INFO), 3, 0, 0);

    /* Silently ignore missing dynamic maps file. */
    if ((conf_fp = vstream_fopen(path, O_RDONLY, 0)) != 0) {
	if (fstat(vstream_fileno(conf_fp), &st) < 0)
	    msg_fatal("%s: fstat failed; %m", path);
	if (st.st_uid != 0 || (st.st_mode & (S_IWGRP | S_IWOTH)) != 0)
	    msg_fatal("%s: file must be writable only by root", path);
	buf = vstring_alloc(100);
	while (vstring_get_nonl(buf, conf_fp) != VSTREAM_EOF) {
	    cp = vstring_str(buf);
	    linenum++;
	    if (*cp == '#' || *cp == '\0')
		continue;
	    argv = argv_split(cp, " \t");
	    if (argv->argc != 3 && argv->argc != 4) {
		msg_fatal("%s: Expected \"dict_type .so-name open-function"
			  " [mkmap-function]\" at line %d", path, linenum);
	    }
	    if (STREQ(argv->argv[0], "*")) {
		msg_warn("%s: wildcard dynamic map entry no longer supported.",
			 path);
		continue;
	    }
	    if (argv->argv[1][0] != '/') {
		msg_fatal("%s: .so name must begin with a \"/\" at line %d",
			  path, linenum);
	    }
	    if (nelm >= vector.nelm) {
		dymap_info = (DYMAP_INFO *) mvect_realloc(&vector, vector.nelm + 3);
	    }
	    dymap_info[nelm].dict_type = mystrdup(argv->argv[0]);
	    dymap_info[nelm].soname = mystrdup(argv->argv[1]);
	    dymap_info[nelm].open_name = mystrdup(argv->argv[2]);
	    if (argv->argc == 4)
		dymap_info[nelm].mkmap_name = mystrdup(argv->argv[3]);
	    else
		dymap_info[nelm].mkmap_name = NULL;
	    nelm++;
	    argv_free(argv);
	    if (hooks_done == 0) {
		hooks_done = 1;
		saved_dict_open_hook = dict_open_extend(dymap_open_lookup);
		saved_mkmap_open_hook = mkmap_open_extend(dymap_mkmap_lookup);
		saved_dict_mapnames_hook = dict_mapnames_extend(dymap_list);
	    }
	}
	vstring_free(buf);
	vstream_fclose(conf_fp);
    }
    if (nelm >= vector.nelm) {
	dymap_info = (DYMAP_INFO *) mvect_realloc(&vector, vector.nelm + 1);
    }
    dymap_info[nelm].dict_type = NULL;
    dymap_info[nelm].soname = NULL;
    dymap_info[nelm].open_name = NULL;
    dymap_info[nelm].mkmap_name = NULL;
}

#endif
