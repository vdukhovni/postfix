/*++
/* NAME
/*	dict_pgsql 3
/* SUMMARY
/*	dictionary manager interface to PostgreSQL databases
/* SYNOPSIS
/*	#include <dict_pgsql.h>
/*
/*	DICT	*dict_pgsql_open(name, open_flags, dict_flags)
/*	const char *name;
/*	int	open_flags;
/*	int	dict_flags;
/* DESCRIPTION
/*	dict_pgsql_open() creates a dictionary of type 'pgsql'.  This
/*	dictionary is an interface for the postfix key->value mappings
/*	to pgsql.  The result is a pointer to the installed dictionary,
/*	or a null pointer in case of problems.
/*
/*	The pgsql dictionary can manage multiple connections to
/*	different sql servers for the same database.  It assumes that
/*	the underlying data on each server is identical (mirrored) and
/*	maintains one connection at any given time.  If any connection
/*	fails,  any other available ones will be opened and used.
/*	The intent of this feature is to eliminate a single point of
/*	failure for mail systems that would otherwise rely on a single
/*	pgsql server.
/* .PP
/*	Arguments:
/* .IP name
/*	Either the path to the PostgreSQL configuration file (if it
/*	starts with '/' or '.'), or the prefix which will be used to
/*	obtain main.cf configuration parameters for this search.
/*
/*	In the first case, the configuration parameters below are
/*	specified in the file as \fIname\fR=\fIvalue\fR pairs.
/*
/*	In the second case, the configuration parameters are
/*	prefixed with the value of \fIname\fR and an underscore,
/*	and they are specified in main.cf.  For example, if this
/*	value is \fIpgsqlsource\fR, the parameters would look like
/*	\fIpgsqlsource_user\fR, \fIpgsqlsource_table\fR, and so on.
/* .IP other_name
/*	reference for outside use.
/* .IP open_flags
/*	Must be O_RDONLY.
/* .IP dict_flags
/*	See dict_open(3).
/* SEE ALSO
/*	dict(3) generic dictionary manager
/*	pgsql_table(5) PostgreSQL client configuration
/* AUTHOR(S)
/*	Aaron Sethman
/*	androsyn@ratbox.org
/*
/*	Based upon dict_mysql.c by
/*
/*	Scott Cotton
/*	IC Group, Inc.
/*	scott@icgroup.com
/*
/*	Joshua Marcus
/*	IC Group, Inc.
/*	josh@icgroup.com
/*
/*	Prepared statement support by
/*	Ömer Güven
/*--*/

/* System library. */

#include "sys_defs.h"

#ifdef HAS_PGSQL
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <time.h>

#include <postgres_ext.h>
#include <libpq-fe.h>

/* Utility library. */

#include "dict.h"
#include "msg.h"
#include "mymalloc.h"
#include "argv.h"
#include "vstring.h"
#include "split_at.h"
#include "myrand.h"
#include "events.h"
#include "hash_fnv.h"
#include "stringops.h"
#include "valid_uri_scheme.h"

/* Global library. */

#include "cfg_parser.h"
#include "db_common.h"

/* Application-specific. */

#include "dict_pgsql.h"

#define STATACTIVE			(1<<0)
#define STATFAIL			(1<<1)
#define STATUNTRIED			(1<<2)

#define TYPEUNIX			(1<<0)
#define TYPEINET			(1<<1)
#define TYPECONNSTR			(1<<2)

#define RETRY_CONN_MAX			100
#define DEF_RETRY_INTV			60	/* 1 minute */
#define DEF_IDLE_INTV			60	/* 1 minute */

#define PGSQL_STATEMENT_LEGACY		0
#define PGSQL_STATEMENT_PARAMETERIZED	1
#define PGSQL_STATEMENT_PREPARED	2

typedef struct PGSQL_STATEMENT PGSQL_STATEMENT;	/* defined below */

typedef struct {
    PGconn *db;
    char   *hostname;
    char   *name;
    char   *port;
    unsigned type;			/* TYPEUNIX | TYPEINET | TYPECONNSTR */
    unsigned stat;			/* STATUNTRIED | STATFAIL | STATCUR */
    time_t  ts;				/* used for attempting reconnection */
    int     stmt_ready;			/* prepared statement is ready */
} HOST;

typedef struct {
    int     len_hosts;			/* number of hosts */
    HOST  **db_hosts;			/* hosts on which databases reside */
    char   *non_uri_target;		/* require dbname to be specified */
} PLPGSQL;

typedef struct {
    DICT    dict;
    CFG_PARSER *parser;
    char   *query;
    char   *result_format;
    void   *ctx;
    int     expansion_limit;
    char   *username;
    char   *password;
    char   *dbname;
    char   *encoding;
    int     retry_interval;
    int     idle_interval;
    char   *table;
    ARGV   *hosts;
    PLPGSQL *pldb;
    HOST   *active_host;
    int     statement_mode;		/* PGSQL_STATEMENT_{LEGACY,...} */
    PGSQL_STATEMENT *stmt;		/* compiled query, non-legacy modes */
} DICT_PGSQL;


/* Just makes things a little easier for me.. */
#define PGSQL_RES PGresult

#define INIT_VSTR(buf, len) do { \
	if (buf == 0) \
	    buf = vstring_alloc(len); \
	VSTRING_RESET(buf); \
	VSTRING_TERMINATE(buf); \
    } while (0)

/* internal function declarations */
static PLPGSQL *plpgsql_init(ARGV *);
static PGSQL_RES *plpgsql_query_legacy(DICT_PGSQL *, const char *, VSTRING *);
static void plpgsql_dealloc(PLPGSQL *);
static void plpgsql_close_host(HOST *);
static void plpgsql_down_host(HOST *, int);
static void plpgsql_connect_single(DICT_PGSQL *, HOST *);
static const char *dict_pgsql_lookup(DICT *, const char *);
DICT   *dict_pgsql_open(const char *, int, int);
static void dict_pgsql_close(DICT *);
static HOST *host_init(const char *);

 /*
  * Lookup variants. dict_pgsql_lookup() does the per-key checks shared
  * between the modes (UTF-8, key folding, domain filter), then dispatches
  * to one of the implementations below. The legacy and prepared
  * implementations are at the bottom of this file, well separated, so a
  * future cleanup that retires legacy support can delete the legacy
  * block, the dispatcher branch that calls it, plpgsql_query_legacy(),
  * dict_pgsql_quote(), and the statement_mode parameter handling.
  */
static const char *dict_pgsql_lookup_legacy(DICT *, const char *);
static const char *dict_pgsql_lookup_prepared(DICT *, const char *);
static PGSQL_STATEMENT *pgsql_statement_alloc(const char *, const char *);
static void pgsql_statement_free(PGSQL_STATEMENT *);

/* dict_pgsql_quote - escape SQL metacharacters in input string */

static void dict_pgsql_quote(DICT *dict, const char *name, VSTRING *result)
{
    DICT_PGSQL *dict_pgsql = (DICT_PGSQL *) dict;
    HOST   *active_host = dict_pgsql->active_host;
    char   *myname = "dict_pgsql_quote";
    size_t  len = strlen(name);
    size_t  buflen;
    int     err = 1;

    if (active_host == 0)
	msg_panic("%s: bogus dict_pgsql->active_host", myname);

    /*
     * We won't get arithmetic overflows in 2*len + 1, because Postfix input
     * keys have reasonable size limits, better safe than sorry.
     */
    if (len > (SSIZE_T_MAX - VSTRING_LEN(result) - 1) / 2)
	msg_panic("%s: arithmetic overflow in %lu+2*%lu+1",
		  myname, (unsigned long) VSTRING_LEN(result),
		  (unsigned long) len);
    buflen = 2 * len + 1;

    /*
     * 202604 Claude: error recovery strategy needs clarification.
     * 
     * When the PQescapeStringConn() call below fails, we flag the host via
     * active_host->stat = STATFAIL and return without writing to `result`.
     * plpgsql_query() examines host->stat immediately after
     * db_common_expand() returns, calls plpgsql_down_host(), and tries the
     * next available server (i.e. not already flagged as STATFAIL).
     * dict_pgsql_lookup() sets dict->error = DICT_ERR_RETRY if all servers
     * fail.
     */
    if (active_host->stat == STATFAIL)		/* Can't happen */
	return;

    /*
     * Escape the input string, using PQescapeStringConn(), because the older
     * PQescapeString() is not safe anymore, as stated by the documentation.
     * 
     * From current libpq (8.1.4) documentation:
     * 
     * PQescapeStringConn writes an escaped version of the from string to the to
     * buffer, escaping special characters so that they cannot cause any
     * harm, and adding a terminating zero byte.
     * 
     * ...
     * 
     * The parameter from points to the first character of the string that is to
     * be escaped, and the length parameter gives the number of bytes in this
     * string. A terminating zero byte is not required, and should not be
     * counted in length.
     * 
     * ...
     * 
     * (The parameter) to shall point to a buffer that is able to hold at least
     * one more byte than twice the value of length, otherwise the behavior
     * is undefined.
     * 
     * ...
     * 
     * If the error parameter is not NULL, then *error is set to zero on
     * success, nonzero on error ... The output string is still generated on
     * error, but it can be expected that the server will reject it as
     * malformed. On error, a suitable message is stored in the conn object,
     * whether or not error is NULL.
     */
    VSTRING_SPACE(result, buflen);
    PQescapeStringConn(active_host->db, vstring_end(result), name, len, &err);
    if (err == 0) {
	VSTRING_SKIP(result);
    } else {

	/*
	 * PQescapeStringConn() failed. According to the docs, we still have
	 * a valid, null-terminated output string, but we need not rely on
	 * this behavior.
	 */
	msg_warn("dict pgsql: (host %s) cannot escape input string: %s",
		 active_host->hostname, PQerrorMessage(active_host->db));
	active_host->stat = STATFAIL;
	VSTRING_TERMINATE(result);
    }
}

/* dict_pgsql_lookup - find database entry */

static const char *dict_pgsql_lookup(DICT *dict, const char *name)
{
    const char *myname = "dict_pgsql_lookup";
    DICT_PGSQL *dict_pgsql = (DICT_PGSQL *) dict;
    int     domain_rc;

    dict->error = 0;

    /*
     * Don't frustrate future attempts to make Postfix UTF-8 transparent.
     */
#ifdef SNAPSHOT
    if ((dict->flags & DICT_FLAG_UTF8_ACTIVE) == 0
	&& !valid_utf8_stringz(name)) {
	if (msg_verbose)
	    msg_info("%s: %s: Skipping lookup of non-UTF-8 key '%s'",
		     myname, dict_pgsql->parser->name, name);
	return (0);
    }
#endif

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
     * If there is a domain list for this map, then only search for addresses
     * in domains on the list. This can significantly reduce the load on the
     * server.
     */
    if ((domain_rc = db_common_check_domain(dict_pgsql->ctx, name)) == 0) {
	if (msg_verbose)
	    msg_info("%s: Skipping lookup of '%s'", myname, name);
	return (0);
    }
    if (domain_rc < 0)
	DICT_ERR_VAL_RETURN(dict, domain_rc, (char *) 0);

    /* Dispatch to the legacy or prepared implementation. */
    return ((dict_pgsql->statement_mode == PGSQL_STATEMENT_LEGACY) ?
	    dict_pgsql_lookup_legacy(dict, name) :
	    dict_pgsql_lookup_prepared(dict, name));
}

/* dict_pgsql_check_stat - check the status of a host */

static int dict_pgsql_check_stat(HOST *host, unsigned stat, unsigned type,
				         time_t t)
{
    if ((host->stat & stat) && (!type || host->type & type)) {
	/* try not to hammer the dead hosts too often */
	if (host->stat == STATFAIL && host->ts > 0 && host->ts >= t)
	    return 0;
	return 1;
    }
    return 0;
}

/* dict_pgsql_find_host - find a host with the given status */

static HOST *dict_pgsql_find_host(PLPGSQL *PLDB, unsigned stat, unsigned type)
{
    time_t  t;
    int     count = 0;
    int     idx;
    int     i;

    t = time((time_t *) 0);
    for (i = 0; i < PLDB->len_hosts; i++) {
	if (dict_pgsql_check_stat(PLDB->db_hosts[i], stat, type, t))
	    count++;
    }

    if (count) {
	idx = (count > 1) ?
	    1 + count * (double) myrand() / (1.0 + RAND_MAX) : 1;

	for (i = 0; i < PLDB->len_hosts; i++) {
	    if (dict_pgsql_check_stat(PLDB->db_hosts[i], stat, type, t) &&
		--idx == 0)
		return PLDB->db_hosts[i];
	}
    }
    return 0;
}

/* dict_pgsql_get_active - get an active connection */

static HOST *dict_pgsql_get_active(DICT_PGSQL *dict_pgsql, PLPGSQL *PLDB)
{
    const char *myname = "dict_pgsql_get_active";
    HOST   *host;
    int     count = RETRY_CONN_MAX;

    /* try the active connections first; prefer the ones to UNIX sockets */
    if ((host = dict_pgsql_find_host(PLDB, STATACTIVE, TYPEUNIX)) != NULL ||
	(host = dict_pgsql_find_host(PLDB, STATACTIVE, TYPEINET)) != NULL ||
     (host = dict_pgsql_find_host(PLDB, STATACTIVE, TYPECONNSTR)) != NULL) {
	if (msg_verbose)
	    msg_info("%s: found active connection to host %s", myname,
		     host->hostname);
	return host;
    }

    /*
     * Try the remaining hosts. "count" is a safety net, in case the loop
     * takes more than RETRY_CONN_INTV and the dead hosts are no longer
     * skipped.
     */
    while (--count > 0 &&
	   ((host = dict_pgsql_find_host(PLDB, STATUNTRIED | STATFAIL,
					 TYPEUNIX)) != NULL ||
	    (host = dict_pgsql_find_host(PLDB, STATUNTRIED | STATFAIL,
					 TYPEINET)) != NULL ||
	    (host = dict_pgsql_find_host(PLDB, STATUNTRIED | STATFAIL,
					 TYPECONNSTR)) != NULL)) {
	if (msg_verbose)
	    msg_info("%s: attempting to connect to host %s", myname,
		     host->hostname);
	plpgsql_connect_single(dict_pgsql, host);
	if (host->stat == STATACTIVE)
	    return host;
    }

    /* bad news... */
    return 0;
}

/* dict_pgsql_event - callback: close idle connections */

static void dict_pgsql_event(int unused_event, void *context)
{
    HOST   *host = (HOST *) context;

    if (host->db)
	plpgsql_close_host(host);
}

/*
 * plpgsql_query_legacy - process a PostgreSQL query.  Return PGSQL_RES* on
 *			success.  On failure, log failure and try other db
 *			instances.  on failure of all db instances, return
 *			0; close unnecessary active connections.
 */

static PGSQL_RES *plpgsql_query_legacy(DICT_PGSQL *dict_pgsql,
				               const char *name,
				               VSTRING *query)
{
    PLPGSQL *PLDB = dict_pgsql->pldb;
    HOST   *host;
    PGSQL_RES *res = 0;
    ExecStatusType status;

    while ((host = dict_pgsql_get_active(dict_pgsql, PLDB)) != NULL) {

	/*
	 * The active host is used to escape strings in the context of the
	 * active connection's character encoding.
	 */
	dict_pgsql->active_host = host;
	VSTRING_RESET(query);
	VSTRING_TERMINATE(query);
	db_common_expand(dict_pgsql->ctx, dict_pgsql->query,
			 name, 0, query, dict_pgsql_quote);
	dict_pgsql->active_host = 0;

	/* Check for potential dict_pgsql_quote() failure. */
	if (host->stat == STATFAIL) {
	    plpgsql_down_host(host, dict_pgsql->retry_interval);
	    continue;
	}

	/*
	 * Submit a command to the server. Be paranoid when processing the
	 * result set: try to enumerate every successful case, and reject
	 * everything else.
	 * 
	 * From PostgreSQL 8.1.4 docs: (PQexec) returns a PGresult pointer or
	 * possibly a null pointer. A non-null pointer will generally be
	 * returned except in out-of-memory conditions or serious errors such
	 * as inability to send the command to the server.
	 */
	if ((res = PQexec(host->db, vstring_str(query))) != 0) {

	    /*
	     * XXX Because non-null result pointer does not imply success, we
	     * need to check the command's result status.
	     * 
	     * Section 28.3.1: A result of status PGRES_NONFATAL_ERROR will
	     * never be returned directly by PQexec or other query execution
	     * functions; results of this kind are instead passed to the
	     * notice processor.
	     * 
	     * PGRES_EMPTY_QUERY is being sent by the server when the query
	     * string is empty. The sanity-checking done by the Postfix
	     * infrastructure makes this case impossible, so we need not
	     * handle this situation explicitly.
	     */
	    switch ((status = PQresultStatus(res))) {
	    case PGRES_TUPLES_OK:
	    case PGRES_COMMAND_OK:
		/* Success. */
		if (msg_verbose)
		    msg_info("dict_pgsql: successful query from host %s",
			     host->hostname);
		event_request_timer(dict_pgsql_event, (void *) host,
				    dict_pgsql->idle_interval);
		return (res);
	    case PGRES_FATAL_ERROR:
		msg_warn("pgsql query failed: fatal error from host %s: %s",
			 host->hostname, PQresultErrorMessage(res));
		break;
	    case PGRES_BAD_RESPONSE:
		msg_warn("pgsql query failed: protocol error, host %s",
			 host->hostname);
		break;
	    default:
		msg_warn("pgsql query failed: unknown code 0x%lx from host %s",
			 (unsigned long) status, host->hostname);
		break;
	    }
	} else {

	    /*
	     * This driver treats null pointers like fatal, non-null result
	     * pointer errors, as suggested by the PostgreSQL 8.1.4
	     * documentation.
	     */
	    msg_warn("pgsql query failed: fatal error from host %s: %s",
		     host->hostname, PQerrorMessage(host->db));
	}

	/*
	 * XXX An error occurred. Clean up memory and skip this connection.
	 */
	if (res != 0)
	    PQclear(res);
	plpgsql_down_host(host, dict_pgsql->retry_interval);
    }

    return (0);
}

/*
 * plpgsql_connect_single -
 * used to reconnect to a single database when one is down or none is
 * connected yet. Log all errors and set the stat field of host accordingly
 */
static void plpgsql_connect_single(DICT_PGSQL *dict_pgsql, HOST *host)
{
    if (host->type == TYPECONNSTR) {
	host->db = PQconnectdb(host->name);
    } else {
	host->db = PQsetdbLogin(host->name, host->port, NULL, NULL,
				dict_pgsql->dbname, dict_pgsql->username,
				dict_pgsql->password);
    }
    if (host->db == NULL || PQstatus(host->db) != CONNECTION_OK) {
	/* 202604 Claude: don't call PQerrorMessage(NULL). */
	msg_warn("connect to pgsql server %s: %s",
		 host->hostname, host->db ? PQerrorMessage(host->db) :
		 "PQconnectdb or PQsetdbLogin failed");
	plpgsql_down_host(host, dict_pgsql->retry_interval);
	return;
    }
    if (PQsetClientEncoding(host->db, dict_pgsql->encoding) != 0) {
	msg_warn("dict_pgsql: cannot set the encoding to %s, skipping %s",
		 dict_pgsql->encoding, host->hostname);
	plpgsql_down_host(host, dict_pgsql->retry_interval);
	return;
    }
    if (msg_verbose)
	msg_info("dict_pgsql: successful connection to host %s",
		 host->hostname);
    /* Success. */
    host->stat = STATACTIVE;
}

/* plpgsql_close_host - close an established PostgreSQL connection */

static void plpgsql_close_host(HOST *host)
{
    if (host->db)
	PQfinish(host->db);
    host->db = 0;
    host->stat = STATUNTRIED;
    host->stmt_ready = 0;
}

/*
 * plpgsql_down_host - close a failed connection AND set a "stay away from
 * this host" timer.
 */
static void plpgsql_down_host(HOST *host, int retry_interval)
{
    if (host->db)
	PQfinish(host->db);
    host->db = 0;
    host->ts = time((time_t *) 0) + retry_interval;
    host->stat = STATFAIL;
    host->stmt_ready = 0;
    event_cancel_timer(dict_pgsql_event, (void *) host);
}

/* pgsql_parse_config - parse pgsql configuration file */

static void pgsql_parse_config(DICT_PGSQL *dict_pgsql, const char *pgsqlcf)
{
    const char *myname = "pgsql_parse_config";
    CFG_PARSER *p = dict_pgsql->parser;
    char   *hosts;
    VSTRING *query;
    char   *select_function;
    char   *statement_mode;

    dict_pgsql->username = cfg_get_str(p, "user", "", 0, 0);
    dict_pgsql->password = cfg_get_str(p, "password", "", 0, 0);
    dict_pgsql->dbname = cfg_get_str(p, "dbname", "", 0, 0);
    dict_pgsql->encoding = cfg_get_str(p, "encoding", "UTF8", 1, 0);
    dict_pgsql->retry_interval = cfg_get_int(p, "retry_interval",
					     DEF_RETRY_INTV, 1, 0);
    dict_pgsql->idle_interval = cfg_get_int(p, "idle_interval",
					    DEF_IDLE_INTV, 1, 0);
    dict_pgsql->result_format = cfg_get_str(p, "result_format", "%s", 1, 0);
    statement_mode = cfg_get_str(p, "statement_mode", "legacy", 1, 0);
    if (strcasecmp(statement_mode, "legacy") == 0) {
	dict_pgsql->statement_mode = PGSQL_STATEMENT_LEGACY;
    } else if (strcasecmp(statement_mode, "parameterized") == 0) {
	dict_pgsql->statement_mode = PGSQL_STATEMENT_PARAMETERIZED;
    } else if (strcasecmp(statement_mode, "prepared") == 0) {
	dict_pgsql->statement_mode = PGSQL_STATEMENT_PREPARED;
    } else {
	msg_fatal("%s: %s: bad statement_mode value '%s'; specify "
		  "'legacy', 'parameterized', or 'prepared'",
		  myname, pgsqlcf, statement_mode);
    }
    myfree(statement_mode);

    /*
     * XXX: The default should be non-zero for safety, but that is not
     * backwards compatible.
     */
    dict_pgsql->expansion_limit = cfg_get_int(dict_pgsql->parser,
					      "expansion_limit", 0, 0, 0);

    if ((dict_pgsql->query = cfg_get_str(p, "query", 0, 0, 0)) == 0) {

	/*
	 * No query specified -- fallback to building it from components (
	 * old style "select %s from %s where %s" )
	 */
	query = vstring_alloc(64);
	select_function = cfg_get_str(p, "select_function", 0, 0, 0);
	if (select_function != 0) {
	    vstring_sprintf(query, "SELECT %s('%%s')", select_function);
	    myfree(select_function);
	} else
	    db_common_sql_build_query(query, p);
	dict_pgsql->query = vstring_export(query);
    }

    /*
     * Must parse all templates before we can use db_common_expand()
     */
    dict_pgsql->ctx = 0;
    (void) db_common_parse(&dict_pgsql->dict, &dict_pgsql->ctx,
			   dict_pgsql->query, 1);
    (void) db_common_parse(0, &dict_pgsql->ctx, dict_pgsql->result_format, 0);
    db_common_parse_domain(p, dict_pgsql->ctx);

    /*
     * In a non-legacy mode, parse the query template into a PGSQL_STATEMENT
     * once at open time. The actual PQprepare() call (if any) is deferred
     * to the first lookup against each host, so that the connection has
     * been established first.
     */
    if (dict_pgsql->statement_mode != PGSQL_STATEMENT_LEGACY)
	dict_pgsql->stmt = pgsql_statement_alloc(dict_pgsql->parser->name,
						 dict_pgsql->query);

    /*
     * Maps that use substring keys should only be used with the full input
     * key.
     */
    if (db_common_dict_partial(dict_pgsql->ctx))
	dict_pgsql->dict.flags |= DICT_FLAG_PATTERN;
    else
	dict_pgsql->dict.flags |= DICT_FLAG_FIXED;
    if (dict_pgsql->dict.flags & DICT_FLAG_FOLD_FIX)
	dict_pgsql->dict.fold_buf = vstring_alloc(10);

    hosts = cfg_get_str(p, "hosts", "", 0, 0);

    dict_pgsql->hosts = argv_split(hosts, CHARS_COMMA_SP);
    if (dict_pgsql->hosts->argc == 0) {
	argv_add(dict_pgsql->hosts, "localhost", ARGV_END);
	argv_terminate(dict_pgsql->hosts);
	if (msg_verbose)
	    msg_info("%s: %s: no hostnames specified, defaulting to '%s'",
		     myname, pgsqlcf, dict_pgsql->hosts->argv[0]);
    }
    /* Don't blacklist the load balancer! */
    if (dict_pgsql->hosts->argc == 1)
	argv_add(dict_pgsql->hosts, dict_pgsql->hosts->argv[0], (char *) 0);
    myfree(hosts);
}

/* dict_pgsql_open - open PGSQL data base */

DICT   *dict_pgsql_open(const char *name, int open_flags, int dict_flags)
{
    DICT_PGSQL *dict_pgsql;
    CFG_PARSER *parser;

    /*
     * Sanity check.
     */
    if (open_flags != O_RDONLY)
	return (dict_surrogate(DICT_TYPE_PGSQL, name, open_flags, dict_flags,
			       "%s:%s map requires O_RDONLY access mode",
			       DICT_TYPE_PGSQL, name));

    /*
     * Open the configuration file.
     */
    if ((parser = cfg_parser_alloc(name)) == 0)
	return (dict_surrogate(DICT_TYPE_PGSQL, name, open_flags, dict_flags,
			       "open %s: %m", name));

    dict_pgsql = (DICT_PGSQL *) dict_alloc(DICT_TYPE_PGSQL, name,
					   sizeof(DICT_PGSQL));
    dict_pgsql->dict.lookup = dict_pgsql_lookup;
    dict_pgsql->dict.close = dict_pgsql_close;
    dict_pgsql->dict.flags = dict_flags;
    dict_pgsql->parser = parser;
    dict_pgsql->statement_mode = PGSQL_STATEMENT_LEGACY;
    dict_pgsql->stmt = 0;
    pgsql_parse_config(dict_pgsql, name);
    dict_pgsql->active_host = 0;
    dict_pgsql->pldb = plpgsql_init(dict_pgsql->hosts);
    if (dict_pgsql->pldb == NULL)
	msg_fatal("couldn't initialize pldb!\n");
    if (msg_verbose && dict_pgsql->pldb->non_uri_target == 0
	&& dict_pgsql->dbname[0] != 0)
	msg_info("%s:%s table ignores 'dbname' field -- "
		 "all 'hosts' targets are URIs",
		 DICT_TYPE_PGSQL, name);
    if (dict_pgsql->pldb->non_uri_target && dict_pgsql->dbname[0] == 0) {
	DICT   *ret;

	ret = dict_surrogate(DICT_TYPE_PGSQL, name, open_flags, dict_flags,
			   "%s:%s host target '%s' requires dbname setting",
			     DICT_TYPE_PGSQL, name,
			     dict_pgsql->pldb->non_uri_target);
	dict_pgsql_close(&dict_pgsql->dict);
	return (ret);
    }
    dict_pgsql->dict.owner = cfg_get_owner(dict_pgsql->parser);
    return (&dict_pgsql->dict);
}

/* plpgsql_init - initialize a PGSQL database */

static PLPGSQL *plpgsql_init(ARGV *hosts)
{
    PLPGSQL *PLDB;
    int     i;

    PLDB = (PLPGSQL *) mymalloc(sizeof(PLPGSQL));
    PLDB->len_hosts = hosts->argc;
    PLDB->db_hosts = (HOST **) mymalloc(sizeof(HOST *) * hosts->argc);
    PLDB->non_uri_target = 0;
    for (i = 0; i < hosts->argc; i++) {
	PLDB->db_hosts[i] = host_init(hosts->argv[i]);
	if (PLDB->db_hosts[i]->type != TYPECONNSTR)
	    PLDB->non_uri_target = PLDB->db_hosts[i]->name;
    }

    return PLDB;
}


/* host_init - initialize HOST structure */

static HOST *host_init(const char *hostname)
{
    const char *myname = "pgsql host_init";
    HOST   *host = (HOST *) mymalloc(sizeof(HOST));
    const char *d = hostname;

    host->db = 0;
    host->hostname = mystrdup(hostname);
    host->stat = STATUNTRIED;
    host->ts = 0;
    host->stmt_ready = 0;

    /*
     * Modern syntax: connection URI.
     */
    if (valid_uri_scheme(d)) {
	host->type = TYPECONNSTR;
	host->name = mystrdup(d);
	host->port = 0;
    }

    /*
     * Historical syntax: "unix:/pathname" and "inet:host:port". Strip the
     * "unix:" and "inet:" prefixes. Look at the first character, which is
     * how PgSQL historically distinguishes between UNIX and INET.
     */
    else {
	if (strncmp(d, "unix:", 5) == 0 || strncmp(d, "inet:", 5) == 0)
	    d += 5;
	host->name = mystrdup(d);
	if (host->name[0] && host->name[0] != '/') {
	    host->type = TYPEINET;
	    host->port = split_at_right(host->name, ':');
	} else {
	    host->type = TYPEUNIX;
	    host->port = 0;
	}
    }
    if (msg_verbose > 1)
	msg_info("%s: host=%s, port=%s, type=%s", myname, host->name,
		 host->port ? host->port : "",
		 host->type == TYPEUNIX ? "unix" :
		 host->type == TYPEINET ? "inet" :
		 "uri");
    return host;
}

/* dict_pgsql_close - close PGSQL data base */

static void dict_pgsql_close(DICT *dict)
{
    DICT_PGSQL *dict_pgsql = (DICT_PGSQL *) dict;

    plpgsql_dealloc(dict_pgsql->pldb);
    cfg_parser_free(dict_pgsql->parser);
    myfree(dict_pgsql->username);
    myfree(dict_pgsql->password);
    myfree(dict_pgsql->dbname);
    myfree(dict_pgsql->encoding);
    myfree(dict_pgsql->query);
    myfree(dict_pgsql->result_format);
    if (dict_pgsql->stmt)
	pgsql_statement_free(dict_pgsql->stmt);
    if (dict_pgsql->hosts)
	argv_free(dict_pgsql->hosts);
    if (dict_pgsql->ctx)
	db_common_free_ctx(dict_pgsql->ctx);
    if (dict->fold_buf)
	vstring_free(dict->fold_buf);
    dict_free(dict);
}

/* plpgsql_dealloc - free memory associated with PLPGSQL close databases */

static void plpgsql_dealloc(PLPGSQL *PLDB)
{
    int     i;

    for (i = 0; i < PLDB->len_hosts; i++) {
	event_cancel_timer(dict_pgsql_event, (void *) (PLDB->db_hosts[i]));
	if (PLDB->db_hosts[i]->db)
	    PQfinish(PLDB->db_hosts[i]->db);
	myfree(PLDB->db_hosts[i]->hostname);
	myfree(PLDB->db_hosts[i]->name);
	myfree((void *) PLDB->db_hosts[i]);
    }
    myfree((void *) PLDB->db_hosts);
    myfree((void *) (PLDB));
}

 /*
  * ====================================================================
  * Begin legacy code path: manual %letter quoting via dict_pgsql_quote().
  *
  * The function below is reachable only when statement_mode ==
  * PGSQL_STATEMENT_LEGACY. To retire legacy support, delete this
  * section, the corresponding branch in dict_pgsql_lookup() above,
  * dict_pgsql_quote(), plpgsql_query_legacy(), and the statement_mode
  * parameter handling.
  * ====================================================================
  */

/* dict_pgsql_lookup_legacy - find database entry, manual-quoting path */

static const char *dict_pgsql_lookup_legacy(DICT *dict, const char *name)
{
    const char *myname = "dict_pgsql_lookup_legacy";
    DICT_PGSQL *dict_pgsql = (DICT_PGSQL *) dict;
    PGSQL_RES *query_res;
    static VSTRING *query;
    static VSTRING *result;
    int     i;
    int     j;
    int     numrows;
    int     numcols;
    int     expansion;
    const char *r;

    INIT_VSTR(query, 10);
    INIT_VSTR(result, 10);

    /*
     * Suppress the actual lookup if the expansion is empty.
     *
     * This initial expansion is outside the context of any specific host
     * connection, we just want to check the key pre-requisites, so when
     * quoting happens separately for each connection, we don't bother with
     * quoting...
     */
    if (!db_common_expand(dict_pgsql->ctx, dict_pgsql->query,
			  name, 0, query, 0))
	return (0);

    /* do the query - set dict->error & cleanup if there's an error */
    if ((query_res = plpgsql_query_legacy(dict_pgsql, name, query)) == 0) {
	dict->error = DICT_ERR_RETRY;
	return 0;
    }
    numrows = PQntuples(query_res);
    if (msg_verbose)
	msg_info("%s: retrieved %d rows", myname, numrows);
    if (numrows == 0) {
	PQclear(query_res);
	return 0;
    }
    numcols = PQnfields(query_res);

    for (expansion = i = 0; i < numrows && dict->error == 0; i++) {
	for (j = 0; j < numcols; j++) {
	    r = PQgetvalue(query_res, i, j);
	    if (db_common_expand(dict_pgsql->ctx, dict_pgsql->result_format,
				 r, name, result, 0)
		&& dict_pgsql->expansion_limit > 0
		&& ++expansion > dict_pgsql->expansion_limit) {
		msg_warn("%s: %s: Expansion limit exceeded for key: '%s'",
			 myname, dict_pgsql->parser->name, name);
		dict->error = DICT_ERR_RETRY;
		break;
	    }
	}
    }
    PQclear(query_res);
    r = vstring_str(result);
    return ((dict->error == 0 && *r) ? r : 0);
}

 /*
  * ====================================================================
  * End legacy code path.
  * ====================================================================
  */

 /*
  * ====================================================================
  * Begin prepared/parameterized code path.
  *
  * Each %letter in the configured query becomes a PostgreSQL $N
  * parameter. The configured query is parsed once at open time
  * (pgsql_statement_alloc), and the rewritten SQL is sent either via
  * PQexecParams (parameterized mode -- no server-side prepare) or via
  * a one-time PQprepare per host followed by PQexecPrepared (prepared
  * mode). Existing query templates that quote substitutions ('%s',
  * '%u', ...) work unchanged: the rewriter recognises and replaces
  * the surrounding quotes.
  * ====================================================================
  */

struct PGSQL_STATEMENT {
    char   *sql;			/* normalised SQL with $N markers */
    char   *name;			/* prepared statement name (PREPARED) */
    ARGV   *param_formats;		/* %s, %u, ... in $1..$N order */
    VSTRING **param_bufs;		/* per-parameter value storage */
    const char **param_values;		/* PQexec*() argument vector */
};

static int pgsql_stmt_param(const char ch)
{
    return (strchr("sudSUD123456789", ch) != 0);
}

static int pgsql_stmt_param_index(ARGV *param_formats, const char *format)
{
    int     i;

    for (i = 0; i < param_formats->argc; i++)
	if (strcmp(param_formats->argv[i], format) == 0)
	    return (i);
    return (-1);
}

static int pgsql_stmt_param_slot(ARGV *param_formats, const char *format)
{
    int     param_index;

    /* PostgreSQL $N can be referenced more than once; reuse the slot. */
    param_index = pgsql_stmt_param_index(param_formats, format);
    if (param_index < 0) {
	argv_add(param_formats, format, ARGV_END);
	param_index = param_formats->argc - 1;
    }
    return (param_index);
}

static void pgsql_stmt_append_param_ref(VSTRING *buf, int param_index,
					        int *pieces)
{
    if ((*pieces)++ > 0)
	vstring_strcat(buf, " || ");
    vstring_sprintf_append(buf, "$%d", param_index + 1);
}

static void pgsql_stmt_append_literal(VSTRING *buf, const char *text,
				              ssize_t len, int *pieces)
{
    if ((*pieces)++ > 0)
	vstring_strcat(buf, " || ");
    VSTRING_ADDCH(buf, '\'');
    vstring_strncat(buf, text, len);
    VSTRING_ADDCH(buf, '\'');
}

/* pgsql_stmt_append_quoted - parse quoted body, append to SQL query */

static void pgsql_stmt_append_quoted(VSTRING *sql, ARGV *param_formats,
				             const char *text, ssize_t len)
{
    int     param_index = -1;
    VSTRING *lit;
    const char *end = text + len;
    const char *cp;
    int     pieces = 0;			/* literal or $number instances */

    /* Fast path for the short form: '%u', '%s' etc. */
    if (len == 2 && text[0] == '%' && pgsql_stmt_param(text[1])) {
	char    format[] = {'%', text[1], 0};

	param_index = pgsql_stmt_param_slot(param_formats, format);
	vstring_sprintf_append(sql, "$%d", param_index + 1);
	return;
    }
    lit = vstring_alloc(10);

    /*
     * Separate literal text from placeholders and join them with ||, so
     * that a single quoted string 'prefix-%u' becomes 'prefix-' || $1.
     */
    for (cp = text; cp < end; cp++) {
	if (*cp == '\'' && cp + 1 < end && cp[1] == '\'') {
	    VSTRING_ADDCH(lit, *cp);
	    VSTRING_ADDCH(lit, *++cp);
	} else if (*cp == '%' && cp + 1 < end) {
	    if (cp[1] == '%') {
		VSTRING_ADDCH(lit, '%');
		cp += 1;
	    } else if (pgsql_stmt_param(cp[1])) {
		char    format[] = {'%', cp[1], 0};

		if (VSTRING_LEN(lit) > 0) {
		    VSTRING_TERMINATE(lit);
		    pgsql_stmt_append_literal(sql, vstring_str(lit),
					      VSTRING_LEN(lit), &pieces);
		    VSTRING_RESET(lit);
		    VSTRING_TERMINATE(lit);
		}

		param_index = pgsql_stmt_param_slot(param_formats, format);
		pgsql_stmt_append_param_ref(sql, param_index, &pieces);
		cp += 1;
	    } else {
		msg_fatal("unexpected '%.2s' in pgsql query template: %.*s",
			  cp, (int) len, text);
	    }
	} else {
	    VSTRING_ADDCH(lit, *cp);
	}
    }

    /* Output at least one piece. */
    if (VSTRING_LEN(lit) > 0 || pieces == 0) {
	VSTRING_TERMINATE(lit);
	pgsql_stmt_append_literal(sql, vstring_str(lit),
				  VSTRING_LEN(lit), &pieces);
    }
    vstring_free(lit);
}

/* pgsql_statement_alloc - parse the configured query into a PGSQL_STATEMENT */

static PGSQL_STATEMENT *pgsql_statement_alloc(const char *mapname,
					              const char *query)
{
    PGSQL_STATEMENT *stmt;
    VSTRING *sql;
    VSTRING *name;
    ARGV   *param_formats;
    const char *cp;
    int     param_index;
    int     i;

    stmt = (PGSQL_STATEMENT *) mymalloc(sizeof(*stmt));
    sql = vstring_alloc(100);
    name = vstring_alloc(32);
    param_formats = argv_alloc(1);

    /*
     * Parse the map query once at open time and normalize it into the form
     * PQprepare() expects. Plain %x placeholders become PostgreSQL $n
     * parameters directly; quoted fragments are handled separately so
     * placeholders inside SQL string literals can be rewritten without
     * changing the literal quoting rules.
     */
    for (cp = query; *cp; cp++) {

	/* Extract text between quotes. */
	if (*cp == '\'') {
	    const char *start;

	    for (start = ++cp; *cp; cp++) {
		if (*cp == '\'') {
		    if (cp[1] == '\'') {
			cp += 1;
		    } else {
			break;
		    }
		}
	    }

	    /*
	     * A broken quote makes the generated SQL ambiguous, so fail
	     * while loading the map instead of preparing or executing a
	     * malformed statement later.
	     */
	    if (*cp == 0)
		msg_fatal("%s: unterminated quote in query template: %s",
			  mapname, query);
	    pgsql_stmt_append_quoted(sql, param_formats, start, cp - start);
	}

	/*
	 * Extract %letter from unquoted text. Such usage is safe only with
	 * prepared-statement support.
	 */
	else if (*cp == '%') {
	    if (cp[1] == 0) {
		msg_fatal("%s: '%%' at end of query template: %s",
			  mapname, query);
	    } else if (cp[1] == '%') {
		VSTRING_ADDCH(sql, '%');
		cp += 1;
	    } else if (pgsql_stmt_param(cp[1])) {
		char    format[] = {'%', cp[1], 0};

		param_index = pgsql_stmt_param_slot(param_formats, format);
		vstring_sprintf_append(sql, "$%d", param_index + 1);
		cp += 1;
	    } else {
		/* Avoid poor error message from db_common_expand(). */
		msg_fatal("%s: unexpected '%.2s' in query template: %s",
			  mapname, cp, query);
	    }
	}

	/* Extract other unquoted text. */
	else {
	    VSTRING_ADDCH(sql, *cp);
	}
    }
    VSTRING_TERMINATE(sql);
    /* Hash the normalized SQL so equivalent templates share one stable name. */
    vstring_sprintf(name, "dict_pgsql_%08lx",
		    (unsigned long) hash_fnvz(vstring_str(sql)));

    stmt->sql = vstring_export(sql);
    stmt->name = vstring_export(name);
    stmt->param_formats = param_formats;
    /* One reusable buffer/value slot per distinct placeholder format. */
    if (param_formats->argc > 0) {
	stmt->param_bufs = (VSTRING **) mymalloc(param_formats->argc
						 * sizeof(*stmt->param_bufs));
	stmt->param_values = (const char **) mymalloc(param_formats->argc
						 * sizeof(*stmt->param_values));
	for (i = 0; i < param_formats->argc; i++) {
	    stmt->param_bufs[i] = vstring_alloc(10);
	    stmt->param_values[i] = 0;
	}
    } else {
	stmt->param_bufs = 0;
	stmt->param_values = 0;
    }
    if (msg_verbose) {
	msg_info("%s: stmt sql = '%s'", __func__, stmt->sql);
	msg_info("%s: stmt name = '%s'", __func__, stmt->name);
	for (i = 0; i < param_formats->argc; i++) {
	    msg_info("%s: stmt param formats[%d] = '%s'",
		     __func__, i, stmt->param_formats->argv[i]);
	}
    }
    return (stmt);
}

/* pgsql_statement_free - release the PGSQL_STATEMENT compiled at open time */

static void pgsql_statement_free(PGSQL_STATEMENT *stmt)
{
    int     i;

    if (stmt->sql)
	myfree(stmt->sql);
    if (stmt->name)
	myfree(stmt->name);
    if (stmt->param_bufs) {
	for (i = 0; i < stmt->param_formats->argc; i++)
	    vstring_free(stmt->param_bufs[i]);
	myfree((void *) stmt->param_bufs);
    }
    if (stmt->param_values)
	myfree((void *) stmt->param_values);
    if (stmt->param_formats)
	argv_free(stmt->param_formats);
    myfree((void *) stmt);
}

static int pgsql_stmt_bind_value(DICT_PGSQL *dict_pgsql, const char *format,
				         const char *name, VSTRING *buf)
{
    /* Re-expand this placeholder for the current lookup key into its slot. */
    VSTRING_RESET(buf);
    VSTRING_TERMINATE(buf);
    return (db_common_expand(dict_pgsql->ctx, format, name, 0, buf,
			     (db_quote_callback_t) 0));
}

static int pgsql_stmt_bind(DICT_PGSQL *dict_pgsql, const char *name)
{
    PGSQL_STATEMENT *stmt = dict_pgsql->stmt;
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
		 dict_pgsql->dict.type, dict_pgsql->dict.name);
	return (0);
    }
    /* Fill every prepared-statement argument in the slot order from alloc(). */
    for (i = 0; i < stmt->param_formats->argc; i++) {
	if (!pgsql_stmt_bind_value(dict_pgsql, stmt->param_formats->argv[i],
				   name, stmt->param_bufs[i]))
	    return (0);

	/*
	 * PQexecParams()/PQexecPrepared() read the NUL-terminated text
	 * pointer for each arg.
	 */
	stmt->param_values[i] = vstring_str(stmt->param_bufs[i]);
    }
    return (1);
}

/* plpgsql_prepare_stmt - run PQprepare on this host once per connection */

static int plpgsql_prepare_stmt(DICT_PGSQL *dict_pgsql, HOST *host)
{
    PGresult *res;
    ExecStatusType status;

    /*
     * Prepared statements live on one PostgreSQL connection, so each host
     * must prepare this compiled SQL once after connect/reconnect.
     */
    if (host->stmt_ready)
	return (1);

    res = PQprepare(host->db, dict_pgsql->stmt->name, dict_pgsql->stmt->sql,
		    dict_pgsql->stmt->param_formats->argc, (const Oid *) 0);
    if (res == 0) {
	msg_warn("pgsql prepare failed: fatal error from host %s: %s",
		 host->hostname, PQerrorMessage(host->db));
	return (0);
    }
    status = PQresultStatus(res);
    if (status != PGRES_COMMAND_OK) {
	msg_warn("pgsql prepare failed: host %s: %s",
		 host->hostname, PQresultErrorMessage(res));
	PQclear(res);
	return (0);
    }
    PQclear(res);
    host->stmt_ready = 1;
    return (1);
}

/*
 * plpgsql_query_prepared - process a parameterized or prepared PostgreSQL
 *			query.  Return PGSQL_RES* on success.  On failure,
 *			log failure and try other db instances; on failure
 *			of all db instances, return 0; close unnecessary
 *			active connections.
 */

static PGSQL_RES *plpgsql_query_prepared(DICT_PGSQL *dict_pgsql)
{
    PLPGSQL *PLDB = dict_pgsql->pldb;
    HOST   *host;
    PGSQL_RES *res = 0;
    ExecStatusType status;
    int     param_count = dict_pgsql->stmt->param_formats->argc;

    while ((host = dict_pgsql_get_active(dict_pgsql, PLDB)) != NULL) {

	/*
	 * In prepared mode, PQprepare() runs once per (host, statement)
	 * pair after each connect/reconnect; in parameterized mode the SQL
	 * is shipped along with the values on every call.
	 */
	if (dict_pgsql->statement_mode == PGSQL_STATEMENT_PREPARED
	    && !plpgsql_prepare_stmt(dict_pgsql, host)) {
	    plpgsql_down_host(host, dict_pgsql->retry_interval);
	    continue;
	}

	if (dict_pgsql->statement_mode == PGSQL_STATEMENT_PARAMETERIZED) {
	    res = PQexecParams(host->db, dict_pgsql->stmt->sql, param_count,
			       (const Oid *) 0, dict_pgsql->stmt->param_values,
			       (const int *) 0, (const int *) 0, 0);
	} else {
	    res = PQexecPrepared(host->db, dict_pgsql->stmt->name, param_count,
				 dict_pgsql->stmt->param_values,
				 (const int *) 0, (const int *) 0, 0);
	}

	if (res != 0) {

	    /*
	     * Same status-handling logic as plpgsql_query_legacy(), see
	     * the long comment there.
	     */
	    switch ((status = PQresultStatus(res))) {
	    case PGRES_TUPLES_OK:
	    case PGRES_COMMAND_OK:
		if (msg_verbose)
		    msg_info("dict_pgsql: successful query from host %s",
			     host->hostname);
		event_request_timer(dict_pgsql_event, (void *) host,
				    dict_pgsql->idle_interval);
		return (res);
	    case PGRES_FATAL_ERROR:
		msg_warn("pgsql query failed: fatal error from host %s: %s",
			 host->hostname, PQresultErrorMessage(res));
		break;
	    case PGRES_BAD_RESPONSE:
		msg_warn("pgsql query failed: protocol error, host %s",
			 host->hostname);
		break;
	    default:
		msg_warn("pgsql query failed: unknown code 0x%lx from host %s",
			 (unsigned long) status, host->hostname);
		break;
	    }
	} else {
	    msg_warn("pgsql query failed: fatal error from host %s: %s",
		     host->hostname, PQerrorMessage(host->db));
	}

	/* Error: clean up and try the next host. */
	if (res != 0)
	    PQclear(res);
	plpgsql_down_host(host, dict_pgsql->retry_interval);
    }

    return (0);
}

/* dict_pgsql_lookup_prepared - find database entry, prepared/parameterized path */

static const char *dict_pgsql_lookup_prepared(DICT *dict, const char *name)
{
    const char *myname = "dict_pgsql_lookup_prepared";
    DICT_PGSQL *dict_pgsql = (DICT_PGSQL *) dict;
    PGSQL_RES *query_res;
    static VSTRING *result;
    int     i;
    int     j;
    int     numrows;
    int     numcols;
    int     expansion;
    const char *r;

    INIT_VSTR(result, 10);

    /*
     * Bind every placeholder once per lookup, before connecting to a host:
     * the bound values are independent of which connection ends up running
     * the query.
     */
    if (!pgsql_stmt_bind(dict_pgsql, name))
	return (0);

    /* do the query - set dict->error & cleanup if there's an error */
    if ((query_res = plpgsql_query_prepared(dict_pgsql)) == 0) {
	dict->error = DICT_ERR_RETRY;
	return 0;
    }
    numrows = PQntuples(query_res);
    if (msg_verbose)
	msg_info("%s: retrieved %d rows", myname, numrows);
    if (numrows == 0) {
	PQclear(query_res);
	return 0;
    }
    numcols = PQnfields(query_res);

    for (expansion = i = 0; i < numrows && dict->error == 0; i++) {
	for (j = 0; j < numcols; j++) {
	    r = PQgetvalue(query_res, i, j);
	    if (db_common_expand(dict_pgsql->ctx, dict_pgsql->result_format,
				 r, name, result, 0)
		&& dict_pgsql->expansion_limit > 0
		&& ++expansion > dict_pgsql->expansion_limit) {
		msg_warn("%s: %s: Expansion limit exceeded for key: '%s'",
			 myname, dict_pgsql->parser->name, name);
		dict->error = DICT_ERR_RETRY;
		break;
	    }
	}
    }
    PQclear(query_res);
    r = vstring_str(result);
    return ((dict->error == 0 && *r) ? r : 0);
}

 /*
  * ====================================================================
  * End prepared/parameterized code path.
  * ====================================================================
  */

#endif
