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
/*	dictionary.
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
/* DIAGNOSTICS
/*	With \fBstatement_mode = legacy\fR, dict_sqlite_open() logs a
/*	warning when the query parameter value does not use the
/*	recommended '' quotes to protect against SQL injection (bad
/*	examples; no quotes or "" quotes). The warning is suppressed in
/*	the prepared-statement modes, where lookup keys are bound rather
/*	than substituted into the SQL text.
/* SEE ALSO
/*	dict(3) generic dictionary manager
/*	sqlite_table(5) Postfix sqlite client configuration
/* AUTHOR(S)
/*	Axel Steiner
/*	ast@treibsand.com
/*
/*	Adopted and updated by:
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*
/*	Wietse Venema
/*	porcupine.org
/*
/*	Prepared statement support by
/*	Ömer Güven
/*--*/

/* System library. */

#include <sys_defs.h>
#include <string.h>
#include <ctype.h>

#ifdef HAS_SQLITE
#include <sqlite3.h>

#if !defined(SQLITE_VERSION_NUMBER) || (SQLITE_VERSION_NUMBER < 3005004)
#define sqlite3_prepare_v2 sqlite3_prepare
#endif
#if !defined(SQLITE_VERSION_NUMBER) || (SQLITE_VERSION_NUMBER < 3005000)
#define sqlite3_open_v2(fname,ppDB,flags,zVfs) sqlite_open(fname,ppDB)
#endif

/* Utility library. */

#include <argv.h>
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

#define INIT_VSTR(buf, len) do { \
	if (buf == 0) \
		buf = vstring_alloc(len); \
	VSTRING_RESET(buf); \
	VSTRING_TERMINATE(buf); \
    } while (0)

#define SQLITE_STATEMENT_LEGACY		0
#define SQLITE_STATEMENT_PREPARED	1

typedef struct SQLITE_STATEMENT SQLITE_STATEMENT;	/* defined below */

typedef struct {
    DICT    dict;			/* generic member */
    CFG_PARSER *parser;			/* common parameter parser */
    sqlite3 *db;			/* sqlite handle */
    char   *query;			/* db_common_expand() query */
    char   *result_format;		/* db_common_expand() result_format */
    void   *ctx;			/* db_common_parse() context */
    char   *dbpath;			/* dbpath config attribute */
    int     expansion_limit;		/* expansion_limit config attribute */
    int     statement_mode;		/* SQLITE_STATEMENT_{LEGACY,PREPARED} */
    SQLITE_STATEMENT *stmt;		/* compiled query, prepared mode only */
} DICT_SQLITE;

 /*
  * Lookup variants. dict_sqlite_lookup() does the per-key checks that are
  * shared between the two modes (UTF-8, key folding, domain filter), then
  * dispatches to one of the implementations below. The legacy and prepared
  * implementations are at the bottom of this file, well separated, so a
  * future cleanup that retires legacy support can delete one block plus the
  * dispatcher branch that calls it.
  */
static const char *dict_sqlite_lookup_legacy(DICT *, const char *);
static const char *dict_sqlite_lookup_prepared(DICT *, const char *);
static SQLITE_STATEMENT *sqlite_statement_alloc(const char *, const char *);
static void sqlite_statement_free(SQLITE_STATEMENT *);

/* dict_sqlite_quote - escape SQL metacharacters in input string */

static void dict_sqlite_quote(DICT *dict, const char *raw_text, VSTRING *result)
{
    char   *quoted_text;

    quoted_text = sqlite3_mprintf("%q", raw_text);
    /* Fix 20100616 */
    if (quoted_text == 0)
	msg_fatal("dict_sqlite_quote: out of memory");
    vstring_strcat(result, quoted_text);
    sqlite3_free(quoted_text);
}

/* dict_sqlite_close - close the database */

static void dict_sqlite_close(DICT *dict)
{
    const char *myname = "dict_sqlite_close";
    DICT_SQLITE *dict_sqlite = (DICT_SQLITE *) dict;

    if (msg_verbose)
	msg_info("%s: %s", myname, dict_sqlite->parser->name);

    if (dict_sqlite->stmt)
	sqlite_statement_free(dict_sqlite->stmt);
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
    int     domain_rc;

    /*
     * In case of return without lookup (skipped key, etc.).
     */
    dict->error = 0;

    /*
     * Don't frustrate future attempts to make Postfix UTF-8 transparent.
     */
    if ((dict->flags & DICT_FLAG_UTF8_ACTIVE) == 0
	&& !valid_utf8_stringz(name)) {
	if (msg_verbose)
	    msg_info("%s: %s: Skipping lookup of non-UTF-8 key '%s'",
		     myname, dict_sqlite->parser->name, name);
	return (0);
    }

    /*
     * Optionally fold the key. Folding may be enabled on-the-fly.
     */
    if (dict->flags & DICT_FLAG_FOLD_FIX) {
	if (dict->fold_buf == 0)
	    dict->fold_buf = vstring_alloc(100);
	vstring_strcpy(dict->fold_buf, name);
	name = lowercase(vstring_str(dict->fold_buf));
    }

    /*
     * Apply the optional domain filter for email address lookups.
     */
    if ((domain_rc = db_common_check_domain(dict_sqlite->ctx, name)) == 0) {
	if (msg_verbose)
	    msg_info("%s: %s: Skipping lookup of '%s'",
		     myname, dict_sqlite->parser->name, name);
	return (0);
    }
    if (domain_rc < 0)
	DICT_ERR_VAL_RETURN(dict, domain_rc, (char *) 0);

    /* Dispatch to the legacy or prepared implementation. */
    return ((dict_sqlite->statement_mode == SQLITE_STATEMENT_LEGACY) ?
	    dict_sqlite_lookup_legacy(dict, name) :
	    dict_sqlite_lookup_prepared(dict, name));
}

/* flag_non_recommended_query - as the name says. */

static void flag_non_recommended_query(const char *query,
				               const char *sqlitecf)
{
    const char *cp;
    int     in_quote;
    const int squote = '\'';
    const int dquote = '"';

    for (in_quote = 0, cp = query; *cp != 0; cp++) {
	if (in_quote == 0) {
	    if (*cp == squote || *cp == dquote)
		in_quote = *cp;
	} else if (*cp == in_quote) {
	    in_quote = 0;
	}
	if (in_quote == squote)
	    continue;
	if (*cp == '%') {
	    if (cp[1] == '%') {
		cp += 1;
	    } else if (ISALNUM(cp[1])) {
		msg_warn("%s:%s: query >%s< contains >%.2s< without the "
			 "recommended '' quotes", DICT_TYPE_SQLITE, sqlitecf,
			 query, cp);
		cp += 1;
	    }
	}
    }
}

/* sqlite_parse_config - parse sqlite configuration file */

static void sqlite_parse_config(DICT_SQLITE *dict_sqlite, const char *sqlitecf)
{
    VSTRING *buf;
    char   *statement_mode;

    /*
     * Parse the primary configuration parameters, and emulate the legacy
     * query interface if necessary. This simplifies migration from one SQL
     * database type to another.
     */
    dict_sqlite->dbpath = cfg_get_str(dict_sqlite->parser, "dbpath", "", 1, 0);
    dict_sqlite->query = cfg_get_str(dict_sqlite->parser, "query", NULL, 0, 0);
    if (dict_sqlite->query == 0) {
	buf = vstring_alloc(100);
	db_common_sql_build_query(buf, dict_sqlite->parser);
	dict_sqlite->query = vstring_export(buf);
    }
    dict_sqlite->result_format =
	cfg_get_str(dict_sqlite->parser, "result_format", "%s", 1, 0);
    statement_mode = cfg_get_str(dict_sqlite->parser, "statement_mode",
				 "legacy", 1, 0);
    if (strcasecmp(statement_mode, "legacy") == 0) {
	dict_sqlite->statement_mode = SQLITE_STATEMENT_LEGACY;
    } else if (strcasecmp(statement_mode, "prepared") == 0) {
	dict_sqlite->statement_mode = SQLITE_STATEMENT_PREPARED;
    } else {
	msg_fatal("%s: %s: bad statement_mode value '%s'; specify "
		  "'legacy' or 'prepared'", __func__, sqlitecf, statement_mode);
    }
    myfree(statement_mode);

    /*
     * The recommended-quoting check applies only to the legacy code path.
     * In prepared mode the query rewriter strips the surrounding quotes,
     * so the warning would be misleading.
     */
    if (dict_sqlite->statement_mode == SQLITE_STATEMENT_LEGACY)
	/* Flag %[a-zA-Z0-9] if not protected with ''. */
	flag_non_recommended_query(dict_sqlite->query, sqlitecf);
    dict_sqlite->expansion_limit =
	cfg_get_int(dict_sqlite->parser, "expansion_limit", 0, 0, 0);

    /*
     * Parse the query / result templates and the optional domain filter.
     */
    dict_sqlite->ctx = 0;
    (void) db_common_parse(&dict_sqlite->dict, &dict_sqlite->ctx,
			   dict_sqlite->query, 1);
    (void) db_common_parse(0, &dict_sqlite->ctx, dict_sqlite->result_format, 0);
    db_common_parse_domain(dict_sqlite->parser, dict_sqlite->ctx);

    /*
     * In prepared mode, parse the query template into a SQLITE_STATEMENT
     * once at open time. The actual sqlite3_prepare_v2() call is deferred
     * to the first lookup, so that sqlite3_open_v2() has run first.
     */
    if (dict_sqlite->statement_mode != SQLITE_STATEMENT_LEGACY)
	dict_sqlite->stmt = sqlite_statement_alloc(dict_sqlite->parser->name,
						   dict_sqlite->query);

    /*
     * Maps that use substring keys should only be used with the full input
     * key.
     */
    if (db_common_dict_partial(dict_sqlite->ctx))
	dict_sqlite->dict.flags |= DICT_FLAG_PATTERN;
    else
	dict_sqlite->dict.flags |= DICT_FLAG_FIXED;
}

/* dict_sqlite_open - open sqlite database */

DICT   *dict_sqlite_open(const char *name, int open_flags, int dict_flags)
{
    DICT_SQLITE *dict_sqlite;
    CFG_PARSER *parser;

    /*
     * Sanity checks.
     */
    if (open_flags != O_RDONLY)
	return (dict_surrogate(DICT_TYPE_SQLITE, name, open_flags, dict_flags,
			       "%s:%s map requires O_RDONLY access mode",
			       DICT_TYPE_SQLITE, name));

    /*
     * Open the configuration file.
     */
    if ((parser = cfg_parser_alloc(name)) == 0)
	return (dict_surrogate(DICT_TYPE_SQLITE, name, open_flags, dict_flags,
			       "open %s: %m", name));

    dict_sqlite = (DICT_SQLITE *) dict_alloc(DICT_TYPE_SQLITE, name,
					     sizeof(DICT_SQLITE));
    dict_sqlite->dict.lookup = dict_sqlite_lookup;
    dict_sqlite->dict.close = dict_sqlite_close;
    dict_sqlite->dict.flags = dict_flags;

    dict_sqlite->parser = parser;
    dict_sqlite->stmt = 0;
    sqlite_parse_config(dict_sqlite, name);

    if (sqlite3_open_v2(dict_sqlite->dbpath, &dict_sqlite->db,
			SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
	DICT   *dict = dict_surrogate(DICT_TYPE_SQLITE, name, open_flags,
			      dict_flags, "%s:%s: open database %s: %s: %m",
				DICT_TYPE_SQLITE, name, dict_sqlite->dbpath,
				      sqlite3_errmsg(dict_sqlite->db));

	dict_sqlite_close(&dict_sqlite->dict);
	return dict;
    }
    dict_sqlite->dict.owner = cfg_get_owner(dict_sqlite->parser);

    return (&dict_sqlite->dict);
}

 /*
  * ====================================================================
  * Begin legacy code path: manual %letter quoting via dict_sqlite_quote().
  *
  * The function below is reachable only when statement_mode ==
  * SQLITE_STATEMENT_LEGACY. To retire legacy support, delete this
  * section, the corresponding branch in dict_sqlite_lookup() above,
  * the legacy-only call to flag_non_recommended_query() in
  * sqlite_parse_config(), and the statement_mode parameter handling.
  * ====================================================================
  */

/* dict_sqlite_lookup_legacy - find database entry, manual-quoting path */

static const char *dict_sqlite_lookup_legacy(DICT *dict, const char *name)
{
    const char *myname = "dict_sqlite_lookup_legacy";
    DICT_SQLITE *dict_sqlite = (DICT_SQLITE *) dict;
    sqlite3_stmt *sql_stmt;
    const char *query_remainder;
    static VSTRING *query;
    static VSTRING *result;
    const char *retval;
    int     expansion = 0;
    int     status;

    /* Expand the query and query the database. */
    INIT_VSTR(query, 10);

    if (!db_common_expand(dict_sqlite->ctx, dict_sqlite->query,
			  name, 0, query, dict_sqlite_quote))
	return (0);

    if (msg_verbose)
	msg_info("%s: %s: Searching with query %s",
		 myname, dict_sqlite->parser->name, vstring_str(query));

    if (sqlite3_prepare_v2(dict_sqlite->db, vstring_str(query), -1,
			   &sql_stmt, &query_remainder) != SQLITE_OK)
	msg_fatal("%s: %s: SQL prepare failed: %s\n",
		  myname, dict_sqlite->parser->name,
		  sqlite3_errmsg(dict_sqlite->db));

    if (*query_remainder && msg_verbose)
	msg_info("%s: %s: Ignoring text at end of query: %s",
		 myname, dict_sqlite->parser->name, query_remainder);

    /* Retrieve and expand the result(s). */
    INIT_VSTR(result, 10);
    while ((status = sqlite3_step(sql_stmt)) != SQLITE_DONE) {
	if (status == SQLITE_ROW) {
	    if (db_common_expand(dict_sqlite->ctx, dict_sqlite->result_format,
			    (const char *) sqlite3_column_text(sql_stmt, 0),
				 name, result, 0)
		&& dict_sqlite->expansion_limit > 0
		&& ++expansion > dict_sqlite->expansion_limit) {
		msg_warn("%s: %s: Expansion limit exceeded for key '%s'",
			 myname, dict_sqlite->parser->name, name);
		dict->error = DICT_ERR_RETRY;
		break;
	    }
	}
	/* Fix 20100616 */
	else {
	    msg_warn("%s: %s: SQL step failed for query '%s': %s\n",
		     myname, dict_sqlite->parser->name,
		     vstring_str(query), sqlite3_errmsg(dict_sqlite->db));
	    dict->error = DICT_ERR_RETRY;
	    break;
	}
    }

    /* Clean up. */
    if (sqlite3_finalize(sql_stmt))
	msg_fatal("%s: %s: SQL finalize failed for query '%s': %s\n",
		  myname, dict_sqlite->parser->name,
		  vstring_str(query), sqlite3_errmsg(dict_sqlite->db));

    return ((dict->error == 0 && *(retval = vstring_str(result)) != 0) ?
	    retval : 0);
}

 /*
  * ====================================================================
  * End legacy code path.
  * ====================================================================
  */

 /*
  * ====================================================================
  * Begin prepared-statement code path.
  *
  * Each %letter in the configured query becomes a SQLite parameter
  * marker (?N). The configured query is parsed once at open time
  * (sqlite_statement_alloc), prepared lazily on the first lookup
  * (sqlite_prepare_stmt, deferred so that sqlite3_open_v2() has run),
  * and reset+rebound on every subsequent lookup. Existing query
  * templates that quote substitutions ('%s', '%u', ...) work unchanged:
  * the rewriter recognises and replaces the surrounding quotes.
  * ====================================================================
  */

struct SQLITE_STATEMENT {
    char   *sql;			/* compiled query template */
    ARGV   *param_formats;		/* %s, %u, ... in bind order */
    VSTRING **param_bufs;		/* per-parameter value storage */
    sqlite3_stmt *handle;		/* prepared statement handle */
};

static int sqlite_stmt_param(const char ch)
{
    return (strchr("sudSUD123456789", ch) != 0);
}

static int sqlite_stmt_param_index(ARGV *param_formats, const char *format)
{
    int     i;

    for (i = 0; i < param_formats->argc; i++)
	if (strcmp(param_formats->argv[i], format) == 0)
	    return (i);
    return (-1);
}

static int sqlite_stmt_param_slot(ARGV *param_formats, const char *format)
{
    int     param_index;

    /* SQLite accepts ?N, so repeated placeholders can share one bind slot. */
    param_index = sqlite_stmt_param_index(param_formats, format);
    if (param_index < 0) {
	argv_add(param_formats, format, ARGV_END);
	param_index = param_formats->argc - 1;
    }
    return (param_index);
}

static void sqlite_stmt_append_param_ref(VSTRING *buf, int param_index,
					         int *pieces)
{
    if ((*pieces)++ > 0)
	vstring_strcat(buf, " || ");
    vstring_sprintf_append(buf, "?%d", param_index + 1);
}

static void sqlite_stmt_append_literal(VSTRING *buf, const char *text,
					       ssize_t len, int *pieces)
{
    if ((*pieces)++ > 0)
	vstring_strcat(buf, " || ");
    VSTRING_ADDCH(buf, '\'');
    vstring_strncat(buf, text, len);
    VSTRING_ADDCH(buf, '\'');
}

static void sqlite_stmt_append_quoted(VSTRING *sql, ARGV *param_formats,
					       const char *text, ssize_t len)
{
    int     param_index = -1;
    VSTRING *lit;
    const char *end = text + len;
    const char *cp;
    int     pieces = 0;

    /* Fast path for common whole-literal placeholders: '%u' -> ?1. */
    if (len == 2 && text[0] == '%' && sqlite_stmt_param(text[1])) {
	char    format[] = {'%', text[1], 0};

	param_index = sqlite_stmt_param_slot(param_formats, format);
	vstring_sprintf_append(sql, "?%d", param_index + 1);
	return;
    }
    lit = vstring_alloc(10);

    /*
     * Preserve legacy quoted-string use by rewriting mixed literals into
     * SQLite concatenation, for example 'prefix-%u' -> 'prefix-' || ?1.
     */
    for (cp = text; cp < end; cp++) {
	if (*cp == '\'' && cp + 1 < end && cp[1] == '\'') {
	    VSTRING_ADDCH(lit, *cp);
	    VSTRING_ADDCH(lit, *++cp);
	} else if (*cp == '%' && cp + 1 < end) {
	    if (cp[1] == '%') {
		VSTRING_ADDCH(lit, '%');
		cp += 1;
	    } else if (sqlite_stmt_param(cp[1])) {
		char    format[] = {'%', cp[1], 0};

		if (VSTRING_LEN(lit) > 0) {
		    VSTRING_TERMINATE(lit);
		    sqlite_stmt_append_literal(sql, vstring_str(lit),
					       VSTRING_LEN(lit), &pieces);
		    VSTRING_RESET(lit);
		    VSTRING_TERMINATE(lit);
		}

		param_index = sqlite_stmt_param_slot(param_formats, format);
		sqlite_stmt_append_param_ref(sql, param_index, &pieces);
		cp += 1;
	    } else {
		msg_fatal("unexpected '%.2s' in sqlite query template: %.*s",
			  cp, (int) len, text);
	    }
	} else {
	    VSTRING_ADDCH(lit, *cp);
	}
    }
    if (VSTRING_LEN(lit) > 0 || pieces == 0) {
	VSTRING_TERMINATE(lit);
	sqlite_stmt_append_literal(sql, vstring_str(lit),
				   VSTRING_LEN(lit), &pieces);
    }
    vstring_free(lit);
}

static SQLITE_STATEMENT *sqlite_statement_alloc(const char *mapname,
						        const char *query)
{
    SQLITE_STATEMENT *stmt;
    VSTRING *sql;
    ARGV   *param_formats;
    const char *cp;
    int     param_index;
    int     i;

    stmt = (SQLITE_STATEMENT *) mymalloc(sizeof(*stmt));
    sql = vstring_alloc(100);
    param_formats = argv_alloc(1);

    /*
     * Parse once at open time. Plain %x placeholders become ?N markers, and
     * quoted fragments are rewritten only when needed for legacy semantics.
     */
    for (cp = query; *cp; cp++) {
	if (*cp == '\'') {
	    const char *start;

	    for (start = ++cp; *cp; cp++) {
		if (*cp == '\'') {
		    if (cp[1] == '\'')
			cp += 1;
		    else
			break;
		}
	    }
	    if (*cp == 0)
		msg_fatal("%s: unterminated quote in query template: %s",
			  mapname, query);
	    sqlite_stmt_append_quoted(sql, param_formats, start, cp - start);
	} else if (*cp == '%') {
	    if (cp[1] == 0) {
		msg_fatal("%s: '%%' at end of query template: %s",
			  mapname, query);
	    } else if (cp[1] == '%') {
		VSTRING_ADDCH(sql, '%');
		cp += 1;
	    } else if (sqlite_stmt_param(cp[1])) {
		char    format[] = {'%', cp[1], 0};

		param_index = sqlite_stmt_param_slot(param_formats, format);
		vstring_sprintf_append(sql, "?%d", param_index + 1);
		cp += 1;
	    } else {
		msg_fatal("%s: unexpected '%.2s' in query template: %s",
			  mapname, cp, query);
	    }
	} else {
	    VSTRING_ADDCH(sql, *cp);
	}
    }
    VSTRING_TERMINATE(sql);

    stmt->sql = vstring_export(sql);
    stmt->param_formats = param_formats;
    stmt->handle = 0;
    /* No-placeholder templates avoid all per-parameter allocations. */
    if (param_formats->argc > 0) {
	stmt->param_bufs = (VSTRING **) mymalloc(param_formats->argc
						 * sizeof(*stmt->param_bufs));
	for (i = 0; i < param_formats->argc; i++)
	    stmt->param_bufs[i] = vstring_alloc(10);
    } else {
	stmt->param_bufs = 0;
    }
    return (stmt);
}

static void sqlite_statement_free(SQLITE_STATEMENT *stmt)
{
    int     i;

    if (stmt->handle)
	sqlite3_finalize(stmt->handle);
    if (stmt->sql)
	myfree(stmt->sql);
    if (stmt->param_bufs) {
	for (i = 0; i < stmt->param_formats->argc; i++)
	    vstring_free(stmt->param_bufs[i]);
	myfree((void *) stmt->param_bufs);
    }
    if (stmt->param_formats)
	argv_free(stmt->param_formats);
    myfree((void *) stmt);
}

static int sqlite_stmt_bind_value(DICT_SQLITE *dict_sqlite, const char *format,
				          const char *name, VSTRING *buf)
{
    /* Re-expand this placeholder for the current lookup key into its slot. */
    VSTRING_RESET(buf);
    VSTRING_TERMINATE(buf);
    return (db_common_expand(dict_sqlite->ctx, format, name, 0, buf,
			     (db_quote_callback_t) 0));
}

static int sqlite_stmt_bind(DICT_SQLITE *dict_sqlite, const char *name)
{
    SQLITE_STATEMENT *stmt = dict_sqlite->stmt;
    int     i;

    if (stmt->param_formats->argc == 0)
	return (1);

    /*
     * Match the legacy single-warning behaviour for an empty lookup key.
     * Without this guard, db_common_expand() would emit the same "empty
     * query string" warning once per placeholder slot.
     */
    if (*name == 0) {
	msg_warn("table \"%s:%s\": empty query string -- ignored",
		 dict_sqlite->dict.type, dict_sqlite->dict.name);
	return (0);
    }
    /* Fill every SQLite bind slot in the same order as sqlite_statement_alloc(). */
    for (i = 0; i < stmt->param_formats->argc; i++) {
	if (!sqlite_stmt_bind_value(dict_sqlite, stmt->param_formats->argv[i],
				    name, stmt->param_bufs[i]))
	    return (0);
	if (sqlite3_bind_text(stmt->handle, i + 1,
			      vstring_str(stmt->param_bufs[i]),
			      VSTRING_LEN(stmt->param_bufs[i]),
			      SQLITE_STATIC) != SQLITE_OK)
	    msg_fatal("%s: %s: SQL bind failed: %s",
		      __func__, dict_sqlite->parser->name,
		      sqlite3_errmsg(dict_sqlite->db));
    }
    return (1);
}

static int sqlite_prepare_stmt(DICT_SQLITE *dict_sqlite)
{
    const char *query_remainder;

    if (dict_sqlite->stmt->handle)
	return (1);
    if (sqlite3_prepare_v2(dict_sqlite->db, dict_sqlite->stmt->sql, -1,
			   &dict_sqlite->stmt->handle,
			   &query_remainder) != SQLITE_OK)
	msg_fatal("%s: %s: SQL prepare failed: %s",
		  __func__, dict_sqlite->parser->name,
		  sqlite3_errmsg(dict_sqlite->db));
    if (*query_remainder && msg_verbose)
	msg_info("%s: %s: Ignoring text at end of prepared query: %s",
		 __func__, dict_sqlite->parser->name, query_remainder);
    return (1);
}

/* dict_sqlite_lookup_prepared - find database entry, prepared-statement path */

static const char *dict_sqlite_lookup_prepared(DICT *dict, const char *name)
{
    const char *myname = "dict_sqlite_lookup_prepared";
    DICT_SQLITE *dict_sqlite = (DICT_SQLITE *) dict;
    sqlite3_stmt *sql_stmt;
    static VSTRING *result;
    const char *retval;
    int     expansion = 0;
    int     status;

    sqlite_prepare_stmt(dict_sqlite);
    sqlite3_reset(dict_sqlite->stmt->handle);
    sqlite3_clear_bindings(dict_sqlite->stmt->handle);
    if (!sqlite_stmt_bind(dict_sqlite, name))
	return (0);
    sql_stmt = dict_sqlite->stmt->handle;
    if (msg_verbose)
	msg_info("%s: %s: Searching with prepared query %s",
		 myname, dict_sqlite->parser->name, dict_sqlite->stmt->sql);

    /* Retrieve and expand the result(s). */
    INIT_VSTR(result, 10);
    while ((status = sqlite3_step(sql_stmt)) != SQLITE_DONE) {
	if (status == SQLITE_ROW) {
	    if (db_common_expand(dict_sqlite->ctx, dict_sqlite->result_format,
			    (const char *) sqlite3_column_text(sql_stmt, 0),
				 name, result, 0)
		&& dict_sqlite->expansion_limit > 0
		&& ++expansion > dict_sqlite->expansion_limit) {
		msg_warn("%s: %s: Expansion limit exceeded for key '%s'",
			 myname, dict_sqlite->parser->name, name);
		dict->error = DICT_ERR_RETRY;
		break;
	    }
	}
	/* Fix 20100616 */
	else {
	    msg_warn("%s: %s: SQL step failed for prepared query '%s': %s\n",
		     myname, dict_sqlite->parser->name,
		     dict_sqlite->stmt->sql, sqlite3_errmsg(dict_sqlite->db));
	    dict->error = DICT_ERR_RETRY;
	    break;
	}
    }

    /*
     * Reset (rather than finalize) so the prepared statement is ready
     * for the next lookup.
     */
    if (sqlite3_reset(sql_stmt) != SQLITE_OK)
	msg_fatal("%s: %s: SQL reset failed for prepared query '%s': %s\n",
		  myname, dict_sqlite->parser->name,
		  dict_sqlite->stmt->sql, sqlite3_errmsg(dict_sqlite->db));

    return ((dict->error == 0 && *(retval = vstring_str(result)) != 0) ?
	    retval : 0);
}

 /*
  * ====================================================================
  * End prepared-statement code path.
  * ====================================================================
  */

#endif
