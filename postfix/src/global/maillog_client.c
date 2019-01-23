/*++
/* NAME
/*	maillog_client 3
/* SUMMARY
/*	syslog client or internal log client
/* SYNOPSIS
/*	#include <maillog_client.h>
/*
/*	int	maillog_client_init(
/*	const char *progname,
/*	int	flags)
/* DESCRIPTION
/*	maillog_client_init() chooses between logging to syslog or
/*	to the internal postlog service, based on the value of the
/*	"maillog_file" parameter setting and postlog-related
/*	environment settings.
/*
/*	This code may be called before a process has initialized
/*	its configuration parameters. A daemon process will receive
/*	logging hints from its parent, through environment variables.
/*	In all cases, a process may invoke maillog_client_init()
/*	any time, for example, after it initializes or updates its
/*	configuration parameters.
/*
/*	Arguments:
/* .IP progname
/*	The program name that will be prepended to logfile records.
/* .IP flags
/*	Specify one of the following:
/* .RS
/* .IP MAILLOG_CLIENT_FLAG_NONE
/*	No special processing.
/* .IP MAILLOG_CLIENT_FLAG_LOGWRITER_FALLBACK
/*	Try to fall back to writing the "maillog_file" directly,
/*	if logging to the internal postlog service is enabled, but
/*	the postlog service is unavailable. If the fallback fails,
/*	die with a fatal error.
/* .RE
/* ENVIRONMENT
/* .ad
/* .fi
/*	When logging to the internal postlog service is enabled,
/*	each process exports the following information, to help
/*	initialize the logging in a child process, before the child
/*	has initialized its configuration parameters.
/* .IP POSTLOG_SERVICE
/*	The pathname of the public postlog service endpoint, usually
/*	"$queue_directory/public/$postlog_service_name".
/* .IP POSTLOG_HOSTNAME
/*	The hostname to prepend to information that is sent to the
/*	internal postlog logging service, usually "$myhostname".
/* CONFIGURATION PARAMETERS
/* .ad
/* .fi
/* .IP "maillog_file (empty)"
/*	The name of an optional logfile. If the value is empty, and
/*	the environment does not specify POSTLOG_SERVICE, the program
/*	will log to the syslog service instead.
/* .IP "myhostname (default: see postconf -d output)"
/*	The internet hostname of this mail system.
/* .IP "postlog_service_name (postlog)"
/*	The name of the internal postlog logging service.
/* SEE ALSO
/*	msg_syslog(3)   syslog client msg_logger(3)   internal
/*	logger
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this
/*	software.
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

 /*
  * Utility library.
  */
#include <logwriter.h>
#include <msg_logger.h>
#include <msg_syslog.h>
#include <safe.h>
#include <stringops.h>

 /*
  * Global library.
  */
#include <mail_params.h>
#include <mail_proto.h>
#include <maillog_client.h>
#include <msg.h>

 /*
  * Using logging to debug logging is painful.
  */
#define MAILLOG_CLIENT_DEBUG	0

 /*
  * Application-specific.
  */
static int maillog_client_flags;
static int using_syslog;

#define POSTLOG_SERVICE_ENV	"POSTLOG_SERVICE"
#define POSTLOG_HOSTNAME_ENV	"POSTLOG_HOSTNAME"

/* maillog_client_logwriter_fallback - fall back to logfile writer or bust */

static void maillog_client_logwriter_fallback(const char *text)
{
    static int fallback_guard = 0;

    if (fallback_guard == 0
	&& logwriter_one_shot(var_maillog_file, text, strlen(text)) < 0) {
	fallback_guard = 1;
	if (maillog_client_flags & MAILLOG_CLIENT_FLAG_LOGWRITER_FALLBACK)
	    msg_fatal("logfile '%s' is not available: %m", var_maillog_file);
    }
}

/* maillog_client_init - set up syslog or internal log client */

void    maillog_client_init(const char *progname, int flags)
{
    char   *maillog_file;
    char   *import_service_path;
    char   *import_hostname;
    char   *service_path;
    char   *myhostname;

    /*
     * Security: this code may run before the import_environment setting has
     * taken effect. It has to guard against privilege escalation attacks on
     * setgid programs, using malicious environment settings.
     * 
     * Import the postlog service name and hostname from the environment, if the
     * process has not yet processed main.cf and command-line options.
     */
    if ((import_service_path = safe_getenv(POSTLOG_SERVICE_ENV)) != 0
	&& *import_service_path == 0)
	import_service_path = 0;
    maillog_file = (var_maillog_file && *var_maillog_file ?
		    var_maillog_file : 0);

#if MAILLOG_CLIENT_DEBUG
#define STRING_OR_NULL(s) ((s) ? (s) : "(null)")
    msg_syslog_init(progname, LOG_PID, LOG_FACILITY);
    msg_info("import_service_path=%s", STRING_OR_NULL(import_service_path));
    msg_info("maillog_file=%s", STRING_OR_NULL(maillog_file));
#endif

    /*
     * Logging to syslog. Either internal logging is disabled, or this is a
     * non-daemon program that does not yet know its configuration parameter
     * values.
     */
    if (import_service_path == 0 && maillog_file == 0) {
	msg_syslog_init(progname, LOG_PID, LOG_FACILITY);
	using_syslog = 1;
	return;
    }

    /*
     * If we enabled syslog with the above code during an earlier call, then
     * update the 'progname' as that may have changed. However, it may be
     * 'better' to silence or unregister the msg_syslog handler.
     */
    if (using_syslog)
	msg_syslog_init(progname, LOG_PID, LOG_FACILITY);

    /*
     * Logging to postlog (or to postlog fallback file).
     */
    if ((import_hostname = safe_getenv(POSTLOG_HOSTNAME_ENV)) != 0
	&& *import_hostname == 0)
	import_hostname = 0;
    if (var_myhostname && *var_myhostname) {
	myhostname = var_myhostname;
    } else if ((myhostname = import_hostname) == 0) {
	myhostname = "amnesiac";
    }
#if MAILLOG_CLIENT_DEBUG
    msg_info("import_hostname=%s", STRING_OR_NULL(import_hostname));
    msg_info("myhostname=%s", STRING_OR_NULL(myhostname));
#endif
    if (var_postlog_service) {
	service_path = concatenate(var_queue_dir, "/", MAIL_CLASS_PUBLIC,
				   "/", var_postlog_service, (char *) 0);
    } else {
	service_path = import_service_path;
    }
    maillog_client_flags = flags;
    msg_logger_init(progname, myhostname, service_path,
		    ((flags & MAILLOG_CLIENT_FLAG_LOGWRITER_FALLBACK)
		     && maillog_file) ?
		    maillog_client_logwriter_fallback :
		    (MSG_LOGGER_FALLBACK_FN) 0);

    /*
     * After processing main.cf, export the postlog service pathname and the
     * hostname, so that a child process can log to postlogd before it has
     * processed main.cf and command-line options.
     */
    if (import_service_path == 0
	|| strcmp(service_path, import_service_path) != 0) {
#if MAILLOG_CLIENT_DEBUG
	msg_info("export %s=%s", POSTLOG_SERVICE_ENV, service_path);
#endif
	if (setenv(POSTLOG_SERVICE_ENV, service_path, 1) < 0)
	    msg_fatal("setenv: %m");
    }
    if (import_hostname == 0 || strcmp(myhostname, import_hostname) != 0) {
#if MAILLOG_CLIENT_DEBUG
	msg_info("export %s=%s", POSTLOG_HOSTNAME_ENV, myhostname);
#endif
	if (setenv(POSTLOG_HOSTNAME_ENV, myhostname, 1) < 0)
	    msg_fatal("setenv: %m");
    }
    if (service_path != import_service_path)
	myfree(service_path);
}
