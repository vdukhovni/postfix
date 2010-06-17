/*++
/* NAME
/*	dict_sqlite 3
/* SUMMARY
/*	dictionary manager interface to SQLite3 databases
/* SYNOPSIS
/*	#include <dict_sqlite.h>
/*
/*	DICT	*dict_sqlite_open(name, open_flags, dict_flags)
/*	const char *name;
/*	int	open_flags;
/*	int	dict_flags;
/* DESCRIPTION
/*	dict_sqlite_open() creates a dictionary of type 'sqlite'.
/*	This dictionary is an interface for the postfix key->value
/*	mappings to SQLite.  The result is a pointer to the installed
/*	dictionary, or a null pointer in case of problems.
/* .PP
/*	Arguments:
/* .IP name
/*	Either the path to the SQLite configuration file (if it
/*	starts with '/' or '.'), or the prefix which will be used
/*	to obtain main.cf configuration parameters for this search.
/*
/*	In the first case, the configuration parameters below are
/*	specified in the file as \fIname\fR=\fIvalue\fR pairs.
/*
/*	In the second case, the configuration parameters are prefixed
/*	with the value of \fIname\fR and an underscore, and they
/*	are specified in main.cf.  For example, if this value is
/*	\fIsqlitecon\fR, the parameters would look like
/*	\fIsqlitecon_dbpath\fR, \fIsqlitecon_query\fR, and so on.
/* .IP open_flags
/*	Must be O_RDONLY.
/* .IP dict_flags
/*	See dict_open(3).
/* .PP
/*	Configuration parameters:
/* .IP dbpath
/*	Path to SQLite database
/* .IP query
/*	Query template, before the query is actually issued, variable
/*	substitutions are performed. See sqlite_table(5) for details.
/* .IP result_format
/*	The format used to expand results from queries.  Substitutions
/*	are performed as described in sqlite_table(5). Defaults to
/*	returning the lookup result unchanged.
/* .IP expansion_limit
/*	Limit (if any) on the total number of lookup result values.
/*	Lookups which exceed the limit fail with dict_errno=DICT_ERR_RETRY.
/*	Note that each non-empty (and non-NULL) column of a
/*	multi-column result row counts as one result.
/* .IP "select_field, where_field, additional_conditions"
/*	Legacy query interface.
/* SEE ALSO
/*	dict(3) generic dictionary manager
/* AUTHOR(S)
/*	Axel Steiner
/*	ast@treibsand.com
/*--*/

/* System library. */

#include <sys_defs.h>
#include <string.h>

#ifdef HAS_SQLITE
#include <sqlite3.h>

#if !defined(SQLITE_VERSION_NUMBER) || (SQLITE_VERSION_NUMBER < 3005004)
#error "Your SQLite version is too old"
#endif

/* Utility library. */

#include <msg.h>
#include <dict.h>
#include <vstring.h>
#include <stringops.h>
#include <mymalloc.h>

/* Global library. */

#include <cfg_parser.h>
#include <db_common.h>

/* Application-specific. */

#include <dict_sqlite.h>

typedef struct {
    DICT    dict;			/* generic member */
    CFG_PARSER *parser;			/* common parameter parser */
    sqlite3 *db;			/* sqlite handle */
    char   *query;			/* db_common_expand() query */
    char   *result_format;		/* db_common_expand() result_format */
    void   *ctx;			/* db_common_parse() context */
    char   *dbpath;			/* dbpath config attribute */
    int     expansion_limit;		/* expansion_limit config attribute */
} DICT_SQLITE;

/* dict_sqlite_quote - escape SQL metacharacters in input string */

static void dict_sqlite_quote(DICT *dict, const char *name, VSTRING *result)
{
    char   *q;

    q = sqlite3_mprintf("%q", name);
    /* Fix 20100616 */
    if (q == 0)
	msg_fatal("dict_sqlite_quote: out of memory");
    vstring_strcat(result, q);
    sqlite3_free(q);
}

/* dict_sqlite_close - close the database */

static void dict_sqlite_close(DICT *dict)
{
    const char *myname = "dict_sqlite_close";
    DICT_SQLITE *dict_sqlite = (DICT_SQLITE *) dict;

    if (msg_verbose)
	msg_info("%s: %s", myname, dict_sqlite->parser->name);

    if (sqlite3_close(dict_sqlite->db) != SQLITE_OK)
	msg_fatal("%s: close %s failed", myname, dict_sqlite->parser->name);
    cfg_parser_free(dict_sqlite->parser);
    myfree(dict_sqlite->dbpath);
    myfree(dict_sqlite->query);
    myfree(dict_sqlite->result_format);
    if (dict_sqlite->ctx)
	db_common_free_ctx(dict_sqlite->ctx);
    if (dict->fold_buf)
	vstring_free(dict->fold_buf);
    dict_free(dict);
}

/* dict_sqlite_lookup - find database entry */

static const char *dict_sqlite_lookup(DICT *dict, const char *name)
{
    const char *myname = "dict_sqlite_lookup";
    DICT_SQLITE *dict_sqlite = (DICT_SQLITE *) dict;
    sqlite3_stmt *sql;
    const char *zErrMsg;
    static VSTRING *query;
    static VSTRING *result;
    const char *r;
    int     expansion = 0;
    int     status;

    /*
     * Don't frustrate future attempts to make Postfix UTF-8 transparent.
     */
    if (!valid_utf_8(name, strlen(name))) {
	if (msg_verbose)
	    msg_info("%s: %s: Skipping lookup of non-UTF-8 key '%s'",
		     myname, dict_sqlite->parser->name, name);
	return (0);
    }

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
     * Domain filter for email address lookups.
     */
    if (db_common_check_domain(dict_sqlite->ctx, name) == 0) {
	if (msg_verbose)
	    msg_info("%s: %s: Skipping lookup of '%s'",
		     myname, dict_sqlite->parser->name, name);
	return (0);
    }

    /*
     * Expand the query and query the database.
     */
#define INIT_VSTR(buf, len) do { \
	if (buf == 0) \
		buf = vstring_alloc(len); \
	VSTRING_RESET(buf); \
	VSTRING_TERMINATE(buf); \
    } while (0)

    INIT_VSTR(query, 10);

    if (!db_common_expand(dict_sqlite->ctx, dict_sqlite->query,
			  name, 0, query, dict_sqlite_quote))
	return (0);

    if (msg_verbose)
	msg_info("%s: %s: Searching with query %s",
		 myname, dict_sqlite->parser->name, vstring_str(query));

    if (sqlite3_prepare_v2(dict_sqlite->db, vstring_str(query), -1,
			   &sql, &zErrMsg) != SQLITE_OK) {
	msg_fatal("%s: %s: SQL prepare failed: %s\n",
		  myname, dict_sqlite->parser->name,
		  sqlite3_errmsg(dict_sqlite->db));
    }

    /*
     * Retrieve and expand the result(s).
     */
    INIT_VSTR(result, 10);
    while ((status = sqlite3_step(sql)) == SQLITE_ROW) {
	if (db_common_expand(dict_sqlite->ctx, dict_sqlite->result_format,
			     (char *) sqlite3_column_text(sql, 0),
			     name, result, 0)
	    && dict_sqlite->expansion_limit > 0
	    && ++expansion > dict_sqlite->expansion_limit) {
	    msg_warn("%s: %s: Expansion limit exceeded for key: '%s'",
		     myname, dict_sqlite->parser->name, name);
	    dict_errno = DICT_ERR_RETRY;
	    break;
	}
    }

    /* Fix 20100616 */
    if (status != SQLITE_ROW && status != SQLITE_DONE) {
	msg_warn("%s: %s: sql step for %s; %s\n",
		 myname, dict_sqlite->parser->name,
		 vstring_str(query), sqlite3_errmsg(dict_sqlite->db));
	dict_errno = DICT_ERR_RETRY;
    }

    /*
     * Clean up.
     */
    if (sqlite3_finalize(sql))
	msg_fatal("%s: %s: SQL finalize for %s; %s\n",
		  myname, dict_sqlite->parser->name,
		  vstring_str(query), sqlite3_errmsg(dict_sqlite->db));

    r = vstring_str(result);
    return ((dict_errno == 0 && *r) ? r : 0);
}

/* sqlite_parse_config - parse sqlite configuration file */

static void sqlite_parse_config(DICT_SQLITE *dict_sqlite, const char *sqlitecf)
{
    CFG_PARSER *p;
    VSTRING *buf;

    p = dict_sqlite->parser = cfg_parser_alloc(sqlitecf);
    dict_sqlite->dbpath = cfg_get_str(p, "dbpath", "", 1, 0);
    dict_sqlite->result_format = cfg_get_str(p, "result_format", "%s", 1, 0);

    if ((dict_sqlite->query = cfg_get_str(p, "query", NULL, 0, 0)) == 0) {
	buf = vstring_alloc(64);
	db_common_sql_build_query(buf, p);
	dict_sqlite->query = vstring_export(buf);
    }
    dict_sqlite->expansion_limit = cfg_get_int(p, "expansion_limit", 0, 0, 0);
    dict_sqlite->ctx = 0;

    (void) db_common_parse(&dict_sqlite->dict, &dict_sqlite->ctx, dict_sqlite->query, 1);
    (void) db_common_parse(0, &dict_sqlite->ctx, dict_sqlite->result_format, 0);

    db_common_parse_domain(p, dict_sqlite->ctx);

    if (dict_sqlite->dict.flags & DICT_FLAG_FOLD_FIX)
	dict_sqlite->dict.fold_buf = vstring_alloc(10);
}

/* dict_sqlite_open - open sqlite database */

DICT   *dict_sqlite_open(const char *name, int open_flags, int dict_flags)
{
    DICT_SQLITE *dict_sqlite;

    /*
     * Sanity checks.
     */
    if (open_flags != O_RDONLY)
	msg_fatal("%s:%s map requires O_RDONLY access mode",
		  DICT_TYPE_SQLITE, name);

    dict_sqlite = (DICT_SQLITE *) dict_alloc(DICT_TYPE_SQLITE, name,
					     sizeof(DICT_SQLITE));
    dict_sqlite->dict.lookup = dict_sqlite_lookup;
    dict_sqlite->dict.close = dict_sqlite_close;
    dict_sqlite->dict.flags = dict_flags;
    dict_sqlite->dict.flags |= DICT_FLAG_FIXED;
    sqlite_parse_config(dict_sqlite, name);

    if (sqlite3_open(dict_sqlite->dbpath, &dict_sqlite->db)) {
	msg_fatal("Can't open database: %s\n", sqlite3_errmsg(dict_sqlite->db));
	sqlite3_close(dict_sqlite->db);
    }
    return (DICT_DEBUG (&dict_sqlite->dict));
}

#endif
