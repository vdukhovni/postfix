/*++
/* NAME
/*	dict_pgsql 3
/* SUMMARY
/*	dictionary manager interface to Postgresql files
/* SYNOPSIS
/*	#include <dict_pgsql.h>
/*
/*	DICT	*dict_pgsql_open(name, open_flags, dict_flags)
/*	const char *name;
/*	int     open_flags;
/*	int     dict_flags;
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
/*
/*	Arguments:
/* .IP name
/*	The path of the PostgreSQL configuration file.  The file
/*	encodes number of pieces of information: username, password,
/*	databasename, table, select_field, where_field, and hosts.
/*	For example, if you want the map to reference databases of
/*	the name "your_db" and execute a query like this:  select
/*	forw_addr from aliases where alias like '<some username>'
/*	against any database called "postfix_info" located on hosts
/*	host1.some.domain and host2.some.domain, logging in as user
/*	"postfix" and password "passwd" then the configuration file
/*	should read:
/*
/*	user = postfix
/*	password = passwd
/*	DBname = postfix_info
/*	table = aliases
/*	select_field = forw_addr
/*	where_field = alias
/*	hosts = host1.some.domain host2.some.domain
/*
/* .IP other_name
/*	reference for outside use.
/* .IP open_flags
/*	Must be O_RDONLY.
/* .IP dict_flags
/*	See dict_open(3).
/* SEE ALSO
/*	dict(3) generic dictionary manager
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
#include "dict_pgsql.h"
#include "argv.h"
#include "vstring.h"
#include "split_at.h"
#include "find_inet.h"

#define STATACTIVE	0
#define STATFAIL	1
#define STATUNTRIED	2
#define RETRY_CONN_INTV	60		/* 1 minute */

typedef struct {
    PGconn *db;
    char   *hostname;
    int     stat;			/* STATUNTRIED | STATFAIL | STATCUR */
    time_t  ts;				/* used for attempting reconnection */
} HOST;

typedef struct {
    int     len_hosts;			/* number of hosts */
    HOST   *db_hosts;			/* hosts on which databases reside */
} PLPGSQL;

typedef struct {
    char   *username;
    char   *password;
    char   *dbname;
    char   *table;
    char   *query;			/* if set, overrides fields, etc */
    char   *select_function;
    char   *select_field;
    char   *where_field;
    char   *additional_conditions;
    char  **hostnames;
    int     len_hosts;
} PGSQL_NAME;

typedef struct {
    DICT    dict;
    PLPGSQL *pldb;
    PGSQL_NAME *name;
} DICT_PGSQL;


/* Just makes things a little easier for me.. */
#define PGSQL_RES PGresult

/* internal function declarations */
static PLPGSQL *plpgsql_init(char *hostnames[], int);
static PGSQL_RES *plpgsql_query(PLPGSQL *, const char *, char *, char *, char *);
static void plpgsql_dealloc(PLPGSQL *);
static void plpgsql_close_host(HOST *);
static void plpgsql_down_host(HOST *);
static void plpgsql_connect_single(HOST *, char *, char *, char *);
static const char *dict_pgsql_lookup(DICT *, const char *);
DICT   *dict_pgsql_open(const char *, int, int);
static void dict_pgsql_close(DICT *);
static PGSQL_NAME *pgsqlname_parse(const char *);
static HOST host_init(char *);



/**********************************************************************
 * public interface dict_pgsql_lookup
 * find database entry return 0 if no alias found, set dict_errno
 * on errors to DICT_ERROR_RETRY and set dict_errno to 0 on success
 *********************************************************************/
static void pgsql_escape_string(char *new, const char *old, unsigned int len)
{
    unsigned int x,
            y;

    /*
     * XXX We really should be using an escaper that is provided by the PGSQL
     * library. The code below seems to be over-kill (see RUS-CERT Advisory
     * 2001-08:01), but it's better to be safe than to be sorry -- Wietse
     */
    for (x = 0, y = 0; x < len; x++, y++) {
	switch (old[x]) {
	case '\n':
	    new[y++] = '\\';
	    new[y] = 'n';
	    break;
	case '\r':
	    new[y++] = '\\';
	    new[y] = 'r';
	    break;
	case '\'':
	    new[y++] = '\\';
	    new[y] = '\'';
	    break;
	case '"':
	    new[y++] = '\\';
	    new[y] = '"';
	    break;
	case 0:
	    new[y++] = '\\';
	    new[y] = '0';
	    break;
	default:
	    new[y] = old[x];
	    break;
	}
    }
    new[y] = 0;
}

/*
 * expand a filter (lookup or result)
 */
static void dict_pgsql_expand_filter(char *filter, char *value, VSTRING *out)
{
    char   *myname = "dict_pgsql_expand_filter";
    char   *sub,
           *end;

    /*
     * Yes, replace all instances of %s with the address to look up. Replace
     * %u with the user portion, and %d with the domain portion.
     */
    sub = filter;
    end = sub + strlen(filter);
    while (sub < end) {

	/*
	 * Make sure it's %[sud] and not something else.  For backward
	 * compatibilty, treat anything other than %u or %d as %s, with a
	 * warning.
	 */
	if (*(sub) == '%') {
	    char   *u = value;
	    char   *p = strrchr(u, '@');

	    switch (*(sub + 1)) {
	    case 'd':
		if (p)
		    vstring_strcat(out, p + 1);
		break;
	    case 'u':
		if (p)
		    vstring_strncat(out, u, p - u);
		else
		    vstring_strcat(out, u);
		break;
	    default:
		msg_warn
		    ("%s: Invalid filter substitution format '%%%c'!",
		     myname, *(sub + 1));
		break;
	    case 's':
		vstring_strcat(out, u);
		break;
	    }
	    sub++;
	} else
	    vstring_strncat(out, sub, 1);
	sub++;
    }
}

static const char *dict_pgsql_lookup(DICT *dict, const char *name)
{
    PGSQL_RES *query_res;
    DICT_PGSQL *dict_pgsql;
    PLPGSQL *pldb;
    static VSTRING *result;
    static VSTRING *query = 0;
    int     i,
            j,
            numrows;
    char   *name_escaped = 0;
    int     isFunctionCall;
    int     numcols;

    dict_pgsql = (DICT_PGSQL *) dict;
    pldb = dict_pgsql->pldb;
    /* initialization  for query */
    query = vstring_alloc(24);
    vstring_strcpy(query, "");
    if ((name_escaped = (char *) mymalloc((sizeof(char) * (strlen(name) * 2) +1))) == NULL) {
	msg_fatal("dict_pgsql_lookup: out of memory.");
    }
    /* prepare the query */
    pgsql_escape_string(name_escaped, name, (unsigned int) strlen(name));

    /* Build SQL - either a select from table or select a function */

    isFunctionCall = (dict_pgsql->name->select_function != NULL);
    if (isFunctionCall) {
	vstring_sprintf(query, "select %s('%s')",
			dict_pgsql->name->select_function,
			name_escaped);
    } else if (dict_pgsql->name->query) {
	dict_pgsql_expand_filter(dict_pgsql->name->query, name_escaped, query);
    } else {
	vstring_sprintf(query, "select %s from %s where %s = '%s' %s", dict_pgsql->name->select_field,
			dict_pgsql->name->table,
			dict_pgsql->name->where_field,
			name_escaped,
			dict_pgsql->name->additional_conditions);
    }

    if (msg_verbose)
	msg_info("dict_pgsql_lookup using sql query: %s", vstring_str(query));

    /* free mem associated with preparing the query */
    myfree(name_escaped);

    /* do the query - set dict_errno & cleanup if there's an error */
    if ((query_res = plpgsql_query(pldb,
				   vstring_str(query),
				   dict_pgsql->name->dbname,
				   dict_pgsql->name->username,
				   dict_pgsql->name->password)) == 0) {
	dict_errno = DICT_ERR_RETRY;
	vstring_free(query);
	return 0;
    }
    dict_errno = 0;
    /* free the vstring query */
    vstring_free(query);
    numrows = PQntuples(query_res);
    if (msg_verbose)
	msg_info("dict_pgsql_lookup: retrieved %d rows", numrows);
    if (numrows == 0) {
	PQclear(query_res);
	return 0;
    }
    numcols = PQnfields(query_res);

    if (numcols == 1 && numrows == 1 && isFunctionCall) {

	/*
	 * We do the above check because PostgreSQL 7.3 will allow functions
	 * to return result sets
	 */
	if (PQgetisnull(query_res, 0, 0) == 1) {

	    /*
	     * Functions returning a single row & column that is null are
	     * deemed to have not found the key.
	     */
	    PQclear(query_res);
	    return 0;
	}
    }
    if (result == 0)
	result = vstring_alloc(10);

    vstring_strcpy(result, "");
    for (i = 0; i < numrows; i++) {
	if (i > 0)
	    vstring_strcat(result, ",");
	for (j = 0; j < numcols; j++) {
	    if (j > 0)
		vstring_strcat(result, ",");
	    vstring_strcat(result, PQgetvalue(query_res, i, j));
	    if (msg_verbose > 1)
		msg_info("dict_pgsql_lookup: retrieved field: %d: %s", j, PQgetvalue(query_res, i, j));
	}
    }
    PQclear(query_res);
    return vstring_str(result);
}

/*
 * plpgsql_query - process a PostgreSQL query.  Return PGSQL_RES* on success.
 *		     On failure, log failure and try other db instances.
 *		     on failure of all db instances, return 0;
 *		     close unnecessary active connections
 */

static PGSQL_RES *plpgsql_query(PLPGSQL *PLDB,
				        const char *query,
				        char *dbname,
				        char *username,
				        char *password)
{
    int     i;
    HOST   *host;
    PGSQL_RES *res = 0;

    for (i = 0; i < PLDB->len_hosts; i++) {
	/* can't deal with typing or reading PLDB->db_hosts[i] over & over */
	host = &(PLDB->db_hosts[i]);
	if (msg_verbose > 1)
	    msg_info("dict_pgsql: trying host %s stat %d, last res %p", host->hostname, host->stat, res);

	/* answer already found */
	if (res != 0 && host->stat == STATACTIVE) {
	    if (msg_verbose)
		msg_info("dict_pgsql: closing unnessary connection to %s",
			 host->hostname);
	    plpgsql_close_host(host);
	}
	/* try to connect for the first time if we don't have a result yet */
	if (res == 0 && host->stat == STATUNTRIED) {
	    if (msg_verbose)
		msg_info("dict_pgsql: attempting to connect to host %s",
			 host->hostname);
	    plpgsql_connect_single(host, dbname, username, password);
	}

	/*
	 * try to reconnect if we don't have an answer and the host had a
	 * prob in the past and it's time for it to reconnect
	 */
	if (res == 0 && host->stat == STATFAIL && host->ts < time((time_t *) 0)) {
	    if (msg_verbose)
		msg_info("dict_pgsql: attempting to reconnect to host %s",
			 host->hostname);
	    plpgsql_connect_single(host, dbname, username, password);
	}

	/*
	 * if we don't have a result and the current host is marked active,
	 * try the query.  If the query fails, mark the host STATFAIL
	 */
	if (res == 0 && host->stat == STATACTIVE) {
	    if ((res = PQexec(host->db, query))) {
		if (msg_verbose)
		    msg_info("dict_pgsql: successful query from host %s", host->hostname);
	    } else {
		msg_warn("%s", PQerrorMessage(host->db));
		plpgsql_down_host(host);
	    }
	}
    }
    return res;
}

/*
 * plpgsql_connect_single -
 * used to reconnect to a single database when one is down or none is
 * connected yet. Log all errors and set the stat field of host accordingly
 */
static void plpgsql_connect_single(HOST *host, char *dbname, char *username, char *password)
{
    char   *destination = host->hostname;
    char   *unix_socket = 0;
    char   *hostname = 0;
    char   *service;
    char   *port = 0;

    /*
     * Ad-hoc parsing code. Expect "unix:pathname" or "inet:host:port", where
     * both "inet:" and ":port" are optional.
     */
    if (strncmp(destination, "unix:", 5) == 0) {
	unix_socket = destination + 5;
    } else {
	if (strncmp(destination, "inet:", 5) == 0)
	    destination += 5;
	hostname = mystrdup(destination);
	if ((service = split_at(hostname, ':')) != 0)
	    port = service;
    }

    if ((host->db = PQsetdbLogin(hostname, port, NULL, NULL, dbname, username, password))) {
	if (PQstatus(host->db) == CONNECTION_OK) {
	    if (msg_verbose)
		msg_info("dict_pgsql: successful connection to host %s",
			 host->hostname);
	    host->stat = STATACTIVE;
	} else
	    msg_warn("%s", PQerrorMessage(host->db));
    } else {
	msg_warn("Unable to connect to database");
	plpgsql_down_host(host);
    }
    if (hostname)
	myfree(hostname);
}

/* plpgsql_close_host - close an established PostgreSQL connection */

static void plpgsql_close_host(HOST *host)
{
    PQfinish(host->db);
    host->db = 0;
    host->stat = STATUNTRIED;
}

/*
 * plpgsql_down_host - close a failed connection AND set a "stay away from
 * this host" timer.
 */
static void plpgsql_down_host(HOST *host)
{
    PQfinish(host->db);
    host->db = 0;
    host->ts = time((time_t *) 0) + RETRY_CONN_INTV;
    host->stat = STATFAIL;
}

/**********************************************************************
 * public interface dict_pgsql_open
 *    create association with database with appropriate values
 *    parse the map's config file
 *    allocate memory
 **********************************************************************/
DICT   *dict_pgsql_open(const char *name, int open_flags, int dict_flags)
{
    DICT_PGSQL *dict_pgsql;

    /*
     * Sanity checks.
     */
    if (open_flags != O_RDONLY)
	msg_fatal("%s:%s map requires O_RDONLY access mode",
		  DICT_TYPE_PGSQL, name);

    dict_pgsql = (DICT_PGSQL *) dict_alloc(DICT_TYPE_PGSQL, name,
					   sizeof(DICT_PGSQL));
    dict_pgsql->dict.lookup = dict_pgsql_lookup;
    dict_pgsql->dict.close = dict_pgsql_close;
    dict_pgsql->name = pgsqlname_parse(name);
    dict_pgsql->pldb = plpgsql_init(dict_pgsql->name->hostnames,
				    dict_pgsql->name->len_hosts);
    dict_pgsql->dict.flags = dict_flags | DICT_FLAG_FIXED;
    if (dict_pgsql->pldb == NULL)
	msg_fatal("couldn't intialize pldb!\n");
    return &dict_pgsql->dict;
}

/* pgsqlname_parse - parse pgsql configuration file */
static PGSQL_NAME *pgsqlname_parse(const char *pgsqlcf_path)
{
    int     i;
    char   *nameval;
    char   *hosts;
    PGSQL_NAME *name = (PGSQL_NAME *) mymalloc(sizeof(PGSQL_NAME));
    ARGV   *hosts_argv;
    VSTRING *opt_dict_name;

    /*
     * setup a dict containing info in the pgsql cf file. the dict has a
     * name, and a path.  The name must be distinct from the path, or the
     * dict interface gets confused.  The name must be distinct for two
     * different paths, or the configuration info will cache across different
     * pgsql maps, which can be confusing.
     */
    opt_dict_name = vstring_alloc(64);
    vstring_sprintf(opt_dict_name, "pgsql opt dict %s", pgsqlcf_path);
    dict_load_file(vstring_str(opt_dict_name), pgsqlcf_path);
    /* pgsql username lookup */
    if ((nameval = (char *) dict_lookup(vstring_str(opt_dict_name), "user")) == NULL)
	name->username = mystrdup("");
    else
	name->username = mystrdup(nameval);
    if (msg_verbose)
	msg_info("pgsqlname_parse(): set username to '%s'", name->username);
    /* password lookup */
    if ((nameval = (char *) dict_lookup(vstring_str(opt_dict_name), "password")) == NULL)
	name->password = mystrdup("");
    else
	name->password = mystrdup(nameval);
    if (msg_verbose)
	msg_info("pgsqlname_parse(): set password to '%s'", name->password);

    /* database name lookup */
    if ((nameval = (char *) dict_lookup(vstring_str(opt_dict_name), "dbname")) == NULL)
	msg_fatal("%s: pgsql options file does not include database name", pgsqlcf_path);
    else
	name->dbname = mystrdup(nameval);
    if (msg_verbose)
	msg_info("pgsqlname_parse(): set database name to '%s'", name->dbname);

    /* table lookup */
    if ((nameval = (char *) dict_lookup(vstring_str(opt_dict_name), "table")) == NULL)
	msg_fatal("%s: pgsql options file does not include table name", pgsqlcf_path);
    else
	name->table = mystrdup(nameval);
    if (msg_verbose)
	msg_info("pgsqlname_parse(): set table name to '%s'", name->table);

    name->select_function = NULL;
    name->query = NULL;

    /*
     * See what kind of lookup we have - a traditional 'select' or a function
     * call
     */
    if ((nameval = (char *) dict_lookup(vstring_str(opt_dict_name), "select_function")) != NULL) {

	/* We have a 'select %s(%s)' function call. */
	name->select_function = mystrdup(nameval);
	if (msg_verbose)
	    msg_info("pgsqlname_parse(): set function name to '%s'", name->table);
	/* query string */
    } else if ((nameval = (char *) dict_lookup(vstring_str(opt_dict_name), "query")) != NULL) {
	name->query = mystrdup(nameval);
	if (msg_verbose)
	    msg_info("pgsqlname_parse(): set query to '%s'", name->query);
    } else {

	/*
	 * We have an old style 'select %s from %s...' call, so get the
	 * fields
	 */

	/* table lookup */
	if ((nameval = (char *) dict_lookup(vstring_str(opt_dict_name), "table")) == NULL)
	    msg_fatal("%s: pgsql options file does not include table name", pgsqlcf_path);
	else
	    name->table = mystrdup(nameval);
	if (msg_verbose)
	    msg_info("pgsqlname_parse(): set table name to '%s'", name->table);

	/* select field lookup */
	if ((nameval = (char *) dict_lookup(vstring_str(opt_dict_name), "select_field")) == NULL)
	    msg_fatal("%s: pgsql options file does not include select field", pgsqlcf_path);
	else
	    name->select_field = mystrdup(nameval);
	if (msg_verbose)
	    msg_info("pgsqlname_parse(): set select_field to '%s'", name->select_field);

	/* where field lookup */
	if ((nameval = (char *) dict_lookup(vstring_str(opt_dict_name), "where_field")) == NULL)
	    msg_fatal("%s: pgsql options file does not include where field", pgsqlcf_path);
	else
	    name->where_field = mystrdup(nameval);
	if (msg_verbose)
	    msg_info("pgsqlname_parse(): set where_field to '%s'", name->where_field);

	/* additional conditions */
	if ((nameval = (char *) dict_lookup(vstring_str(opt_dict_name), "additional_conditions")) == NULL)
	    name->additional_conditions = mystrdup("");
	else
	    name->additional_conditions = mystrdup(nameval);
	if (msg_verbose)
	    msg_info("pgsqlname_parse(): set additional_conditions to '%s'", name->additional_conditions);
    }

    /* pgsql server hosts */
    if ((nameval = (char *) dict_lookup(vstring_str(opt_dict_name), "hosts")) == NULL)
	hosts = mystrdup("");
    else
	hosts = mystrdup(nameval);
    /* coo argv interface */
    hosts_argv = argv_split(hosts, " ,\t\r\n");

    if (hosts_argv->argc == 0) {		/* no hosts specified,
						 * default to 'localhost' */
	if (msg_verbose)
	    msg_info("pgsqlname_parse(): no hostnames specified, defaulting to 'localhost'");
	argv_add(hosts_argv, "localhost", ARGV_END);
	argv_terminate(hosts_argv);
    }
    name->len_hosts = hosts_argv->argc;
    name->hostnames = (char **) mymalloc((sizeof(char *)) * name->len_hosts);
    i = 0;
    for (i = 0; hosts_argv->argv[i] != NULL; i++) {
	name->hostnames[i] = mystrdup(hosts_argv->argv[i]);
	if (msg_verbose)
	    msg_info("pgsqlname_parse(): adding host '%s' to list of pgsql server hosts",
		     name->hostnames[i]);
    }
    myfree(hosts);
    vstring_free(opt_dict_name);
    argv_free(hosts_argv);
    return name;
}


/*
 * plpgsql_init - initalize a PGSQL database.
 *		    Return NULL on failure, or a PLPGSQL * on success.
 */
static PLPGSQL *plpgsql_init(char *hostnames[], int len_hosts)
{
    PLPGSQL *PLDB;
    int     i;

    if ((PLDB = (PLPGSQL *) mymalloc(sizeof(PLPGSQL))) == NULL) {
	msg_fatal("mymalloc of pldb failed");
    }
    PLDB->len_hosts = len_hosts;
    if ((PLDB->db_hosts = (HOST *) mymalloc(sizeof(HOST) * len_hosts)) == NULL)
	return NULL;
    for (i = 0; i < len_hosts; i++) {
	PLDB->db_hosts[i] = host_init(hostnames[i]);
    }
    return PLDB;
}


/* host_init - initialize HOST structure */
static HOST host_init(char *hostname)
{
    HOST    host;

    host.stat = STATUNTRIED;
    host.hostname = mystrdup(hostname);
    host.db = 0;
    host.ts = 0;
    return host;
}

/**********************************************************************
 * public interface dict_pgsql_close
 * unregister, disassociate from database, freeing appropriate memory
 **********************************************************************/
static void dict_pgsql_close(DICT *dict)
{
    int     i;
    DICT_PGSQL *dict_pgsql = (DICT_PGSQL *) dict;

    plpgsql_dealloc(dict_pgsql->pldb);
    myfree(dict_pgsql->name->username);
    myfree(dict_pgsql->name->password);
    myfree(dict_pgsql->name->dbname);
    myfree(dict_pgsql->name->table);
    myfree(dict_pgsql->name->select_field);
    myfree(dict_pgsql->name->where_field);
    myfree(dict_pgsql->name->additional_conditions);
    for (i = 0; i < dict_pgsql->name->len_hosts; i++) {
	myfree(dict_pgsql->name->hostnames[i]);
    }
    myfree((char *) dict_pgsql->name->hostnames);
    myfree((char *) dict_pgsql->name);
    dict_free(dict);
}

/* plpgsql_dealloc - free memory associated with PLPGSQL close databases */
static void plpgsql_dealloc(PLPGSQL *PLDB)
{
    int     i;

    for (i = 0; i < PLDB->len_hosts; i++) {
	if (PLDB->db_hosts[i].db)
	    PQfinish(PLDB->db_hosts[i].db);
	myfree(PLDB->db_hosts[i].hostname);
    }
    myfree((char *) PLDB->db_hosts);
    myfree((char *) (PLDB));
}

#endif
