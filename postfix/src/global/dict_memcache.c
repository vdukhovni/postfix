/*++
/* NAME
/*	dict_memcache 3
/* SUMMARY
/*	dictionary interface to memcache databases
/* SYNOPSIS
/*	#include <dict_memcache.h>
/*
/*	DICT	*dict_memcache_open(name, open_flags, dict_flags)
/*	const char *name;
/*	int	open_flags;
/*	int	dict_flags;
/* DESCRIPTION
/*	dict_memcache_open() opens a memcache database, providing
/*	a dictionary interface for Postfix key->value mappings.
/*	The result is a pointer to the installed dictionary.
/*
/*	Configuration parameters are described in memcache_table(5).
/*
/*	Arguments:
/* .IP name
/*	Either the path to the Postfix memcache configuration file
/*	(if it starts with '/' or '.'), or the parameter name prefix
/*	which will be used to obtain main.cf configuration parameters.
/* .IP open_flags
/*	O_RDONLY or O_RDWR. This function ignores flags that don't
/*	specify a read, write or append mode.
/* .IP dict_flags
/*	See dict_open(3).
/* SEE ALSO
/*	dict(3) generic dictionary manager
/* BUGS
/*	This code requires libmemcache 1.4.0, because some parts
/*	of their API are documented by looking at the implementation.
/* HISTORY
/*	The first memcache client for Postfix was written by:
/*	Omar Kilani
/*	omar@tinysofa.com
/*	This implementation bears no resemblance to his work.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include "sys_defs.h"

#ifdef HAS_MEMCACHE
#include <string.h>
#include <memcache.h>

#if !defined(MEMCACHE_VERNUM) || MEMCACHE_VERNUM != 10400
#error "Postfix memcache supports only libmemcache version 1.4.0"
#endif

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <dict.h>
#include <vstring.h>
#include <stringops.h>
#include <binhash.h>

/* Global library. */

#include <cfg_parser.h>
#include <db_common.h>

/* Application-specific. */

#include <dict_memcache.h>

 /*
  * Robustness tests (with a single memcache server) proved disappointing.
  * 
  * After failure to connect to the memcache server, libmemcache reports the
  * error once. From then on it silently discards all updates and always
  * reports "not found" for all lookups, without ever reporting an error. To
  * avoid this, we destroy the memcache client and create a new one after
  * libmemcache reports an error.
  * 
  * Even more problematic is that libmemcache will terminate the process when
  * the memcache server connection is lost (the libmemcache error message is:
  * "read(2) failed: Socket is already connected"). Unfortunately, telling
  * libmemcache not to terminate the process will result in an assertion
  * failure followed by core dump.
  * 
  * Conclusion: if we want robust code, then we should use our own memcache
  * protocol implementation instead of libmemcache.
  */

 /*
  * Structure of one memcache dictionary handle.
  */
typedef struct {
    DICT    dict;			/* parent class */
    struct memcache_ctxt *mc_ctxt;	/* libmemcache context */
    struct memcache *mc;		/* libmemcache object */
    CFG_PARSER *parser;			/* common parameter parser */
    void   *dbc_ctxt;			/* db_common context */
    char   *key_format;			/* query key translation */
    int     mc_ttl;			/* memcache expiration */
    int     mc_flags;			/* memcache flags */
    VSTRING *key_buf;			/* lookup key */
    VSTRING *res_buf;			/* lookup result */
} DICT_MC;

 /*
  * Default memcache options.
  */
#define DICT_MC_DEF_HOST	"localhost"
#define DICT_MC_DEF_PORT	"11211"
#define DICT_MC_DEF_HOST_PORT	DICT_MC_DEF_HOST ":" DICT_MC_DEF_PORT
#define DICT_MC_DEF_KEY_FMT	"%s"
#define DICT_MC_DEF_TTL		(7 * 86400)
#define DICT_MC_DEF_FLAGS	0

 /*
  * libmemcache can report errors through an application call-back function,
  * but there is no support for passing application context to the call-back.
  * The call-back API has two documented arguments: pointer to memcache_ctxt,
  * and pointer to memcache_ectxt. The memcache_ctxt data structure has no
  * space for application context, and the mcm_err() function zero-fills the
  * memcache_ectxt data structure, making it useless for application context.
  * 
  * We use our own hash table to find our dictionary handle, so that we can
  * report errors in the proper context.
  */
static BINHASH *dict_mc_hash;

 /*
  * SLMs.
  */
#define STR(x)	vstring_str(x)
#define LEN(x)	VSTRING_LEN(x)

/*#define msg_verbose 1*/

/* dict_memcache_error_cb - error call-back */

static int dict_memcache_error_cb(MCM_ERR_FUNC_ARGS)
{
    const char *myname = "dict_memcache_error_cb";
    const struct memcache_ctxt *ctxt;
    struct memcache_err_ctxt *ectxt;
    DICT_MC *dict_mc;
    void    (*log_fn) (const char *,...);

    /*
     * Play by the rules of the libmemcache API.
     */
    MCM_ERR_INIT_CTXT(ctxt, ectxt);

    /*
     * Locate our own dictionary handle for error reporting context.
     * Unfortunately, the ctxt structure does not store application context,
     * and mcm_err() zero-fills the ectxt structure, making it useless for
     * storing application context. We use our own hash table instead.
     */
    if ((dict_mc = (DICT_MC *) binhash_find(dict_mc_hash, (char *) &ctxt,
					    sizeof(ctxt))) == 0)
	msg_panic("%s: can't locate DICT_MC database handle", myname);

    /*
     * Report the error in our context, and set dict_errno for possible
     * errors. We override dict_errno when an error was recoverable.
     */
    switch (ectxt->severity) {
    default:
#ifdef DICT_MC_RECOVER_FROM_DISCONNECT
	/* Code below causes an assert failure and core dump. */
	if (ectxt->errcode == MCM_ERR_SYS_READ)
	    /* Also: MCM_ERR_SYS_WRITEV, MCM_ERR_SYS_SETSOCKOPT */
	    ectxt->cont = 'y';
#endif
	/* FALLTHROUGH */
    case MCM_ERR_LVL_NOTICE:
	log_fn = msg_warn;
	dict_errno = 1;
	break;
    case MCM_ERR_LVL_INFO:
	log_fn = msg_info;
	break;
    }
    log_fn((ectxt)->errnum ? "database %s:%s: libmemcache error: %s: %m" :
	   "database %s:%s: libmemcache error: %s",
	   DICT_TYPE_MEMCACHE, dict_mc->dict.name, (ectxt)->errstr);
    return (0);
}

static void dict_memcache_mc_free(DICT_MC *);
static void dict_memcache_mc_init(DICT_MC *);

/* dict_memcache_recover - recover after libmemcache error */

static void dict_memcache_recover(DICT_MC *dict_mc)
{
    int     saved_dict_errno;

    /*
     * XXX If we don't try to recover from the first error, libmemcache will
     * silently skip all subsequent database operations.
     */
    saved_dict_errno = dict_errno;
    dict_memcache_mc_free(dict_mc);
    dict_memcache_mc_init(dict_mc);
    dict_errno = saved_dict_errno;
}

/* dict_memcache_prepare_key - prepare lookup key */

static int dict_memcache_prepare_key(DICT_MC *dict_mc, const char *name)
{

    /*
     * Optionally case-fold the search string.
     */
    if (dict_mc->dict.flags & DICT_FLAG_FOLD_FIX) {
	if (dict_mc->dict.fold_buf == 0)
	    dict_mc->dict.fold_buf = vstring_alloc(10);
	vstring_strcpy(dict_mc->dict.fold_buf, name);
	name = lowercase(STR(dict_mc->dict.fold_buf));
    }

    /*
     * Optionally expand the query key format.
     */
#define DICT_MC_NO_KEY		(0)
#define DICT_MC_NO_QUOTING	((void (*)(DICT *, const char *, VSTRING *)) 0)

    if (dict_mc->key_format != 0
	&& strcmp(dict_mc->key_format, DICT_MC_DEF_KEY_FMT) != 0) {
	VSTRING_RESET(dict_mc->key_buf);
	if (db_common_expand(dict_mc->dbc_ctxt, dict_mc->key_format,
			     name, DICT_MC_NO_KEY, dict_mc->key_buf,
			     DICT_MC_NO_QUOTING) == 0)
	    return (0);
    } else {
	vstring_strcpy(dict_mc->key_buf, name);
    }

    /*
     * The length indicates whether the expansion is empty or not.
     */
    return (LEN(dict_mc->key_buf));
}

/* dict_memcache_update - update memcache database */

static void dict_memcache_update(DICT *dict, const char *name,
				         const char *value)
{
    const char *myname = "dict_memcache_update";
    DICT_MC *dict_mc = (DICT_MC *) dict;

    /*
     * Skip updates with a null key, noisily. This would result in loss of
     * information.
     */
    if (dict_memcache_prepare_key(dict_mc, name) == 0) {
	dict_errno = 1;
	msg_warn("database %s:%s: name \"%s\" expands to empty lookup key "
		 "-- skipping update", DICT_TYPE_MEMCACHE,
		 dict_mc->dict.name, name);
	return;
    }

    /*
     * Our error call-back routine will report errors and set dict_errno.
     */
    dict_errno = (mcm_set(dict_mc->mc_ctxt, dict_mc->mc, STR(dict_mc->key_buf),
			  LEN(dict_mc->key_buf), value, strlen(value),
			  dict_mc->mc_ttl, dict_mc->mc_flags) != 0);
    if (msg_verbose)
	msg_info("%s: %s: update key \"%s\" => \"%s\" %s",
		 myname, dict_mc->dict.name, STR(dict_mc->key_buf), value,
		 dict_errno ? "(error)" : "(no error)");

    /*
     * Recover after server failure.
     */
    if (dict_errno)
	dict_memcache_recover(dict_mc);
}

/* dict_memcache_lookup - lookup memcache database */

static const char *dict_memcache_lookup(DICT *dict, const char *name)
{
    const char *myname = "dict_memcache_lookup";
    DICT_MC *dict_mc = (DICT_MC *) dict;
    struct memcache_req *req;
    struct memcache_res *res;
    const char *retval;

    /*
     * Skip lookups with a null key, silently. This is just asking for
     * information that cannot exist.
     */
#define DICT_MC_SKIP(why, map_name, key) do { \
	if (msg_verbose) \
	    msg_info("%s: %s: skipping lookup of key \"%s\": %s", \
		     myname, (map_name), (key), (why)); \
	return (0); \
    } while (0)

    if (*name == 0)
	DICT_MC_SKIP("empty lookup key", dict_mc->dict.name, name);
    if (db_common_check_domain(dict_mc->dbc_ctxt, name) == 0)
	DICT_MC_SKIP("domain mismatch", dict_mc->dict.name, name);
    if (dict_memcache_prepare_key(dict_mc, name) == 0)
	DICT_MC_SKIP("empty lookup key expansion", dict_mc->dict.name, name);

    /*
     * Our error call-back routine will report errors and set dict_errno. We
     * reset dict_errno after an error turns out to be recoverable.
     */
    if ((req = mcm_req_new(dict_mc->mc_ctxt)) == 0)
	msg_fatal("%s: can't create new request: %m", myname);	/* XXX */
    /* Not: mcm_req_add(), because that makes unnecessary copy of the key. */
    if ((res = mcm_req_add_ref(dict_mc->mc_ctxt, req, STR(dict_mc->key_buf),
			       LEN(dict_mc->key_buf))) == 0)
	msg_fatal("%s: can't create new result: %m", myname);	/* XXX */

    dict_errno = 0;
    mcm_get(dict_mc->mc_ctxt, dict_mc->mc, req);
    if (mcm_res_found(dict_mc->mc_ctxt, res) && res->bytes) {
	vstring_strncpy(dict_mc->res_buf, res->val, res->bytes);
	retval = STR(dict_mc->res_buf);
	dict_errno = 0;
    } else {
	retval = 0;
    }
    mcm_res_free(dict_mc->mc_ctxt, req, res);
    mcm_req_free(dict_mc->mc_ctxt, req);

    if (msg_verbose)
	msg_info("%s: %s: key %s => %s",
		 myname, dict_mc->dict.name, STR(dict_mc->key_buf),
		 retval ? STR(dict_mc->res_buf) :
		 dict_errno ? "(error)" : "(not found)");

    /*
     * Recover after server failure.
     */
    if (dict_errno)
	dict_memcache_recover(dict_mc);

    return (retval);
}

/* dict_memcache_mc_free - destroy libmemcache objects */

static void dict_memcache_mc_free(DICT_MC *dict_mc)
{
    binhash_delete(dict_mc_hash, (char *) &dict_mc->mc_ctxt,
		   sizeof(dict_mc->mc_ctxt), (void (*) (char *)) 0);
    mcm_free(dict_mc->mc_ctxt, dict_mc->mc);
    mcMemFreeCtxt(dict_mc->mc_ctxt);
}

/* dict_memcache_mc_init - create libmemcache objects */

static void dict_memcache_mc_init(DICT_MC *dict_mc)
{
    const char *myname = "dict_memcache_mc_init";
    char   *servers;
    char   *server;
    char   *cp;

    /*
     * Create the libmemcache objects.
     */
    dict_mc->mc_ctxt =
	mcMemNewCtxt((mcFreeFunc) myfree, (mcMallocFunc) mymalloc,
		     (mcMallocFunc) mymalloc, (mcReallocFunc) myrealloc);
    if (dict_mc->mc_ctxt == 0)
	msg_fatal("error creating memcache context: %m");	/* XXX */
    dict_mc->mc = mcm_new(dict_mc->mc_ctxt);
    if (dict_mc->mc == 0)
	msg_fatal("error creating memcache object: %m");	/* XXX */

    /*
     * Set up call-back info for error reporting.
     */
    if (dict_mc_hash == 0)
	dict_mc_hash = binhash_create(1);
    binhash_enter(dict_mc_hash, (char *) &dict_mc->mc_ctxt,
		  sizeof(dict_mc->mc_ctxt), (char *) dict_mc);
    mcErrSetupCtxt(dict_mc->mc_ctxt, dict_memcache_error_cb);

    /*
     * Add the server list.
     */
    cp = servers = cfg_get_str(dict_mc->parser, "hosts",
			       DICT_MC_DEF_HOST_PORT, 0, 0);
    while ((server = mystrtok(&cp, " ,\t\r\n")) != 0) {
	if (msg_verbose)
	    msg_info("%s: database %s:%s: adding server %s",
		     myname, DICT_TYPE_MEMCACHE, dict_mc->dict.name, server);
	if (mcm_server_add4(dict_mc->mc_ctxt, dict_mc->mc, server) < 0)
	    msg_warn("database %s:%s: error adding server %s",
		     DICT_TYPE_MEMCACHE, dict_mc->dict.name, server);
    }
    myfree(servers);
}

/* dict_memcache_close - close memcache database */

static void dict_memcache_close(DICT *dict)
{
    DICT_MC *dict_mc = (DICT_MC *) dict;

    dict_memcache_mc_free(dict_mc);
    cfg_parser_free(dict_mc->parser);
    db_common_free_ctx(dict_mc->dbc_ctxt);
    vstring_free(dict_mc->key_buf);
    vstring_free(dict_mc->res_buf);
    if (dict_mc->key_format)
	myfree(dict_mc->key_format);
    if (dict->fold_buf)
	vstring_free(dict->fold_buf);
    dict_free(dict);
}

/* dict_memcache_open - open memcache database */

DICT   *dict_memcache_open(const char *name, int open_flags, int dict_flags)
{
    DICT_MC *dict_mc;

    /*
     * Sanity checks.
     */
    if (dict_flags & DICT_FLAG_NO_UNAUTH)
	msg_fatal("%s:%s map is not allowed for security-sensitive data",
		  DICT_TYPE_MEMCACHE, name);
    open_flags &= (O_RDONLY | O_RDWR | O_WRONLY | O_APPEND);
    if (open_flags != O_RDONLY && open_flags != O_RDWR)
	msg_fatal("%s:%s map requires O_RDONLY or O_RDWR access mode",
		  DICT_TYPE_MEMCACHE, name);

    /*
     * Create the dictionary object.
     */
    dict_mc = (DICT_MC *) dict_alloc(DICT_TYPE_MEMCACHE, name,
				     sizeof(*dict_mc));
    dict_mc->dict.lookup = dict_memcache_lookup;
    if (open_flags == O_RDWR)
	dict_mc->dict.update = dict_memcache_update;
    dict_mc->dict.close = dict_memcache_close;
    dict_mc->dict.flags = dict_flags;
    dict_mc->key_buf = vstring_alloc(10);
    dict_mc->res_buf = vstring_alloc(10);

    /*
     * Parse the configuration file.
     */
    dict_mc->parser = cfg_parser_alloc(name);
    dict_mc->key_format = cfg_get_str(dict_mc->parser, "key_format",
				      DICT_MC_DEF_KEY_FMT, 0, 0);
    dict_mc->mc_ttl = cfg_get_int(dict_mc->parser, "ttl",
				  DICT_MC_DEF_TTL, 0, 0);
    dict_mc->mc_flags = cfg_get_int(dict_mc->parser, "flags",
				    DICT_MC_DEF_FLAGS, 0, 0);

    /*
     * Initialize the memcache objects.
     */
    dict_memcache_mc_init(dict_mc);

    /*
     * Parse templates and common database parameters. Maps that use
     * substring keys should only be used with the full input key.
     */
    dict_mc->dbc_ctxt = 0;
    db_common_parse(&dict_mc->dict, &dict_mc->dbc_ctxt,
		    dict_mc->key_format, 1);
    db_common_parse_domain(dict_mc->parser, dict_mc->dbc_ctxt);
    if (db_common_dict_partial(dict_mc->dbc_ctxt))
	/* Breaks recipient delimiters */
	dict_mc->dict.flags |= DICT_FLAG_PATTERN;
    else
	dict_mc->dict.flags |= DICT_FLAG_FIXED;

    return (&dict_mc->dict);
}

#endif
