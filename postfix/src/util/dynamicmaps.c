/*++
/* NAME
/*	dynamicmaps 3
/* SUMMARY
/*	load dictionaries dynamically
/* SYNOPSIS
/*	#include <dynamicmaps.h>
/*
/*	typedef void *(*dymap_mkmap_t) (const char *)
/*	typedef DICT *(*dymap_open_t) (const char *, int, int)
/*
/*	void dymap_init(const char *path)
/*
/*	ARGV *dymap_list(ARGV *map_names)
/*
/*	dymap_open_t dymap_get_open_fn(const char *dict_type)
/*
/*	dymap_mkmap_t dymap_get_mkmap_fn(const char *dict_type)
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
/*	configuration file which is in dynamicmaps.cf format, and
/*	may be called multiple times during a process lifetime.
/*
/*	dymap_list() appends to its argument the names of dictionary
/*	types available in dynamicmaps.cf.
/*
/*	dymap_get_open_fn() loads the specified dictionary and
/*	returns a function pointer to its "open" function.
/*
/*	dymap_get_mkmap_fn() loads the specified dictionary and
/*	returns a function pointer to its "mkmap" function.
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
#include <dynamicmaps.h>
#include <load_lib.h>
#include <vstring.h>
#include <vstream.h>
#include <vstring_vstream.h>
#include <mvect.h>

#ifdef USE_DYNAMIC_LIBS

 /*
  * Contents of one dynamicmaps.cf entry.
  */
typedef struct {
    const char *dict_type;
    const char *soname;
    const char *open_name;
    const char *mkmap_name;
} DICT_DL_INFO;

static DICT_DL_INFO *dict_dlinfo;

#define STREQ(x, y) (strcmp((x), (y)) == 0)

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

    if (dict_dlinfo != 0)
	mvect_free(&vector);

    dict_dlinfo =
	(DICT_DL_INFO *) mvect_alloc(&vector, sizeof(DICT_DL_INFO), 3, 0, 0);

    /* Silently ignore missing dynamic maps file. */
    if ((conf_fp = vstream_fopen(path, O_RDONLY, 0)) != 0) {
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
		dict_dlinfo = (DICT_DL_INFO *) mvect_realloc(&vector, vector.nelm + 3);
	    }
	    dict_dlinfo[nelm].dict_type = mystrdup(argv->argv[0]);
	    dict_dlinfo[nelm].soname = mystrdup(argv->argv[1]);
	    dict_dlinfo[nelm].open_name = mystrdup(argv->argv[2]);
	    if (argv->argc == 4)
		dict_dlinfo[nelm].mkmap_name = mystrdup(argv->argv[3]);
	    else
		dict_dlinfo[nelm].mkmap_name = NULL;
	    nelm++;
	    argv_free(argv);
	}
	vstring_free(buf);
	vstream_fclose(conf_fp);
    }
    if (nelm >= vector.nelm) {
	dict_dlinfo = (DICT_DL_INFO *) mvect_realloc(&vector, vector.nelm + 1);
    }
    dict_dlinfo[nelm].dict_type = NULL;
    dict_dlinfo[nelm].soname = NULL;
    dict_dlinfo[nelm].open_name = NULL;
    dict_dlinfo[nelm].mkmap_name = NULL;
}

/* dymap_list - enumerate dynamically-linked database type names */

ARGV   *dymap_list(ARGV *map_names)
{
    static const char myname[] = "dymap_list";
    DICT_DL_INFO *dl;

    if (!dict_dlinfo)
	msg_panic("%s: dlinfo==NULL", myname);
    if (map_names == 0)
	map_names = argv_alloc(2);
    for (dl = dict_dlinfo; dl->dict_type; dl++) {
	argv_add(map_names, dl->dict_type, ARGV_END);
    }
    return (map_names);
}

/* dymap_find - find dynamically-linked database metadata */

static DICT_DL_INFO *dymap_find(const char *dict_type)
{
    static const char myname[] = "dymap_find";
    DICT_DL_INFO *dp;

    if (!dict_dlinfo)
	msg_panic("%s: dlinfo==NULL", myname);

    for (dp = dict_dlinfo; dp->dict_type; dp++) {
	if (STREQ(dp->dict_type, dict_type))
	    return dp;
    }
    return (0);
}

/* dymap_get_open_fn - look up "dict_foo_open" function */

dymap_open_t dymap_get_open_fn(const char *dict_type)
{
    struct stat st;
    LIB_FN  fn[2];
    dymap_open_t open_fn;
    DICT_DL_INFO *dl;

    if ((dl = dymap_find(dict_type)) == 0
	|| stat(dl->soname, &st) < 0
	|| dl->open_name == 0)
	return (0);
    fn[0].name = dl->open_name;
    fn[0].ptr = (void **) &open_fn;
    fn[1].name = NULL;
    load_library_symbols(dl->soname, fn, NULL);
    return (open_fn);
}

/* dymap_get_mkmap_fn - look up "mkmap_foo_open" function */

dymap_mkmap_t dymap_get_mkmap_fn(const char *dict_type)
{
    struct stat st;
    LIB_FN  fn[2];
    dymap_mkmap_t mkmap_fn;
    DICT_DL_INFO *dl;

    dl = dymap_find(dict_type);
    if (!dl)
	msg_fatal("unsupported dictionary type: %s. "
		  "Is the postfix-%s package installed?",
		  dict_type, dict_type);
    if (stat(dl->soname, &st) < 0) {
	msg_fatal("unsupported dictionary type: %s (%s not found). "
		  "Is the postfix-%s package installed?",
		  dict_type, dl->soname, dict_type);
    }
    if (!dl->mkmap_name)
	msg_fatal("unsupported dictionary type: %s does not support "
		  "bulk-mode creation.", dict_type);
    fn[0].name = dl->mkmap_name;
    fn[0].ptr = (void **) &mkmap_fn;
    fn[1].name = NULL;
    load_library_symbols(dl->soname, fn, NULL);
    return (mkmap_fn);
}

#endif
