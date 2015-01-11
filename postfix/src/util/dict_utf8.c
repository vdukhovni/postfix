/*++
/* NAME
/*	dict_utf8 3
/* SUMMARY
/*	dictionary UTF-8 helpers
/* SYNOPSIS
/*	#include <dict.h>
/*
/*	DICT	*dict_utf8_activate(
/*	DICT	*dict)
/*
/*	char	*dict_utf8_check_fold(
/*	DICT	*dict,
/*	const char *string,
/*	CONST_CHAR_STAR *err,
/*	int	fold_flag)
/*
/*	int	dict_utf8_check(
/*	const char *string,
/*	CONST_CHAR_STAR *err)
/* DESCRIPTION
/*	dict_utf8_activate() wraps a dictionary's lookup/update/delete
/*	methods with code that enforces UTF-8 checks on keys and
/*	values, and that logs a warning when incorrect UTF-8 is
/*	encountered. The original dictionary handle becomes invalid.
/*
/*	The wrapper code enforces a policy that maximizes application
/*	robustness (it avoids the need for new error-handling code
/*	paths in application code).  Attempts to store non-UTF-8
/*	keys or values are skipped while reporting a non-error
/*	status, attempts to look up or delete non-UTF-8 keys are
/*	skipped while reporting a non-error status, and attempts
/*	to look up a non-UTF-8 value are flagged while reporting a
/*	configuration error.
/*
/*	The dict_utf8_check* functions may be invoked to perform
/*	UTF-8 validity checks when util_utf8_enable is non-zero and
/*	DICT_FLAG_UTF8_ENABLE is set. Otherwise both functions
/*	always report success.
/*
/*	dict_utf8_check_fold() optionally folds a string, and checks
/*	it for UTF-8 validity. The result is the possibly-folded
/*	string, or a null pointer in case of error.
/*
/*	dict_utf8_check() checks a string for UTF-8 validity. The
/*	result is zero in case of error.
/* BUGS
/*	dict_utf8_activate() does not nest.
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

 /*
  * System library.
  */
#include <sys_defs.h>
#include <string.h>

 /*
  * Utility library.
  */
#include <msg.h>
#include <stringops.h>
#include <dict.h>
#include <mymalloc.h>
#include <msg.h>

 /*
  * Backed-up accessor function pointers.
  */
typedef struct {
    const char *(*lookup) (struct DICT *, const char *);
    int     (*update) (struct DICT *, const char *, const char *);
    int     (*delete) (struct DICT *, const char *);
} DICT_UTF8_BACKUP;

 /*
  * The goal is to maximize robustness: bad UTF-8 should not appear in keys,
  * because those are derived from controlled inputs, and values should be
  * printable before they are stored. But if we failed to check something
  * then it should not result in fatal errors and thus open up the system for
  * a denial-of-service attack.
  * 
  * Proposed over-all policy: skip attempts to store invalid UTF-8 lookup keys
  * or values. Rationale: some storage may not permit malformed UTF-8. This
  * maximizes program robustness. If we get an invalid lookup result, report
  * a configuration error.
  * 
  * LOOKUP
  * 
  * If the key is invalid, log a warning and skip the request. Rationale: the
  * item cannot exist.
  * 
  * If the lookup result is invalid, log a warning and return a configuration
  * error.
  * 
  * UPDATE
  * 
  * If the key is invalid, then log a warning and skip the request. Rationale:
  * the item cannot exist.
  * 
  * If the value is invalid, log a warning and skip the request. Rationale:
  * storage may not permit malformed UTF-8. This maximizes program
  * robustness.
  * 
  * DELETE
  * 
  * If the key is invalid, then skip the request. Rationale: the item cannot
  * exist.
  */

/* dict_utf8_check_fold - casefold or validate string */

char   *dict_utf8_check_fold(DICT *dict, const char *string,
			             CONST_CHAR_STAR *err)
{
    int     fold_flag = (dict->flags & DICT_FLAG_FOLD_ANY);

    /*
     * Casefold and implicitly validate UTF-8.
     */
    if (fold_flag != 0 && (fold_flag & (dict->flags & DICT_FLAG_FIXED) ?
			   DICT_FLAG_FOLD_FIX : DICT_FLAG_FOLD_MUL)) {
	if (dict->fold_buf == 0)
	    dict->fold_buf = vstring_alloc(10);
	return (casefold(dict->fold_buf, string, err));
    }

    /*
     * Validate UTF-8 without casefolding.
     */
    if (!allascii(string) && valid_utf8_string(string, strlen(string)) == 0) {
	if (err)
	    *err = "malformed UTF-8 or invalid codepoint";
	return (0);
    }
    return ((char *) string);
}

/* dict_utf8_check validate UTF-8 string */

int     dict_utf8_check(const char *string, CONST_CHAR_STAR *err)
{
    if (!allascii(string) && valid_utf8_string(string, strlen(string)) == 0) {
	if (err)
	    *err = "malformed UTF-8 or invalid codepoint";
	return (0);
    }
    return (1);
}

/* dict_utf8_lookup - UTF-8 lookup method wrapper */

static const char *dict_utf8_lookup(DICT *dict, const char *key)
{
    DICT_UTF8_BACKUP *backup;
    const char *utf8_err;
    const char *fold_res;
    const char *value;
    int     saved_flags;

    /*
     * Validate and optionally fold the key, and if invalid skip the request.
     */
    if ((fold_res = dict_utf8_check_fold(dict, key, &utf8_err)) == 0) {
	msg_warn("%s:%s: non-UTF-8 key \"%s\": %s",
		 dict->type, dict->name, key, utf8_err);
	dict->error = DICT_ERR_NONE;
	return (0);
    }

    /*
     * Proxy the request with casefolding turned off.
     */
    saved_flags = (dict->flags & DICT_FLAG_FOLD_ANY);
    dict->flags &= ~DICT_FLAG_FOLD_ANY;
    backup = (void *) dict + dict->size;
    value = backup->lookup(dict, fold_res);
    dict->flags |= saved_flags;

    /*
     * Validate the result, and if invalid fail the request.
     */
    if (value != 0 && dict_utf8_check(value, &utf8_err) == 0) {
	msg_warn("%s:%s: key \"%s\": non-UTF-8 value \"%s\": %s",
		 dict->type, dict->name, key, value, utf8_err);
	dict->error = DICT_ERR_CONFIG;
	return (0);
    } else {
	return (value);
    }
}

/* dict_utf8_update - UTF-8 update method wrapper */

static int dict_utf8_update(DICT *dict, const char *key, const char *value)
{
    DICT_UTF8_BACKUP *backup;
    const char *utf8_err;
    const char *fold_res;
    int     saved_flags;
    int     status;

    /*
     * Validate or fold the key, and if invalid skip the request.
     */
    if ((fold_res = dict_utf8_check_fold(dict, key, &utf8_err)) == 0) {
	msg_warn("%s:%s: non-UTF-8 key \"%s\": %s",
		 dict->type, dict->name, key, utf8_err);
	dict->error = DICT_ERR_NONE;
	return (DICT_STAT_SUCCESS);
    }

    /*
     * Validate the value, and if invalid skip the request.
     */
    else if (dict_utf8_check(value, &utf8_err) == 0) {
	msg_warn("%s:%s: key \"%s\": non-UTF-8 value \"%s\": %s",
		 dict->type, dict->name, key, value, utf8_err);
	dict->error = DICT_ERR_NONE;
	return (DICT_STAT_SUCCESS);
    }

    /*
     * Proxy the request with casefolding turned off.
     */
    else {
	saved_flags = (dict->flags & DICT_FLAG_FOLD_ANY);
	dict->flags &= ~DICT_FLAG_FOLD_ANY;
	backup = (void *) dict + dict->size;
	status = backup->update(dict, fold_res, value);
	dict->flags |= saved_flags;
	return (status);
    }
}

/* dict_utf8_delete - UTF-8 delete method wrapper */

static int dict_utf8_delete(DICT *dict, const char *key)
{
    DICT_UTF8_BACKUP *backup;
    const char *utf8_err;
    const char *fold_res;
    int     saved_flags;
    int     status;

    /*
     * Validate and optionally fold the key, and if invalid skip the request.
     */
    if ((fold_res = dict_utf8_check_fold(dict, key, &utf8_err)) == 0) {
	msg_warn("%s:%s: non-UTF-8 key \"%s\": %s",
		 dict->type, dict->name, key, utf8_err);
	dict->error = DICT_ERR_NONE;
	return (DICT_STAT_SUCCESS);
    }

    /*
     * Proxy the request with casefolding turned off.
     */
    else {
	saved_flags = (dict->flags & DICT_FLAG_FOLD_ANY);
	dict->flags &= ~DICT_FLAG_FOLD_ANY;
	backup = (void *) dict + dict->size;
	status = backup->delete(dict, fold_res);
	dict->flags |= saved_flags;
	return (status);
    }
}

/* dict_utf8_activate - wrap a legacy dict object for UTF-8 processing */

DICT   *dict_utf8_activate(DICT *dict)
{
    DICT_UTF8_BACKUP *backup;

    /*
     * Sanity check.
     */
    if (dict->flags & DICT_FLAG_UTF8_ACTIVE)
	msg_panic("dict_utf8_activate: %s:%s is already encapsulated",
		  dict->type, dict->name);

    /*
     * Unlike dict_debug(3) we do not put a proxy dict object in front of the
     * encapsulated object, because then we would have to bidirectionally
     * propagate changes in the data members (errors, flags, jbuf, and so on)
     * between proxy object and encapsulated object.
     * 
     * Instead we append ourselves to the encapsulated dict object itself, and
     * redirect some function pointers. This approach does not yet generalize
     * to arbitrary levels of encapsulation. That is, it does not co-exist
     * with dict_debug(3) which is broken for the reasons stated above.
     */
    dict = myrealloc(dict, dict->size + sizeof(*backup));
    backup = (void *) dict + dict->size;

    /*
     * Interpose on the lookup/update/delete methods. It is a conscious
     * decision not to tinker with the iterator or destructor.
     */
    backup->lookup = dict->lookup;
    backup->update = dict->update;
    backup->delete = dict->delete;

    dict->lookup = dict_utf8_lookup;
    dict->update = dict_utf8_update;
    dict->delete = dict_utf8_delete;

    /*
     * Leave our mark. See sanity check above.
     */
    dict->flags |= DICT_FLAG_UTF8_ACTIVE;

    return (dict);
}
