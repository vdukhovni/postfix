/*++
/* NAME
/*	dict_inline 3
/* SUMMARY
/*	dictionary manager interface for inline table
/* SYNOPSIS
/*	#include <dict_inline.h>
/*
/*	DICT	*dict_inline_open(name, open_flags, dict_flags)
/*	const char *name;
/*	int	open_flags;
/*	int	dict_flags;
/* DESCRIPTION
/*	dict_inline_open() opens a read-only, in-memory table.
/*	Example: "\fBinline:{\fIkey_1=value_1, ..., key_n=value_n\fR}".
/*	The longer form with { key = value } allows values that
/*	contain whitespace or comma.
/* SEE ALSO
/*	dict(3) generic dictionary manager
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

#include <msg.h>
#include <mymalloc.h>
#include <htable.h>
#include <stringops.h>
#include <dict.h>
#include <dict_inline.h>

/* Application-specific. */

typedef struct {
    DICT    dict;			/* generic members */
    HTABLE *table;			/* lookup table */
    HTABLE_INFO **info;			/* for iterator */
    HTABLE_INFO **cursor;		/* ditto */
} DICT_INLINE;

/* dict_inline_lookup - search inline table */

static const char *dict_inline_lookup(DICT *dict, const char *name)
{
    DICT_INLINE *dict_inline = (DICT_INLINE *) dict;
    const char *result = 0;

    /*
     * Optionally fold the key.
     */
    if (dict->flags & DICT_FLAG_FOLD_FIX) {
	if (dict->fold_buf == 0)
	    dict->fold_buf = vstring_alloc(10);
	vstring_strcpy(dict->fold_buf, name);
	name = lowercase(vstring_str(dict->fold_buf));
    }

    /*
     * Look up the value.
     */
    result = htable_find(dict_inline->table, name);

    DICT_ERR_VAL_RETURN(dict, DICT_ERR_NONE, result);
}

/* dict_inline_sequence - traverse the dictionary */

static int dict_inline_sequence(DICT *dict, int function,
				        const char **key, const char **value)
{
    const char *myname = "dict_inline_sequence";
    DICT_INLINE *dict_inline = (DICT_INLINE *) dict;

    /*
     * Determine and execute the seek function.
     */
    switch (function) {
    case DICT_SEQ_FUN_FIRST:
	if (dict_inline->info == 0)
	    dict_inline->info = htable_list(dict_inline->table);
	dict_inline->cursor = dict_inline->info;
	break;
    case DICT_SEQ_FUN_NEXT:
	if (dict_inline->cursor[0])
	    dict_inline->cursor += 1;
	break;
    default:
	msg_panic("%s: invalid function: %d", myname, function);
    }

    /*
     * Return the entry under the cursor.
     */
    if (dict_inline->cursor[0]) {
	*key = dict_inline->cursor[0]->key;
	*value = dict_inline->cursor[0]->value;
	DICT_ERR_VAL_RETURN(dict, DICT_ERR_NONE, DICT_STAT_SUCCESS);
    } else {
	*key = 0;
	*value = 0;
	DICT_ERR_VAL_RETURN(dict, DICT_ERR_NONE, DICT_STAT_FAIL);
    }
}

/* dict_inline_close - disassociate from inline table */

static void dict_inline_close(DICT *dict)
{
    DICT_INLINE *dict_inline = (DICT_INLINE *) dict;

    htable_free(dict_inline->table, myfree);
    if (dict_inline->info)
	myfree((void *) dict_inline->info);
    if (dict->fold_buf)
	vstring_free(dict->fold_buf);
    dict_free(dict);
}

/* dict_inline_open - open inline table */

DICT   *dict_inline_open(const char *name, int open_flags, int dict_flags)
{
    DICT_INLINE *dict_inline;
    char   *cp, *saved_name = 0;
    size_t  len;
    HTABLE *table = 0;
    char   *nameval, *vname, *value;
    const char *err = 0;
    char   *xperr = 0;

    /*
     * Clarity first. Let the optimizer worry about redundant code.
     */
#define DICT_INLINE_RETURN(x) do { \
	    DICT *__d = (x); \
	    if (saved_name != 0) \
		myfree(saved_name); \
	    if (xperr != 0) \
		myfree(xperr); \
	    if (table != 0) \
		htable_free(table, myfree); \
	    return (__d); \
	} while (0)

    /*
     * Sanity checks.
     */
    if (open_flags != O_RDONLY)
	DICT_INLINE_RETURN(dict_surrogate(DICT_TYPE_INLINE, name,
					  open_flags, dict_flags,
				  "%s:%s map requires O_RDONLY access mode",
					  DICT_TYPE_INLINE, name));

    /*
     * Parse the table into its constituent name=value pairs.
     */
    if ((len = balpar(name, CHARS_BRACE)) == 0 || name[len] != 0
	|| *(cp = saved_name = mystrndup(name + 1, len - 2)) == 0)
	DICT_INLINE_RETURN(dict_surrogate(DICT_TYPE_INLINE, name,
					  open_flags, dict_flags,
					  "bad syntax: \"%s:%s\"; "
					  "need \"%s:{name=value...}\"",
					  DICT_TYPE_INLINE, name,
					  DICT_TYPE_INLINE));

    table = htable_create(5);
    while ((nameval = mystrtokq(&cp, CHARS_COMMA_SP, CHARS_BRACE)) != 0) {
	if ((nameval[0] != CHARS_BRACE[0]
	     || (err = xperr = extpar(&nameval, CHARS_BRACE, EXTPAR_FLAG_STRIP)) == 0)
	    && (err = split_nameval(nameval, &vname, &value)) != 0)
	    break;
	(void) htable_enter(table, vname, mystrdup(value));
    }
    if (err != 0 || table->used == 0)
	DICT_INLINE_RETURN(dict_surrogate(DICT_TYPE_INLINE, name,
					  open_flags, dict_flags,
					  "%s: \"%s:%s\"; "
					  "need \"%s:{name=value...}\"",
					  err != 0 ? err : "empty table",
					  DICT_TYPE_INLINE, name,
					  DICT_TYPE_INLINE));

    /*
     * Bundle up the result.
     */
    dict_inline = (DICT_INLINE *)
	dict_alloc(DICT_TYPE_INLINE, name, sizeof(*dict_inline));
    dict_inline->dict.lookup = dict_inline_lookup;
    dict_inline->dict.sequence = dict_inline_sequence;
    dict_inline->dict.close = dict_inline_close;
    dict_inline->dict.flags = dict_flags | DICT_FLAG_FIXED;
    dict_inline->dict.owner.status = DICT_OWNER_TRUSTED;
    if (dict_flags & DICT_FLAG_FOLD_FIX)
	dict_inline->dict.fold_buf = vstring_alloc(10);
    dict_inline->info = 0;
    dict_inline->table = table;
    table = 0;
    DICT_INLINE_RETURN(DICT_DEBUG (&dict_inline->dict));
}
