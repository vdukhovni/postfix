/*++
/* NAME
/*	dict_utf8 3
/* SUMMARY
/*	dictionary UTF-8 helpers
/* SYNOPSIS
/*	#include <dict.h>
/*
/*	DICT	*dict_utf8_encapsulate(
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
/*	dict_utf8_encapsulate() wraps a dictionary's lookup/update/delete
/*	methods with code that enforces UTF-8 checks on keys and
/*	values, and that logs a warning when incorrect UTF-8 is
/*	encountered. The original dictionary handle becomes invalid.
/*
/*	The wrapper code enforces a policy that maximizes application
/*	robustness.  Attempts to store non-UTF-8 keys or values are
/*	skipped while reporting success, attempts to look up or
/*	delete non-UTF-8 keys are skipped while reporting success,
/*	and attempts to look up a non-UTF-8 value are flagged while
/*	reporting a configuration error.
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
    if (fold_flag != 0 && (fold_flag == (dict->flags & DICT_FLAG_FIXED) ?
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

static const char *dict_utf8_lookup(DICT *self, const char *key)
{
    DICT   *dict;
    const char *utf8_err;
    const char *fold_res;
    const char *value;

    /*
     * Validate and optionally fold the key, and if invalid skip the request.
     */
    if ((fold_res = dict_utf8_check_fold(self, key, &utf8_err)) == 0) {
	msg_warn("%s:%s: non-UTF-8 key \"%s\": %s",
		       self->type, self->name, key, utf8_err);
	self->error = DICT_ERR_NONE;
	return (0);
    }

    /*
     * Proxy the request.
     */
    dict = (void *) self - self->size;
    dict->flags = self->flags;
    value = dict->lookup(dict, fold_res);
    self->flags = dict->flags;
    self->error = dict->error;

    /*
     * Validate the result, and if invalid fail the request.
     */
    if (value != 0 && dict_utf8_check(value, &utf8_err) == 0) {
	msg_warn("%s:%s: key \"%s\": non-UTF-8 value \"%s\": %s",
		       self->type, self->name, key, value, utf8_err);
	self->error = DICT_ERR_CONFIG;
	return (0);
    } else {
	return (value);
    }
}

/* dict_utf8_update - UTF-8 update method wrapper */

static int dict_utf8_update(DICT *self, const char *key, const char *value)
{
    DICT   *dict;
    const char *utf8_err;
    const char *fold_res;
    int     status;

    /*
     * Validate or fold the key, and if invalid skip the request.
     */
    if ((fold_res = dict_utf8_check_fold(self, key, &utf8_err)) == 0) {
	msg_warn("%s:%s: non-UTF-8 key \"%s\": %s",
		       self->type, self->name, key, utf8_err);
	self->error = DICT_ERR_NONE;
	return (DICT_STAT_SUCCESS);
    }

    /*
     * Validate the value, and if invalid skip the request.
     */
    else if (dict_utf8_check(value, &utf8_err) == 0) {
	msg_warn("%s:%s: key \"%s\": non-UTF-8 value \"%s\": %s",
		       self->type, self->name, key, value, utf8_err);
	self->error = DICT_ERR_NONE;
	return (DICT_STAT_SUCCESS);
    }

    /*
     * Proxy the request.
     */
    else {
	dict = (void *) self - self->size;
	dict->flags = self->flags;
	status = dict->update(dict, fold_res, value);
	self->flags = dict->flags;
	self->error = dict->error;
	return (status);
    }
}

/* dict_utf8_delete - UTF-8 delete method wrapper */

static int dict_utf8_delete(DICT *self, const char *key)
{
    DICT   *dict;
    const char *utf8_err;
    const char *fold_res;
    int     status;

    /*
     * Validate and optionally fold the key, and if invalid skip the request.
     */
    if ((fold_res = dict_utf8_check_fold(self, key, &utf8_err)) == 0) {
	msg_warn("%s:%s: non-UTF-8 key \"%s\": %s",
		       self->type, self->name, key, utf8_err);
	self->error = DICT_ERR_NONE;
	return (DICT_STAT_SUCCESS);
    }

    /*
     * Proxy the request.
     */
    else {
	dict = (void *) self - self->size;
	dict->flags = self->flags;
	status = dict->delete(dict, fold_res);
	self->flags = dict->flags;
	self->error = dict->error;
	return (status);
    }
}

/* dict_utf8_close - dummy */

static void dict_utf8_close(DICT *self)
{
    DICT   *dict;

    /*
     * Destroy the dict object that we are appended to, and thereby destroy
     * ourselves.
     */
    dict = (void *) self - self->size;
    dict->close(dict);
}

/* dict_utf8_encapsulate - wrap a legacy dict object for UTF-8 processing */

DICT   *dict_utf8_encapsulate(DICT *dict)
{
    DICT   *self;

    /*
     * Sanity check.
     */
    if (dict->flags & DICT_FLAG_UTF8_PROXY)
	msg_panic("dict_utf8_encapsulate: %s:%s is already encapsulated",
		  dict->type, dict->name);

    /*
     * Append ourselves to the dict object, so that dict_close(dict) will do
     * the right thing. dict->size is based on the actual size of the dict
     * object's subclass, so we don't have to worry about alignment problems.
     * 
     * XXX Add dict_flags argument to dict_alloc() so that it can allocate the
     * right memory amount, and we can avoid having to resize an object.
     */
    dict = myrealloc(dict, dict->size + sizeof(*self));
    self = (void *) dict + dict->size;
    *self = *dict;

    /*
     * Interpose on the lookup/update/delete/close methods. In particular we
     * do not interpose on the iterator. Invalid keys are not stored, and we
     * want to be able to delete an invalid value.
     */
    self->lookup = dict_utf8_lookup;
    self->update = dict_utf8_update;
    self->delete = dict_utf8_delete;
    self->close = dict_utf8_close;

    /*
     * Finally, disable casefolding in the dict object. It now happens in the
     * lookup/update/delete wrappers.
     */
    dict->flags &= ~DICT_FLAG_FOLD_ANY;
    self->flags |= DICT_FLAG_UTF8_PROXY;

    return (self);
}
