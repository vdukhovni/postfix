/*++
/* NAME
/*	dict_mysql 3
/* SUMMARY
/*	dictionary manager interface to db files
/* SYNOPSIS
/*	#include <dict.h>
/*	#include <dict_mysql.h>
/*
/*	DICT	*dict_mysql_open(name, dummy, unused_dict_flags)
/*	const char	*name;
/*	int     dummy;
/*	int     unused_dict_flags;
/* DESCRIPTION
/*	dict_mysql_open() opens the mysql databases with name dbname on
/*	each host in hostlist and registers under the given name with the
/*	dictionary manager. The result is a pointer to the installed dictionary,
/*	or a null pointer in case of problems.
/*
/*	Arguments:
/* .IP name
/*	The path of the MySQL configuration file.  The file encodes a number of
/*	pieces of information: username, password, databasename, table,
/*	select_field, where_field, and hosts.  For example, if you want the map to
/*	reference databases of the name "your_db" and execute a query like this:
/*	select forw_addr from aliases where alias like '<some username>' against
/*	any database called "vmailer_info" located on hosts host1.some.domain and
/*	host2.some.domain, logging in as user "vmailer" and password "passwd" then
/*	the configuration file should read:
/*
/*	user = vmailer
/*	password = passwd
/*	DBname = vmailer_info
/*	table = aliases
/*	select_field = forw_addr
/*	where_field = alias
/*	hosts = host1.some.domain host2.some.domain
/*
/* .IP other_name
/*	reference for outside use.
/* .IP unusued_flags
/*	unused flags
/* SEE ALSO
/*	dict(3) generic dictionary manager
/* AUTHOR(S)
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

#ifdef HAS_MYSQL
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <time.h>

/* Utility library. */
#include "dict.h"
#include "msg.h"
#include "mymalloc.h"
#include "dict_mysql.h"
#include "argv.h"
#include "vstring.h"

extern int dict_errno;

typedef struct {
    char   *username;
    char   *password;
    char   *dbname;
    char   *table;
    char   *select_field;
    char   *where_field;
    char   *additional_conditions;
    char  **hostnames;
    int     len_hosts;
} MYSQL_NAME;

typedef struct {
    DICT    dict;
    PLMYSQL *pldb;
    MYSQL_NAME *name;
} DICT_MYSQL;

/* mysqlname_parse - parse mysql configuration file */

static MYSQL_NAME *mysqlname_parse(const char *mysqlcf_path)
{
    int     i;
    char   *nameval;
    char   *hosts;
    MYSQL_NAME *name = (MYSQL_NAME *) mymalloc(sizeof(MYSQL_NAME));
    ARGV   *hosts_argv;

    dict_load_file("mysql_options", mysqlcf_path);
    /* mysql username lookup */
    if ((nameval = (char *) dict_lookup("mysql_options", "user")) == NULL)
	name->username = mystrdup("");
    else
	name->username = mystrdup(nameval);
    if (msg_verbose)
	msg_info("dict_mysql_parse: set username to '%s'", name->username);
    /* password lookup */
    if ((nameval = (char *) dict_lookup("mysql_options", "password")) == NULL)
	name->password = mystrdup("");
    else
	name->password = mystrdup(nameval);
    if (msg_verbose)
	msg_info("dict_mysql_parse: set password to '%s'", name->password);

    /* database name lookup */
    if ((nameval = (char *) dict_lookup("mysql_options", "dbname")) == NULL)
	msg_fatal("%s: mysql options file does not include database name", mysqlcf_path);
    else
	name->dbname = mystrdup(nameval);
    if (msg_verbose)
	msg_info("mysql_name_parse: set database name to '%s'", name->dbname);

    /* table lookup */
    if ((nameval = (char *) dict_lookup("mysql_options", "table")) == NULL)
	msg_fatal("%s: mysql options file does not include table name", mysqlcf_path);
    else
	name->table = mystrdup(nameval);
    if (msg_verbose)
	msg_info("mysql_name_parse: set table name to '%s'", name->table);

    /* select field lookup */
    if ((nameval = (char *) dict_lookup("mysql_options", "select_field")) == NULL)
	msg_fatal("%s: mysql options file does not include select field", mysqlcf_path);
    else
	name->select_field = mystrdup(nameval);
    if (msg_verbose)
	msg_info("mysql_name_parse: set select_field to '%s'", name->select_field);

    /* where field lookup */
    if ((nameval = (char *) dict_lookup("mysql_options", "where_field")) == NULL)
	msg_fatal("%s: mysql options file does not include where field", mysqlcf_path);
    else
	name->where_field = mystrdup(nameval);
    if (msg_verbose)
	msg_info("mysql_name_parse: set where_field to '%s'", name->where_field);

    /* additional conditions */
    if ((nameval = (char *) dict_lookup("mysql_options", "additional_conditions")) == NULL)
	name->additional_conditions = mystrdup("");
    else
	name->additional_conditions = mystrdup(nameval);
    if (msg_verbose)
	msg_info("mysql_name_parse: set additional_conditions to '%s'", name->additional_conditions);

    /* mysql server hosts */
    if ((nameval = (char *) dict_lookup("mysql_options", "hosts")) == NULL)
	hosts = mystrdup("");
    else
	hosts = mystrdup(nameval);
    /* coo argv interface */
    hosts_argv = argv_split(hosts, " ");
    argv_terminate(hosts_argv);

    if (hosts_argv->argc == 0) {		/* no hosts specified,
						 * default to 'localhost' */
	msg_info("mysql_name_parse: no hostnames specified, defaulting to 'localhost'");
	name->len_hosts = 1;
	name->hostnames = (char **) mymalloc(sizeof(char *));
	name->hostnames[0] = mystrdup("localhost");
    } else {
	name->len_hosts = hosts_argv->argc;
	name->hostnames = (char **) mymalloc((sizeof(char *)) * name->len_hosts);
	i = 0;
	for (i = 0; hosts_argv->argv[i] != NULL; i++) {
	    name->hostnames[i] = mystrdup(hosts_argv->argv[i]);
	    if (msg_verbose)
		msg_info("adding host '%s' to list of mysql server hosts", name->hostnames[i]);
	}
    }
    myfree(hosts);
    argv_free(hosts_argv);
    return name;
}

/* dict_mysql_lookup - find database entry return 0 if no alias found */
static const char *dict_mysql_lookup(DICT *dict, const char *name)
{
    MYSQL_RES *query_res;
    MYSQL_ROW row;
    int     i,
            numrows;
    static VSTRING *result;
    static VSTRING *query = 0;
    char   *name_escaped = 0;
    DICT_MYSQL *dict_mysql;
    PLMYSQL *pldb;

    dict_mysql = (DICT_MYSQL *) dict;
    pldb = dict_mysql->pldb;
    /* initialization  for query */
    query = vstring_alloc(24);
    vstring_strcpy(query, "");
    if ((name_escaped = (char *) mymalloc((sizeof(char) * (strlen(name) * 2) +1))) == NULL) {
	msg_fatal("dict_mysql_lookup: out of memory.");
    }
    /* prepare the query */
    mysql_escape_string(name_escaped, name, (unsigned int) strlen(name));
    vstring_sprintf(query, "select %s from %s where %s = '%s' %s", dict_mysql->name->select_field,
       dict_mysql->name->table, dict_mysql->name->where_field, name_escaped,
		    dict_mysql->name->additional_conditions);
    if (msg_verbose)
	msg_info("dict_mysql_lookup using sql query: %s", vstring_str(query));
    /* free mem associated with preparing the query */
    myfree(name_escaped);
    /* do the query */
    if ((query_res = plmysql_query(pldb, vstring_str(query))) == NULL) {
	dict_errno = DICT_ERR_RETRY;
	vstring_free(query);
	return 0;
    }
    dict_errno = 0;
    /* free the vstring query */
    vstring_free(query);
    numrows = mysql_num_rows(query_res);
    if (msg_verbose)
	msg_info("dict_mysql_lookup: retrieved %d rows", numrows);
    if (numrows == 0) {
	mysql_free_result(query_res);
	return 0;
    }
    if (result == 0)
	result = vstring_alloc(10);
    vstring_strcpy(result, "");
    for (i = 0; i < numrows; i++) {
	row = mysql_fetch_row(query_res);
	if (msg_verbose > 1)
	    msg_info("dict_mysql_lookup: retrieved row: %d: %s", i, row[0]);
	if (i > 0)
	    vstring_strcat(result, ",");
	vstring_strcat(result, row[0]);
    }
    mysql_free_result(query_res);
    return vstring_str(result);
}

/* dict_mysql_close - unregister, disassociate from database */
static void dict_mysql_close(DICT *dict)
{
    int     i;
    DICT_MYSQL *dict_mysql = (DICT_MYSQL *) dict;

    plmysql_dealloc(dict_mysql->pldb);
    myfree(dict_mysql->name->username);
    myfree(dict_mysql->name->password);
    myfree(dict_mysql->name->dbname);
    myfree(dict_mysql->name->table);
    myfree(dict_mysql->name->select_field);
    myfree(dict_mysql->name->where_field);
    myfree(dict_mysql->name->additional_conditions);
    for (i = 0; i < dict_mysql->name->len_hosts; i++) {
	myfree(dict_mysql->name->hostnames[i]);
    }
    myfree((char *) dict_mysql->name);
}

/* dict_mysql_update - add or update table entry */
static void dict_mysql_update(DICT *dict, const char *unused_name, const char *unused_value)
{
    DICT_MYSQL *dict_mysql = (DICT_MYSQL *) dict;

    msg_fatal("dict_mysql_update: attempt to update mysql database");
}

/* dict_mysql_open - create association with database */
DICT   *dict_mysql_open(const char *name, int unused_flags, int unused_dict_flags)
{
    DICT_MYSQL *dict_mysql;
    int     connections;

    dict_mysql = (DICT_MYSQL *) mymalloc(sizeof(DICT_MYSQL));
    dict_mysql->dict.lookup = dict_mysql_lookup;
    dict_mysql->dict.update = dict_mysql_update;
    dict_mysql->dict.close = dict_mysql_close;
    dict_mysql->dict.fd = -1;			/* there's no file descriptor
						 * for locking */
    dict_mysql->name = (MYSQL_NAME *) mymalloc(sizeof(MYSQL_NAME));
    dict_mysql->name = mysqlname_parse(name);
    dict_mysql->pldb = plmysql_init(dict_mysql->name->dbname,
				    dict_mysql->name->hostnames,
				    dict_mysql->name->len_hosts);
    if (dict_mysql->pldb == NULL)
	msg_fatal("couldn't intialize pldb!\n");
    connections = plmysql_connect(dict_mysql->pldb, dict_mysql->name->username,
				  dict_mysql->name->password);
    if (connections == 0)
	/* the mysql lookup mechanism will try to reconnect anyway ... */
	msg_warn("couldn't connect pldb to any database instances");
    else
	msg_info("pldb connected to %d database instances", connections);
    dict_register(name, (DICT *) dict_mysql);
    return &dict_mysql->dict;
}

/* host_init - initialize HOST structure */
static HOST host_init(char *hostname)
{
    int     stat;
    MYSQL   db;
    time_t  ts;
    HOST    host;

    host.stat = STATUNTRIED;
    host.hostname = hostname;
    host.db = db;
    host.ts = ts;
    return host;
}

/*
 * plmysql_init - initalize a MYSQL database.
 *	          Return NULL on failure, or a PLMYSQL * on success.
 */
PLMYSQL *plmysql_init(char *dbname,
		              char *hostnames[],
		              int len_hosts)
{
    PLMYSQL *PLDB;
    MYSQL  *dbs;
    int     i;
    HOST    host;

    if ((PLDB = (PLMYSQL *) mymalloc(sizeof(PLMYSQL))) == NULL) {
	msg_fatal("mymalloc of pldb failed");
    }
    PLDB->dbname = dbname;
    PLDB->len_hosts = len_hosts;
    if ((PLDB->db_hosts = (HOST *) mymalloc(sizeof(HOST) * len_hosts)) == NULL)
	return NULL;
    for (i = 0; i < len_hosts; i++) {
	PLDB->db_hosts[i] = host_init(hostnames[i]);
    }
    return PLDB;
}

/* plmysql_dealloc - free memory associated with PLMYSQL close databases */
void    plmysql_dealloc(PLMYSQL *PLDB)
{
    int     i;

    for (i = 0; i < PLDB->len_hosts; i++) {
	mysql_close(&(PLDB->db_hosts[i].db));
	myfree(PLDB->db_hosts[i].hostname);
    }
    myfree((char *) PLDB->db_hosts);
    myfree((char *) (PLDB));
}

/* plmysql_down_host - down a HOST * */
inline void plmysql_down_host(HOST *host)
{
    if (host->stat != STATFAIL)
	host->ts = time(&(host->ts));
    host->stat = STATFAIL;
}

/* plmysql_connect_single -
 * used to reconnect to a single database when one is down and as a helper for
 * plmysql_connect
 */
int     plmysql_connect_single(PLMYSQL *PLDB, int host)
{
    if ((mysql_connect(&(PLDB->db_hosts[host].db), PLDB->db_hosts[host].hostname,
		       PLDB->username, PLDB->password))) {
	if (mysql_select_db(&(PLDB->db_hosts[host].db), PLDB->dbname) == 0) {
	    PLDB->db_hosts[host].stat = STATACTIVE;
	    return 1;
	} else {
	    plmysql_down_host(&(PLDB->db_hosts[host]));
	    msg_warn("%s", mysql_error(&PLDB->db_hosts[host].db));
	}
    } else {
	plmysql_down_host(&(PLDB->db_hosts[host]));
	msg_warn("%s", mysql_error(&PLDB->db_hosts[host].db));
    }
    return 0;
}

/*
 * plmysql_connect -
 * given a PLMYSQL struct PLDB *, connect it and select db.
 * return  the number of databases successfully connected (0 for failure)
 */
int     plmysql_connect(PLMYSQL *PLDB, char *username, char *password)
{
    int     i,
            res;

    res = 0;

    PLDB->username = username;
    PLDB->password = password;

    for (i = 0; i < PLDB->len_hosts; i++) {
	res = res + plmysql_connect_single(PLDB, i);
    }
    return res;
}

/* plmysql_ready_reconn -
   given a downed HOST, return whether or not it should retry connection
*/
int     plmysql_ready_reconn(HOST host)
{
    time_t  t;
    long    now;

    now = (long) time(&t);
    if ((now - ((long) host.ts)) >= RETRY_CONN_INTV)
	return 1;
    return 0;
}

/*
 * plmysql_query - process a MySQL query.  Return 0 on success.
 *	           On failure, log failure and try other db instances.
 */

MYSQL_RES *plmysql_query(PLMYSQL *PLDB, const char *query)
{
    int     i;
    MYSQL_RES *res;

    for (i = 0; i < PLDB->len_hosts; i++) {
	if ((PLDB->db_hosts[i].stat != STATACTIVE) &&
	    (plmysql_ready_reconn(PLDB->db_hosts[i]))) {
	    msg_warn("attempting to reconnect to host");
	    plmysql_connect_single(PLDB, i);
	    continue;
	}
	if ((!(mysql_query(&(PLDB->db_hosts[i].db), query))) && \
	    (res = mysql_store_result(&(PLDB->db_hosts[i].db)))) {
	    return res;
	}
	msg_warn("%s", mysql_error(&PLDB->db_hosts[i].db));
	plmysql_down_host(&(PLDB->db_hosts[i]));
    }
    return NULL;
}

#endif
