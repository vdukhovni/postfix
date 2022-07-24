/*++
/* NAME
/*	dict_stream 3
/* SUMMARY
/*
/* SYNOPSIS
/*	#include <dict.h>
/*
/*	VSTREAM *dict_stream_open(
/*	const char *dict_type,
/*	const char *mapname,
/*	int	open_flags,
/*	int	dict_flags,
/*	struct stat * st,
/*	VSTRING **why)
/* DESCRIPTION
/*	dict_stream_open() opens a dictionary, which can be specified
/*	as a file name, or as inline text enclosed with {}. If successful,
/*	dict_stream_open() returns a non-null VSTREAM pointer. Otherwise,
/*	it returns an error text through the why argument, allocating
/*	storage for the error text if the why argument points to a
/*	null pointer.
/*
/*	When the dictionary file is specified inline, dict_stream_open()
/*	removes the outer {} from the mapname value, and removes leading
/*	or trailing comma or whitespace from the result. It then expects
/*	to find zero or more rules enclosed in {}, separated by comma
/*	and/or whitespace. dict_stream() writes each rule as one text
/*	line to an in-memory stream, without its enclosing {} and without
/*	leading or trailing whitespace. The result value is a VSTREAM
/*	pointer for the in-memory stream that can be read as a regular
/*	file.
/* .sp
/*	inline-file = "{" 0*(0*wsp-comma rule-spec) 0*wsp-comma "}"
/* .sp
/*	rule-spec = "{" 0*wsp rule-text 0*wsp "}"
/* .sp
/*	rule-text = any text containing zero or more balanced {}
/* .sp
/*	wsp-comma = wsp | ","
/* .sp
/*	wsp = whitespace
/*
/*	Arguments:
/* .IP dict_type
/* .IP open_flags
/* .IP dict_flags
/*	The same as with dict_open(3).
/* .IP mapname
/*	Pathname of a file with dictionary content, or inline dictionary
/*	content as specified above.
/* .IP st
/*	File metadata with the file owner, or fake metadata with the
/*	real UID and GID of the dict_stream_open() caller. This is 
/*	used for "taint" tracking (zero=trusted, non-zero=untrusted).
/* IP why
/*	Pointer to pointer to error message storage. dict_stream_open()
/*	updates this storage when reporting an error, and allocates
/*	memory if why points to a null pointer.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	Google, Inc.
/*	111 8th Avenue
/*	New York, NY 10011, USA
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>

 /*
  * Utility library.
  */
#include <dict.h>
#include <msg.h>
#include <mymalloc.h>
#include <stringops.h>
#include <vstring.h>

#define STR(x) vstring_str(x)
#define LEN(x) VSTRING_LEN(x)

/* dict_inline_to_multiline - convert inline map spec to multiline text */

static char *dict_inline_to_multiline(VSTRING *vp, const char *mapname)
{
    char   *saved_name = mystrdup(mapname);
    char   *bp = saved_name;
    char   *cp;
    char   *err = 0;

    VSTRING_RESET(vp);
    /* Strip the {} from the map "name". */
    err = extpar(&bp, CHARS_BRACE, EXTPAR_FLAG_NONE);
    /* Extract zero or more rules inside {}. */
    while (err == 0 && (cp = mystrtokq(&bp, CHARS_COMMA_SP, CHARS_BRACE)) != 0)
	if ((err = extpar(&cp, CHARS_BRACE, EXTPAR_FLAG_STRIP)) == 0)
	    /* Write rule to in-memory file. */
	    vstring_sprintf_append(vp, "%s\n", cp);
    VSTRING_TERMINATE(vp);
    myfree(saved_name);
    return (err);
}

/* dict_stream_open - open inline configuration or configuration file */

VSTREAM *dict_stream_open(const char *dict_type, const char *mapname,
			          int open_flags, int dict_flags,
			          struct stat * st, VSTRING **why)
{
    VSTRING *inline_buf = 0;
    VSTREAM *map_fp;
    char   *err = 0;

#define RETURN_0_WITH_REASON(...) do { \
	if (*why == 0) \
	    *why = vstring_alloc(100); \
	vstring_sprintf(*why, __VA_ARGS__); \
	if (inline_buf != 0) \
	   vstring_free(inline_buf); \
	if (err != 0) \
	    myfree(err); \
	return (0); \
    } while (0)

    if (mapname[0] == CHARS_BRACE[0]) {
	inline_buf = vstring_alloc(100);
	if ((err = dict_inline_to_multiline(inline_buf, mapname)) != 0)
	    RETURN_0_WITH_REASON("%s map: %s", dict_type, err);
	map_fp = vstream_memopen(inline_buf, O_RDONLY);
	vstream_control(map_fp, VSTREAM_CTL_OWN_VSTRING, VSTREAM_CTL_END);
	st->st_uid = getuid();			/* geteuid()? */
	st->st_gid = getgid();			/* getegid()? */
	return (map_fp);
    } else {
	if ((map_fp = vstream_fopen(mapname, open_flags, 0)) == 0)
	    RETURN_0_WITH_REASON("open %s: %m", mapname);
	if (fstat(vstream_fileno(map_fp), st) < 0)
	    msg_fatal("fstat %s: %m", mapname);
	return (map_fp);
    }
}
