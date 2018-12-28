/*++
/* NAME
/*	dict_wrapper 3
/* SUMMARY
/*	dictionary method wrappers
/* SYNOPSIS
/*	#include <dict.h>
/*
/*	void	dict_wrapper_prepend(
/*	DICT	*dict,
/*	DICT_WRAPPER *wrapper)
/*
/*	DICT_WRAPPER *dict_wrapper_alloc(
/*	ssize_t	size)
/*
/*	void	dict_wrapper_free(
/*	DICT_WRAPPER *wrapper)
/* DESCRIPTION
/*	dict_wrapper_prepend() prepends the specified dictionary
/*	lookup/update/delete wrappers to a chain that is evaluated
/*	in reverse order. dict_wrapper_prepend() takes ownership
/*	of the wrapper.
/*
/*	dict_wrapper_alloc() allocates memory for a dictionary
/*	wrapper object and initializes all wrapper methods to
/*	empty (no override).
/*
/*	dict_wrapper_free() destroys a chain of dictionary wrappers.
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
  * The final DICT_WRAPPER is installed first, and also contains the original
  * DICT's methods.
  */
typedef struct {
    DICT_WRAPPER wrapper;		/* parent class, must be first */
    const char *(*saved_lookup) (DICT *, const char *);
    int     (*saved_update) (DICT *, const char *, const char *);
    int     (*saved_delete) (DICT *, const char *);
} DICT_FINAL_WRAPPER;

 /*
  * Functions that override DICT methods, and that call into the head of
  * the dict wrapper chain.
  */

/* dict_wrapper_lookup - DICT method to call into wrapper chain head */

static const char *dict_wrapper_lookup(DICT *dict, const char *key)
{
    DICT_WRAPPER *head_wrapper = dict->wrapper;

    return (head_wrapper->lookup(head_wrapper, dict, key));
}

/* dict_wrapper_update - DICT method to call into wrapper chain head */

static int dict_wrapper_update(DICT *dict, const char *key, const char *value)
{
    DICT_WRAPPER *head_wrapper = dict->wrapper;

    return (head_wrapper->update(head_wrapper, dict, key, value));
}

/* dict_wrapper_delete - DICT method to call into wrapper chain head */

static int dict_wrapper_delete(DICT *dict, const char *key)
{
    DICT_WRAPPER *head_wrapper = dict->wrapper;

    return (head_wrapper->delete(head_wrapper, dict, key));
}

 /*
  * Empty methods for wrappers that override only some methods. These ensure
  * that the next wrapper's methods are called with the right 'self' pointer.
  */

/* empty_wrapper_lookup - wrapper method to call into next wrapper */

static const char *empty_wrapper_lookup(DICT_WRAPPER *wrapper, DICT *dict,
				               const char *key)
{
    DICT_WRAPPER *next_wrapper = wrapper->next;

    return (next_wrapper->lookup(next_wrapper, dict, key));
}

/* empty_wrapper_update - wrapper method to call into next wrapper */

static int empty_wrapper_update(DICT_WRAPPER *wrapper, DICT *dict,
			               const char *key, const char *value)
{
    DICT_WRAPPER *next_wrapper = wrapper->next;

    return (next_wrapper->update(next_wrapper, dict, key, value));
}

/* empty_wrapper_delete - wrapper method to call into next wrapper */

static int empty_wrapper_delete(DICT_WRAPPER *wrapper, DICT *dict,
			               const char *key)
{
    DICT_WRAPPER *next_wrapper = wrapper->next;

    return (next_wrapper->delete(next_wrapper, dict, key));
}

 /*
  * Wrapper methods for the final dict wrapper in the chain. These call into
  * the saved DICT methods.
  */

/* final_wrapper_lookup - wrapper method to call saved DICT method */

static const char *final_wrapper_lookup(DICT_WRAPPER *wrapper, DICT *dict,
					        const char *key)
{
    DICT_FINAL_WRAPPER *final_wrapper = (DICT_FINAL_WRAPPER *) wrapper;

    return (final_wrapper->saved_lookup(dict, key));
}

/* final_wrapper_update - wrapper method to call saved DICT method */

static int final_wrapper_update(DICT_WRAPPER *wrapper, DICT *dict,
				        const char *key, const char *value)
{
    DICT_FINAL_WRAPPER *final_wrapper = (DICT_FINAL_WRAPPER *) wrapper;

    return (final_wrapper->saved_update(dict, key, value));
}

/* final_wrapper_delete - wrapper method to call saved DICT method */

static int final_wrapper_delete(DICT_WRAPPER *wrapper, DICT *dict,
				        const char *key)
{
    DICT_FINAL_WRAPPER *final_wrapper = (DICT_FINAL_WRAPPER *) wrapper;

    return (final_wrapper->saved_delete(dict, key));
}

 /*
  * Finally, the functions that build the wrapper chain.
  */

/* dict_wrapper_activate - wrap a DICT object for additional processing */

static void dict_wrapper_activate(DICT *dict)
{
    const char myname[] = "dict_wrapper_activate";
    DICT_FINAL_WRAPPER *final_wrapper;

    /*
     * Sanity check.
     */
    if (dict->wrapper != 0)
	msg_panic("%s: %s:%s wrapper support is already activated",
		  myname, dict->type, dict->name);

    /*
     * Install the final wrapper object that calls the original DICT's
     * methods, and redirect DICT's method calls to ourselves. All other
     * dictionary wrappers will be prepended to a chain that ends in the
     * final wrapper object.
     */
    final_wrapper = (DICT_FINAL_WRAPPER *) mymalloc(sizeof(*final_wrapper));
    final_wrapper->wrapper.name = "final";
    final_wrapper->wrapper.lookup = final_wrapper_lookup;
    final_wrapper->wrapper.update = final_wrapper_update;
    final_wrapper->wrapper.delete = final_wrapper_delete;
    final_wrapper->wrapper.next = 0;
    dict->wrapper = &final_wrapper->wrapper;

    /*
     * Interpose on the DICT's lookup/update/delete methods.
     */
    final_wrapper->saved_lookup = dict->lookup;
    final_wrapper->saved_update = dict->update;
    final_wrapper->saved_delete = dict->delete;

    dict->lookup = dict_wrapper_lookup;
    dict->update = dict_wrapper_update;
    dict->delete = dict_wrapper_delete;
}

/* dict_wrapper_alloc - allocate and initialize dictionary wrapper */

DICT_WRAPPER *dict_wrapper_alloc(ssize_t size)
{
    DICT_WRAPPER *wrapper;

    wrapper = (DICT_WRAPPER *) mymalloc(size);
    wrapper->lookup = empty_wrapper_lookup;
    wrapper->update = empty_wrapper_update;
    wrapper->delete = empty_wrapper_delete;
    return (wrapper);
}

/* dict_wrapper_prepend - prepend dict method overrides */

void    dict_wrapper_prepend(DICT *dict, DICT_WRAPPER *wrapper)
{
    if (dict->wrapper == 0)
	dict_wrapper_activate(dict);

    wrapper->next = dict->wrapper;
    dict->wrapper = wrapper;
}

/* dict_wrapper_free - wrapper destructor */

void    dict_wrapper_free(DICT_WRAPPER *wrapper)
{
    if (wrapper->next)
	dict_wrapper_free(wrapper->next);
    myfree((void *) wrapper);
}
