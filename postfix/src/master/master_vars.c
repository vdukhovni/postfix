/*++
/* NAME
/*	master_vars 3
/* SUMMARY
/*	Postfix master - global configuration file access
/* SYNOPSIS
/*	#include "master.h"
/*
/*	void	master_vars_init()
/* DESCRIPTION
/*	master_vars_init() reads values from the global Postfix configuration
/*	file and assigns them to tunable program parameters. Where no value
/*	is specified, a compiled-in default value is used.
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

/* System library. */

#include <sys_defs.h>
#include <string.h>
#include <unistd.h>

/* Utility library. */

#include <msg.h>
#include <stringops.h>
#include <mymalloc.h>

/* Global library. */

#include <mail_conf.h>
#include <mail_params.h>

/* Application-specific. */

#include "master.h"

 /*
  * Tunable parameters.
  */
char   *var_inet_protocols;
int     var_proc_limit;
int     var_throttle_time;
char   *var_master_disable;

 /*
  * Support to warn about main.cf parameters that can only be initialized but
  * not updated, and to initialize or update data structures that derive
  * values from main.cf parameters. Add similar code if we also need to
  * monitor non-string parameters.
  */
typedef struct MASTER_VARS_STR_WATCH {
    const char *name;			/* parameter name */
    char  **value;			/* current main.cf value */
    char  **backup;			/* actual value that is being used */
    int     flags;			/* see below */
    void    (*assign) (void);		/* init or update data structure */
} MASTER_VARS_STR_WATCH;

typedef struct MASTER_VARS_INT_WATCH {
    const char *name;			/* parameter name */
    int    *value;			/* current main.cf value */
    int    *backup;			/* actual value that is being used */
    int     flags;			/* see below */
    void    (*assign) (void);		/* init or update data structure */
} MASTER_VARS_INT_WATCH;

#define MASTER_VARS_WATCH_FLAG_UPDATE	(1<<0)	/* support update after init */
#define MASTER_VARS_WATCH_FLAG_ISSET	(1<<1)	/* backup is initialized */

/* master_vars_str_watch - watch string-valued parameters for change */

static void master_vars_str_watch(MASTER_VARS_STR_WATCH *str_watch_table)
{
    MASTER_VARS_STR_WATCH *wp;

    for (wp = str_watch_table; wp->name != 0; wp++) {

	/*
	 * Detect changes to monitored parameter values. If a change is
	 * supported, we discard the backed up value and update it to the
	 * current value later. Otherwise we complain.
	 */
	if (wp->backup[0] != 0
	    && strcmp(wp->backup[0], wp->value[0]) != 0) {
	    if ((wp->flags & MASTER_VARS_WATCH_FLAG_UPDATE) == 0) {
		msg_warn("ignoring %s parameter value change", wp->name);
		msg_warn("old value: \"%s\", new value: \"%s\"",
			 wp->backup[0], wp->value[0]);
		msg_warn("to change %s, stop and start Postfix", wp->name);
	    } else {
		myfree(wp->backup[0]);
		wp->backup[0] = 0;
	    }
	}

	/*
	 * Initialize the backed up parameter value, or update if it this
	 * parameter supports updates after initialization. Optionally assign
	 * the parameter value to an application-specific data structure.
	 */
	if (wp->backup[0] == 0) {
	    if (wp->assign != 0)
		wp->assign();
	    wp->backup[0] = mystrdup(wp->value[0]);
	}
    }
}

/* master_vars_int_watch - watch integer-valued parameters for change */

static void master_vars_int_watch(MASTER_VARS_INT_WATCH *str_watch_table)
{
    MASTER_VARS_INT_WATCH *wp;

    for (wp = str_watch_table; wp->name != 0; wp++) {

	/*
	 * Detect changes to monitored parameter values. If a change is
	 * supported, we discard the backed up value and update it to the
	 * current value later. Otherwise we complain.
	 */
	if ((wp->flags & MASTER_VARS_WATCH_FLAG_ISSET) != 0
	    && wp->backup[0] != wp->value[0]) {
	    if ((wp->flags & MASTER_VARS_WATCH_FLAG_UPDATE) == 0) {
		msg_warn("ignoring %s parameter value change", wp->name);
		msg_warn("old value: \"%d\", new value: \"%d\"",
			 wp->backup[0], wp->value[0]);
		msg_warn("to change %s, stop and start Postfix", wp->name);
	    } else {
		wp->flags &= ~MASTER_VARS_WATCH_FLAG_ISSET;
	    }
	}

	/*
	 * Initialize the backed up parameter value, or update if it this
	 * parameter supports updates after initialization. Optionally assign
	 * the parameter value to an application-specific data structure.
	 */
	if ((wp->flags & MASTER_VARS_WATCH_FLAG_ISSET) == 0) {
	    if (wp->assign != 0)
		wp->assign();
	    wp->flags |= MASTER_VARS_WATCH_FLAG_ISSET;
	    wp->backup[0] = wp->value[0];
	}
    }
}

/* master_vars_init - initialize from global Postfix configuration file */

void    master_vars_init(void)
{
    char   *path;
    static const CONFIG_STR_TABLE str_table[] = {
	VAR_INET_PROTOCOLS, DEF_INET_PROTOCOLS, &var_inet_protocols, 1, 0,
	VAR_MASTER_DISABLE, DEF_MASTER_DISABLE, &var_master_disable, 0, 0,
	0,
    };
    static const CONFIG_INT_TABLE int_table[] = {
	VAR_PROC_LIMIT, DEF_PROC_LIMIT, &var_proc_limit, 1, 0,
	0,
    };
    static const CONFIG_TIME_TABLE time_table[] = {
	VAR_THROTTLE_TIME, DEF_THROTTLE_TIME, &var_throttle_time, 1, 0,
	0,
    };
    static char *saved_inet_protocols;
    static char *saved_queue_dir;
    static char *saved_config_dir;
    static MASTER_VARS_STR_WATCH str_watch_table[] = {
	VAR_CONFIG_DIR, &var_config_dir, &saved_config_dir, 0, 0,
	VAR_QUEUE_DIR, &var_queue_dir, &saved_queue_dir, 0, 0,
	VAR_INET_PROTOCOLS, &var_inet_protocols, &saved_inet_protocols, 0, 0,
	/* XXX Add inet_interfaces here after this code is burned in. */
	0,
    };
    static int saved_inet_windowsize;
    static MASTER_VARS_INT_WATCH int_watch_table[] = {
	VAR_INET_WINDOW, &var_inet_windowsize, &saved_inet_windowsize, 0, 0,
	0,
    };

    /*
     * Flush existing main.cf settings, so that we handle deleted main.cf
     * settings properly.
     */
    mail_conf_flush();
    set_mail_conf_str(VAR_PROCNAME, var_procname);
    mail_conf_read();
    get_mail_conf_str_table(str_table);
    get_mail_conf_int_table(int_table);
    get_mail_conf_time_table(time_table);
    path = concatenate(var_config_dir, "/", MASTER_CONF_FILE, (char *) 0);
    fset_master_ent(path);
    myfree(path);

    /*
     * Look for parameter changes that require special attention.
     */
    master_vars_str_watch(str_watch_table);
    master_vars_int_watch(int_watch_table);
}
