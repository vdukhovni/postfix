#ifdef HAS_MYSQL

#include <time.h>
#include "mysql.h"

#define STATACTIVE 0
#define STATFAIL 1
#define STATUNTRIED 2
#define RETRY_CONN_INTV 300		/* 5 minutes */

extern DICT *dict_mysql_open(const char *name, int unused_flags, int dict_flags);

typedef struct {
    char   *hostname;
    int     stat;			/* STATUNTRIED | STATFAIL | STATCUR */
    time_t  ts;				/* used for attempting reconnection
					 * every so often if a host is down */
    MYSQL   db;
} HOST;


typedef struct {
    char   *username;			/* login for database */
    char   *password;			/* password for database */
    char   *dbname;			/* the name of the database on all
					 * the servers */
    HOST   *db_hosts;			/* the hosts on which the databases
					 * reside */
    int     len_hosts;			/* number of hosts */
} PLMYSQL;

extern PLMYSQL *plmysql_init(char *dbname, char *hostnames[], int len_hosts);

extern int plmysql_connect(PLMYSQL *PLDB, char *username, char *password);

MYSQL_RES *plmysql_query(PLMYSQL *PLDB, const char *query);

void    plmysql_dealloc(PLMYSQL *PLDB);

inline void plmysql_down_host(HOST *host);

int     plmysql_connect_single(PLMYSQL *PLDB, int host);

int     plmysql_ready_reconn(HOST host);

#endif
