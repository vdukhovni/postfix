/*++
/* NAME
/*	dict 3
/* SUMMARY
/*	dictionary manager, multi-line entry support
/* SYNOPSIS
/*	#include <dict_ml.h>
/*
/*	void	dict_ml_load_file(dict_name, path)
/*	const char *dict_name;
/*	const char *path;
/*
/*	void	dict_ml_load_fp(dict_name, fp)
/*	const char *dict_name;
/*	VSTREAM	*fp;
/* DESCRIPTION
/*	This module implements input routines for dictionaries
/*	with single-line and multi-line values.
/* .IP \(bu
/*	Single-line values are specified as "name = value".
/*	Like dict_load_file() and dict_load_fp(), leading and
/*	trailing whitespace is stripped from name and value.
/* .IP \(bu
/*	Multi-line values are specified as:
/*
/* .na
/* .nf
/* .in +4
/*	name = <<EOF
/*	text here...
/*	EOF
/* .in -4
/* .ad
/* .fi
/*
/*	Leading or trailing white space is not stripped from
/*	multi-line values.
/* .IP \(bu
/*	The following input is ignored outside "<<" context: empty
/*	lines, all whitespace lines, and lines whose first
/*	non-whitespace character is "#".
/* .PP
/*	dict_ml_load_file() enters (name, value) pairs from the
/*	specified file to the specified dictionary.
/*
/*	dict_ml_load_fp() reads (name, value) pairs from an open
/*	stream. It has the same semantics as dict_ml_load_file().
/* SEE ALSO
/*	dict(3)
/* DIAGNOSTICS
/*	Fatal errors: out of memory, malformed macro name, missing
/*	or mal-formed end marker.
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
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>
#include <string.h>

/* Utility library. */

#include <msg.h>
#include <vstring.h>
#include <vstream.h>
#include <vstring_vstream.h>
#include <iostuff.h>
#include <stringops.h>
#include <dict.h>
#include <dict_ml.h>

#define STR(x) vstring_str(x)

/* dict_ml_load_file - load table from file */

void    dict_ml_load_file(const char *dict_name, const char *path)
{
    VSTREAM *fp;
    struct stat st;
    time_t  before;
    time_t  after;

    /*
     * Read the file again if it is hot. This may result in reading a partial
     * parameter name when a file changes in the middle of a read.
     */
    for (before = time((time_t *) 0); /* see below */ ; before = after) {
	if ((fp = vstream_fopen(path, O_RDONLY, 0)) == 0)
	    msg_fatal("open %s: %m", path);
	dict_ml_load_fp(dict_name, fp);
	if (fstat(vstream_fileno(fp), &st) < 0)
	    msg_fatal("fstat %s: %m", path);
	if (vstream_ferror(fp) || vstream_fclose(fp))
	    msg_fatal("read %s: %m", path);
	after = time((time_t *) 0);
	if (st.st_mtime < before - 1 || st.st_mtime > after)
	    break;
	if (msg_verbose)
	    msg_info("pausing to let %s cool down", path);
	doze(300000);
    }
}

/* dict_ml_load_fp - load table from stream */

void    dict_ml_load_fp(const char *dict_name, VSTREAM *fp)
{
    VSTRING *line_buf;
    char   *member_name;
    VSTRING *multi_line_buf = 0;
    VSTRING *saved_member_name = 0;
    VSTRING *saved_end_marker = 0;
    char   *value;
    int     lineno;
    const char *err;
    char   *cp;

    line_buf = vstring_alloc(100);
    lineno = 1;
    while (vstring_get_nonl(line_buf, fp) > 0) {
	lineno++;
	cp = STR(line_buf) + strspn(STR(line_buf), " \t\n\v\f\r");
	if (*cp == 0 || *cp == '#')
	    continue;
	if ((err = split_nameval(STR(line_buf), &member_name, &value)) != 0)
	    msg_fatal("%s, line %d: %s: \"%s\"",
		      VSTREAM_PATH(fp), lineno, err, STR(line_buf));
	if (value[0] == '<' && value[1] == '<') {
	    value += 2;
	    while (ISSPACE(*value))
		value++;
	    if (*value == 0)
		msg_fatal("%s, line %d: missing end marker after <<",
			  VSTREAM_PATH(fp), lineno);
	    if (!ISALNUM(*value))
		msg_fatal("%s, line %d: malformed end marker after <<",
			  VSTREAM_PATH(fp), lineno);
	    if (multi_line_buf == 0) {
		saved_member_name = vstring_alloc(100);
		saved_end_marker = vstring_alloc(100);
		multi_line_buf = vstring_alloc(100);
	    } else
		VSTRING_RESET(multi_line_buf);
	    vstring_strcpy(saved_member_name, member_name);
	    vstring_strcpy(saved_end_marker, value);
	    while (vstring_get_nonl(line_buf, fp) > 0) {
		lineno++;
		if (strcmp(STR(line_buf), STR(saved_end_marker)) == 0)
		    break;
		if (VSTRING_LEN(multi_line_buf) > 0)
		    vstring_strcat(multi_line_buf, "\n");
		vstring_strcat(multi_line_buf, STR(line_buf));
	    }
	    if (vstream_feof(fp))
		msg_fatal("%s, line %d: missing \"%s\" end marker",
			  VSTREAM_PATH(fp), lineno, value);
	    member_name = STR(saved_member_name);
	    value = STR(multi_line_buf);
	}
	dict_update(dict_name, member_name, value);
    }
    vstring_free(line_buf);
    if (multi_line_buf) {
	vstring_free(saved_member_name);
	vstring_free(saved_end_marker);
	vstring_free(multi_line_buf);
    }
}
