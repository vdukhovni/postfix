/*++
/* NAME
/*	dict_file_to_buf 3
/* SUMMARY
/*	include content from file as blob
/* SYNOPSIS
/*	#include <dict.h>
/*
/*	VSTRING	*dict_file_to_buf(
/*	DICT	*dict,
/*	const char *pathname)
/*
/*	VSTRING	*dict_file_to_b64(
/*	DICT	*dict,
/*	const char *pathname)
/*
/*	VSTRING	*dict_file_from_b64(
/*	DICT	*dict,
/*	const char *value)
/*
/*	char	*dict_file_get_error(
/*	DICT	*dict)
/*
/*	void	dict_file_purge_buffers(
/*	DICT	*dict)
/* DESCRIPTION
/*	dict_file_to_buf() reads the content of the specified file.
/*	It returns a pointer to a buffer which is owned by the DICT,
/*	or a null pointer in case of error.
/*
/*	dict_file_to_b64() reads the content of the specified file,
/*	and converts the result to base64.
/*	It returns a pointer to a buffer which is owned by the DICT,
/*	or a null pointer in case of error.
/*
/*	dict_file_from_b64() converts a value from base64. It returns
/*	a pointer to a buffer which is owned by the DICT, or a null
/*	pointer in case of error.
/*
/*	dict_file_purge_buffers() disposes of dict_file-related
/*	memory that are associated with this DICT.
/*
/*	dict_file_get_error() should be called only after error;
/*	it returns a desciption of the problem. Storage is owned
/*	by the caller.
/* DIAGNOSTICS
/*	In case of error the result value is a null pointer, and
/*	an error description can be retrieved with dict_file_get_error().
/*	The storage is owned by the caller.
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
#include <sys/stat.h>
#include <string.h>

 /*
  * Utility library.
  */
#include <base64_code.h>
#include <dict.h>
#include <msg.h>
#include <mymalloc.h>
#include <vstream.h>
#include <vstring.h>

 /*
  * SLMs.
  */
#define STR(x) vstring_str(x)
#define LEN(x) VSTRING_LEN(x)

/* dict_file_to_buf - read a file into a buffer */

VSTRING *dict_file_to_buf(DICT *dict, const char *pathname)
{
    struct stat st;
    VSTREAM *fp;

    /* dict_file_to_buf() postcondition: dict->file_buf exists. */
    if (dict->file_buf == 0)
	dict->file_buf = vstring_alloc(100);

    if ((fp = vstream_fopen(pathname, O_RDONLY, 0)) == 0
	|| fstat(vstream_fileno(fp), &st) < 0) {
	vstring_sprintf(dict->file_buf, "open %s: %m", pathname);
	if (fp)
	    vstream_fclose(fp);
	return (0);
    }
    VSTRING_RESET(dict->file_buf);
    VSTRING_SPACE(dict->file_buf, st.st_size);
    if (vstream_fread(fp, STR(dict->file_buf), st.st_size) != st.st_size) {
	vstring_sprintf(dict->file_buf, "read %s: %m", pathname);
	vstream_fclose(fp);
	return (0);
    }
    (void) vstream_fclose(fp);
    VSTRING_AT_OFFSET(dict->file_buf, st.st_size);
    VSTRING_TERMINATE(dict->file_buf);
    return (dict->file_buf);
}

/* dict_file_to_b64 - read a file into a base64-encoded buffer */

VSTRING *dict_file_to_b64(DICT *dict, const char *pathname)
{
    ssize_t helper;

    if (dict_file_to_buf(dict, pathname) == 0)
	return (0);
    if (dict->file_b64 == 0)
	dict->file_b64 = vstring_alloc(100);
    helper = (VSTRING_LEN(dict->file_buf) + 2) / 3;
    if (helper > SSIZE_T_MAX / 4) {
	vstring_sprintf(dict->file_buf, "file too large: %s", pathname);
	return (0);
    }
    VSTRING_RESET(dict->file_b64);
    VSTRING_SPACE(dict->file_b64, helper * 4);
    return (base64_encode(dict->file_b64, STR(dict->file_buf),
			  LEN(dict->file_buf)));
}

/* dict_file_from_b64 - convert value from base64 */

VSTRING *dict_file_from_b64(DICT *dict, const char *value)
{
    ssize_t helper;
    VSTRING *result;

    if (dict->file_buf == 0)
	dict->file_buf = vstring_alloc(100);
    helper = strlen(value) / 4;
    VSTRING_RESET(dict->file_buf);
    VSTRING_SPACE(dict->file_buf, helper * 3);
    result = base64_decode(dict->file_buf, value, strlen(value));
    if (result == 0)
	vstring_sprintf(dict->file_buf, "malformed BASE64 value: %.30s", value);
    return (result);
}

/* dict_file_get_error - return error text */

char   *dict_file_get_error(DICT *dict)
{
    if (dict->file_buf == 0)
	msg_panic("dict_file_get_error: no buffer");
    return (mystrdup(STR(dict->file_buf)));
}

/* dict_file_purge_buffers - purge file buffers */

void    dict_file_purge_buffers(DICT *dict)
{
    if (dict->file_buf) {
	vstring_free(dict->file_buf);
	dict->file_buf = 0;
    }
    if (dict->file_b64) {
	vstring_free(dict->file_b64);
	dict->file_b64 = 0;
    }
}
