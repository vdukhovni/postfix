/*++
/* NAME
/*	postlift 8
/* SUMMARY
/*	Postfix client privilege elevator
/* SYNOPSIS
/*	\fBpostlift\fR [\fB-v\fR] [\fB-c \fIconfig_dir\fR]
/* DESCRIPTION
/*	This program implements an alternative privilege elevation
/*	method for systems that disable set-gid support.
/*
/*	On these systems, postlift(8) replaces set-gid privilege
/*	elevation with a client-server mechanism. An unprivileged Postfix
/*	client command (postdrop(1), postlog(1) or postqueue(1)) connects
/*	to the postlift(8) service, requesting that postlift(8) runs
/*	the exact same command (postdrop(1) etc.) in a child process with
/*	"postdrop" user and group privileges.
/*
/*	These elevated privileges allow the child process to add a
/*	message to the protected Postfix maildrop queue, or to interact
/*	with a Postfix core daemon process. postlift(8) propagates the
/*	child process exit status back to the unprivileged client command.
/*
/*	See the Postfix NO_NEW_PRIVILEGES_README documentation for
/*	background and for activating the postlift(8) service with
/*	systemd.
/*
/*	Options:
/* .IP "\fB-c \fIconfig_dir\fR"
/*	The \fBmain.cf\fR configuration file is in the named directory
/*	instead of the default configuration directory.
/* .IP \fB-v\fR
/*	Enable verbose logging for debugging purposes. Multiple \fB-v\fR
/*	options make the software increasingly verbose.
/* DIAGNOSTICS
/*	Problems are reported to the standard error stream.
/* BUGS
/*	Email submissions from non-root users will be lost before the
/*	postlift(8) socket is activated.
/*
/*	There is no standard API to request peer credentials for a
/*	UNIX-domain connection. Consequently, this program works only
/*	on FreeBSD, Linux, MacOS, NetBSD, OpenBSD, and Solaris systems
/*	released after 2007 or so; on other systems all requests will
/*	fail.
/* ENVIRONMENT
/* .ad
/* .fi
/* .IP \fBMAIL_CONFIG\fR
/*	Directory with Postfix configuration files.
/* CONFIGURATION PARAMETERS
/* .ad
/* .fi
/*	The following \fBmain.cf\fR parameters are especially relevant
/*	to this program.
/*
/*	The text below provides only a parameter summary. See
/*	\fBpostconf\fR(5) for more details including examples.
/* CONFIGURATION CONTROLS
/* .ad
/* .fi
/* .IP "\fBconfig_directory (see 'postconf -d' output)\fR"
/*	The default location of the Postfix main.cf and master.cf
/*	configuration files.
/* RESOURCE CONTROLS
/* .ad
/* .fi
/* .IP "\fBfork_attempts (5)\fR"
/*	The maximal number of attempts to fork() a child process.
/* .IP "\fBfork_delay (1s)\fR"
/*	The delay between attempts to fork() a child process.
/* CREDENTIAL CONTROLS
/* .ad
/* .fi
/* .IP "\fBdefault_privs (nobody)\fR"
/*	The default rights used by the \fBlocal\fR(8) delivery agent for delivery
/*	to an external file or command.
/* .IP "\fBmail_owner (postfix)\fR"
/*	The UNIX system account that owns the Postfix queue and most Postfix
/*	daemon processes.
/* .IP "\fBsetgid_group (postdrop)\fR"
/*	The group ownership of set-gid Postfix commands and of group-writable
/*	Postfix directories.
/* SEE ALSO
/*	NO_NEW_PRIVILEGES_README, configuration and architecture
/*	postconf(5), Postfix configuration
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* HISTORY
/*	This service was introduced with Postfix version 3.12.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <sysexits.h>
#include <defs_for_main.h>

 /*
  * Utility library.
  */
#include <argv.h>
#include <clean_env.h>
#include <get_unix_peer_creds.h>
#include <msg.h>
#include <msg_vstream.h>
#include <stringops.h>
#include <vstring.h>

 /*
  * Global library.
  */
#include <mail_conf.h>
#include <mail_dict.h>
#include <maillog_client.h>
#include <mail_params.h>
#include <mail_parm_split.h>
#include <mail_proto.h>
#include <mail_task.h>
#include <mail_version.h>
#include <plift_clnt.h>

 /*
  * Application-specific.
  */
#include <postlift_creds.h>
#include <postlift_run.h>
#include <postlift_req.h>

 /*
  * The commands that we are willing to run with elevated privilege.
  */
static const char *allowed_cmds[] = {
    "postdrop",
    "postlog",
    "postqueue",
    0,
};

 /*
  * SLMs.
  */
#define STR(x)	vstring_str(x)

/* usage - explain */

static NORETURN usage(char *myname)
{
    msg_fatal("usage: %s [-v] [-c config_dir]", myname);
}

MAIL_VERSION_STAMP_DECLARE;

int     main(int argc, char **argv)
{
    int     fd;
    struct stat st;
    int     ch;
    char   *slash;
    uid_t   peer_uid;
    gid_t   peer_gid;
    ARGV   *import_env;
    int     rcvd_stdin_fd, rcvd_stdout_fd, rcvd_stderr_fd;
    VSTRING *rcvd_cmd;
    ARGV   *rcvd_argv;
    VSTRING *rcvd_config_dir;
    ARGV   *rcvd_envp;
    VSTRING *env_entry;
    char   *path;
    pid_t   child_pid;
    int     status;

    /*
     * Begin >70 lines of boilerplate code for commands that run with
     * effective (uid, gid) equal to real (uid, gid).
     */
#if defined(CODE_FOR_MAIN)
    CODE_FOR_MAIN();
#endif						/* CODE_FOR_MAIN */

    /*
     * Fingerprint executables and core dumps. This program does not create
     * files, but....
     */
    MAIL_VERSION_STAMP_ALLOCATE;

    /*
     * Be consistent with file permissions.
     */
    umask(022);

    /*
     * To minimize confusion, make sure that the standard file descriptors
     * are open before opening anything else. XXX Work around for 44BSD where
     * fstat can return EBADF on an open file descriptor.
     */
    for (fd = 0; fd < 3; fd++)
	if (fstat(fd, &st) == -1
	    && (close(fd), open("/dev/null", O_RDWR, 0)) != fd)
	    msg_fatal("open /dev/null: %m");

    /*
     * Process environment options as early as we can. We run with effective
     * (uid, gid) equal to real (uid, gid), and we are supposed to be running
     * in a controlled environment.
     */
    if (getenv(CONF_ENV_VERB))
	msg_verbose = 1;

    /*
     * Initialize. Set up logging. Read the global configuration file after
     * parsing command-line arguments.
     */
    if ((slash = strrchr(argv[0], '/')) != 0 && slash[1])
	argv[0] = slash + 1;
    msg_vstream_init(argv[0], VSTREAM_ERR);
    maillog_client_init(mail_task(argv[0]), MAILLOG_CLIENT_FLAG_NONE);
    set_mail_conf_str(VAR_PROCNAME, var_procname = mystrdup(argv[0]));

    /*
     * Check the Postfix library version as soon as we enable logging.
     */
    MAIL_VERSION_CHECK;

    /*
     * Parse JCL.
     */
    while ((ch = GETOPT(argc, argv, "c:v")) > 0) {
	switch (ch) {
	default:
	    usage(argv[0]);
	    break;
	case 'c':
	    if (setenv(CONF_ENV_PATH, optarg, 1) < 0)
		msg_fatal("out of memory");
	    break;
	case 'v':
	    msg_verbose++;
	    break;
	}
    }
    mail_conf_read();
    /* Enforce consistent operation of different Postfix parts. */
    import_env = mail_parm_split(VAR_IMPORT_ENVIRON, var_import_environ);
    update_env(import_env->argv);
    argv_free(import_env);
    /* Re-evaluate mail_task() after reading main.cf. */
    maillog_client_init(mail_task(argv[0]), MAILLOG_CLIENT_FLAG_NONE);
    mail_dict_init();

    /*
     * End >70 lines of boilerplate code for commands that run with effective
     * (uid, gid) equal to real (uid, gid).
     */

    /*
     * General policy: legitimate clients don't make mistakes. Terminate
     * ungracefully as soon as something is wrong.
     */

    /*
     * Enforce security zone hygiene.
     */
    plift_valid_creds_or_die(getuid(), getgid());

    /*
     * Get the peer UID. This is needed to enforce admission policy in the
     * postdrop(1) etc. client commands. With systemd we rely on
     * MaxConnectionsPerSource to limit the request concurrency by peer UID.
     * 
     * TODO(wietse) with Postfix spawn(8) integration, add master(5) syntax for
     * a new socket location type (not /var/spool/postfix/public), and add
     * spawn(8) syntax to enforce an anvil(8) concurrency limit.
     */
    if (get_unix_peer_creds(STDIN_FILENO, &peer_uid, &peer_gid) != 0)
	msg_fatal("receive peer credentials: %m");

    /*
     * Before we exchange any information with the client, announce the
     * application protocol. All errors are fatal.
     */
    plift_req_greet(VSTREAM_OUT);

    /*
     * Receive caller-specified (stdin, stdout, stderr) file descriptors, and
     * override our stderr, so that error messages will reach the caller's
     * user. All errors are fatal.
     */
    plift_req_fds(STDIN_FILENO, &rcvd_stdin_fd, &rcvd_stdout_fd,
		  &rcvd_stderr_fd);
    if (dup2(rcvd_stderr_fd, STDERR_FILENO) < 0)
	msg_fatal("dup2 received stderr to my stderr: %m");

    /*
     * Receive one size-limited blob with all other request data. This
     * implies an upper bound on the size of strings. argv_attr_scan()
     * enforces its own bound on the number of elements in an array.
     * 
     * Deserialize the received command name, command-line array, and
     * environment. The environment is limited to MAIL_CONFIG, which is the
     * only environment setting that set-gid postdrop etc. commands
     * historically will inherit.
     */
    rcvd_cmd = vstring_alloc(100);
    rcvd_argv = 0;
    rcvd_config_dir = vstring_alloc(100);
    plift_req_data(VSTREAM_IN, allowed_cmds, rcvd_cmd, &rcvd_argv,
		   rcvd_config_dir);

    /*
     * Add the received MAIL_CONFIG setting to the target environment. The
     * child process is responsible for sanitizing its environment.
     */
    rcvd_envp = argv_alloc(2);
    env_entry = vstring_alloc(20);
    if (*STR(rcvd_config_dir) != 0) {
	vstring_sprintf(env_entry, "%s=%s", CONF_ENV_PATH,
			STR(rcvd_config_dir));
	argv_add(rcvd_envp, STR(env_entry), (char *) 0);
    }

    /*
     * Add the PEER_UID for postdrop(1) etc. admission control.
     */
    vstring_sprintf(env_entry, "%s=%lu", PLIFT_ENV_PEER_UID,
		    (unsigned long) peer_uid);
    argv_add(rcvd_envp, STR(env_entry), (char *) 0);

    /*
     * Run the received command in a child process. The parent process waits
     * for the child to terminate or for the client to disconnect, and
     * reports the command exit status back to the peer.
     */
    path = concatenate(var_command_dir, "/", STR(rcvd_cmd), (char *) 0);
    if ((child_pid = plift_run(path, rcvd_argv->argv, rcvd_envp->argv,
			       rcvd_stdin_fd, rcvd_stdout_fd)) <= 0)
	status = EX_OSERR;
    else
	status = plift_wait(child_pid, STDIN_FILENO, EX_SOFTWARE);
    myfree(path);

    attr_print(VSTREAM_OUT, ATTR_FLAG_NONE,
	       SEND_ATTR_INT(MAIL_ATTR_STATUS, status),
	       ATTR_TYPE_END);
    vstream_fflush(VSTREAM_OUT);

    /*
     * Pacify memory checkers.
     */
    vstring_free(rcvd_config_dir);
    vstring_free(env_entry);
    if (rcvd_argv)
	argv_free(rcvd_argv);
    argv_free(rcvd_envp);
    vstring_free(rcvd_cmd);
    exit(0);
}
