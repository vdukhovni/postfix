/*++
/* NAME
/*	postlift 8
/* SUMMARY
/*	Postfix client privilege elevator
/* SYNOPSIS
/*	\fBpostlift\fR [\fB-v\fR] [\fB-c \fIconfig_dir\fR]
/* DESCRIPTION
/*	On systems that disable set-gid privilege elevation, an
/*	unprivileged Postfix postdrop(1), postlog(1) and postqueue(1)
/*	client command will connect to the postlift(8) service, requesting
/*	that postlift(8) runs the same command (postdrop(1) etc.) in a
/*	child process with "postdrop" user and group privileges.
/*
/*	This allows the elevated child process to add a message to
/*	the protected Postfix maildrop queue, or to interact with a
/*	Postfix core daemon process. postlift(8) propagates the exit
/*	status from the elevated child process back to the unprivileged
/*	client command.
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
/*	Temporary limitation: When an unprivileged client command is
/*	waiting for an elevated child process, killing the unprivileged
/*	client will not terminate the elevated child. This can cause
/*	multiple processes to read from the same terminal simultaneously,
/*	making the terminal unusable. To minimize this risk, client
/*	commands that read from stdin will automatically time out after
/*	a few seconds if stdin is connected to a terminal.
/*
/*	Email submissions from non-root users will be lost before the
/*	postlift(8) socket is activated.
/*
/*	There is no system-independent API to request peer credentials
/*	for a UNIX-domain socket. Consequently, this program is
/*	broken on some versions of some systems.
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
/* .IP "\fBconfig_directory (see 'postconf -d' output)\fR"
/*	The default location of the Postfix main.cf and master.cf
/*	configuration files.
/* .IP "\fBmail_owner (postfix)\fR"
/*	The UNIX system account that owns the Postfix queue and most Postfix
/*	daemon processes.
/* .IP "\fBfork_attempts (5)\fR"
/*	The maximal number of attempts to fork() a child process.
/* .IP "\fBfork_delay (1s)\fR"
/*	The delay between attempts to fork() a child process.
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
#define _GNU_SOURCE			/* Must be first */
#include <sys_defs.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <sysexits.h>
#include <unistd.h>
#include <defs_for_main.h>

 /*
  * Utility library.
  */
#include <argv.h>
#include <argv_attr.h>
#include <attr.h>
#include <clean_env.h>
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
#include <mypwd.h>
#include <plift_clnt.h>

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

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

/* run_with_redirect - fork() and execve() command with I/O redirect */

static int run_with_redirect(const char *path, char **argv, char **envp,
			             int stdin_fd, int stdout_fd)
{
    int     count;

    /*
     * The exit status will be used by a Zone 1 client process. If no status
     * is available, return something from <sysexits.h>.
     */
    int     default_status = EX_OSERR;

    for (count = 0; count < var_fork_tries; count++) {
	pid_t   pid, wpid;
	int     status;

	switch (pid = fork()) {
	case -1:
	    msg_warn("fork %s: %m", path);
	    break;
	case 0:
	    /* STDERR_FILENO was already set up in the parent. */
	    if (dup2(stdin_fd, STDIN_FILENO) < 0
		|| dup2(stdout_fd, STDOUT_FILENO) < 0)
		msg_fatal("dup2 stdin or stdout: %m");
	    execve(path, argv, envp);
	    msg_fatal("execv %s: %m", path);
	    /* NOTREACHED */
	default:
	    do {
		wpid = waitpid(pid, &status, 0);
	    } while (wpid == -1 && errno == EINTR);
	    return (wpid == -1 ? default_status :
		    WIFEXITED(status) ? WEXITSTATUS(status) :
		    WIFSIGNALED(status) ? EX_SOFTWARE : default_status);
	}
	sleep(var_fork_delay);
    }
    return (default_status);
}

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
    uid_t   my_uid;
    gid_t   my_gid;
    struct mypasswd *postdrop_pwd;
    struct ucred peer_creds;
    SOCKOPT_SIZE peer_cr_len = sizeof(peer_creds);
    ARGV   *import_env;
    int     rcvd_stdin_fd, rcvd_stdout_fd, rcvd_stderr_fd;
    VSTRING *rcvd_cmd;
    VSTREAM *serialized_fp;
    int     serialized_size;
    VSTRING *serialized_buf;
    ARGV   *rcvd_argv;
    VSTRING *rcvd_config_dir;
    ARGV   *rcvd_envp;
    VSTRING *env_entry;
    const char **cpp;
    char   *path;
    int     status;

    /*
     * Begin ~70 lines of boilerplate code for commands that run with
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
     * End ~70 lines of boilerplate code for commands that run with effective
     * (uid, gid) equal to real (uid, gid).
     */

    /*
     * Sanity checks: UID.
     */
    if ((my_uid = getuid()) != geteuid())
	msg_fatal("This program MUST NOT run with set-uid privileges");
    if ((postdrop_pwd = mypwnam(var_sgid_group)) == 0)
	msg_fatal("No '%s' account in the passwd database", var_sgid_group);
    if (my_uid != postdrop_pwd->pw_uid)
	msg_fatal("This program MUST run with the '%s' user ID (%lu)",
		  var_sgid_group, (unsigned long) postdrop_pwd->pw_uid);

    /*
     * Don't allow UID sharing with other accounts. Some of these checks may
     * overlap with similar checks in mail_params.c.
     */
    if (my_uid == 0)
	msg_fatal("This program MUST NOT run with the super-user ID (0)");
    if (my_uid == var_owner_uid)
	msg_fatal("This program MUST NOT run with the '%s' user ID (%lu)",
		  var_mail_owner, (unsigned long) var_owner_uid);
    if (my_uid == var_default_uid)
	msg_fatal("This program MUST NOT run with the '%s' user ID (%lu)",
		  var_default_privs, (unsigned long) var_default_uid);

    /*
     * Sanity checks: GID.
     */
    if ((my_gid = getgid()) != getegid())
	msg_fatal("This program MUST NOT with set-gid privileges");
    if (my_gid != var_sgid_gid)
	msg_fatal("This program MUST run with the '%s' group ID (%lu)",
		  var_sgid_group, (unsigned long) var_sgid_gid);

    /*
     * Don't allow GID sharing with other accounts. Some of these checks may
     * overlap with similar checks in mail_params.c.
     */
    if (my_gid == 0)
	msg_fatal("This program MUST NOT run with privileged group ID (0)");
    if (my_gid == var_owner_gid)
	msg_fatal("This program MUST NOT run with the '%s' group ID (%lu)",
		  var_mail_owner, (unsigned long) var_owner_gid);
    if (my_gid == var_default_uid)
	msg_fatal("This program MUST NOT run with the '%s' user ID (%lu)",
		  var_default_privs, (unsigned long) var_default_gid);

    /*
     * General policy: legitimate clients don't make mistakes. Terminate
     * ungracefully as soon as something is wrong.
     */

    /*
     * Get the peer UID. This is needed to enforce admission policy in the
     * postdrop(1) etc. client commands. With systemd we rely on
     * MaxConnectionsPerSource to limit the request concurrency by peer UID.
     * 
     * TODO(wietse) with Postfix spawn(8) integration, add master(5) syntax for
     * a new socket location type (not /var/spool/postfix/public), and add
     * spawn(8) syntax to enforce an anvil(8) concurrency limit.
     */
    if (getsockopt(STDIN_FILENO, SOL_SOCKET,
		   SO_PEERCRED, &peer_creds, &peer_cr_len) != 0)
	msg_fatal("receive peer credentials: %m");

    /*
     * Before we exchange any information with the client, announce the
     * application protocol.
     */
    attr_print(VSTREAM_OUT, ATTR_FLAG_NONE,
	       SEND_ATTR_STR(MAIL_ATTR_PROTO, MAIL_ATTR_PROTO_POSTLIFT),
	       SEND_ATTR_INT(MAIL_ATTR_STATUS, 0),
	       ATTR_TYPE_END);
    vstream_fflush(VSTREAM_OUT);

    /*
     * Receive caller-specified (stdin, stdout, stderr) file descriptors, and
     * override our stderr, so that error messages will reach the caller's
     * user.
     */
    if ((rcvd_stdin_fd = LOCAL_RECV_FD(STDIN_FILENO)) < 0)
	msg_fatal("receive client stdin=%d: %m", rcvd_stdin_fd);
    if ((rcvd_stdout_fd = LOCAL_RECV_FD(STDIN_FILENO)) < 0)
	msg_fatal("receive client stdout: %m");
    if ((rcvd_stderr_fd = LOCAL_RECV_FD(STDIN_FILENO)) < 0)
	msg_fatal("receive client stderr: %m");
    if (dup2(rcvd_stderr_fd, STDERR_FILENO) < 0)
	msg_fatal("dup2 received stderr to my stderr: %m");

    /*
     * Receive one size-limited blob with all other request data. This
     * implies an upper bound on the size of strings. argv_attr_scan()
     * enforces its own bound on the number of array elements.
     */
    if (attr_scan(VSTREAM_IN, ATTR_FLAG_NONE,
		  RECV_ATTR_INT(MAIL_ATTR_SIZE, &serialized_size),
		  ATTR_TYPE_END) != 1)
	msg_fatal("receive serialized size");
    if (serialized_size < 1 || serialized_size > PLIST_REQ_MAXLEN)
	msg_fatal("serialized request too large");
    serialized_buf = vstring_alloc(serialized_size);
    if (vstream_fread(VSTREAM_IN, STR(serialized_buf),
		      serialized_size) != serialized_size)
	msg_fatal("receive serialized request data: %m");
    vstring_set_payload_size(serialized_buf, serialized_size);
    serialized_fp = vstream_memopen(serialized_buf, O_RDONLY);

    /*
     * Deserialize the received command name, command-line array, and
     * environment. The environment is limited to MAIL_CONFIG, which is the
     * only environment setting that set-gid postdrop etc. commands
     * historically will inherit.
     */
    rcvd_cmd = vstring_alloc(100);
    rcvd_argv = 0;
    rcvd_config_dir = vstring_alloc(100);
    if (attr_scan(serialized_fp, ATTR_FLAG_NONE,
		  RECV_ATTR_STR(MAIL_ATTR_ACTION, rcvd_cmd),
		  RECV_ATTR_FUNC(argv_attr_scan, (void *) &rcvd_argv),
		  RECV_ATTR_STR(CONF_ENV_PATH, rcvd_config_dir),
		  ATTR_TYPE_END) != 3)
	msg_fatal("receive request parameters: %m");
    vstream_fclose(serialized_fp);
    vstring_free(serialized_buf);

    /*
     * Input sanitization: Validate the received command name.
     */
    for (cpp = allowed_cmds; /* see below */ ; cpp++) {
	if (*cpp == 0)
	    msg_fatal_status(0, "invalid command name: '%s'", STR(rcvd_cmd));
	if (strcmp(STR(rcvd_cmd), *cpp) == 0)
	    break;
    }

    /*
     * Input sanitization: Validate the received command array size. The
     * child process is responsible for sanitizing its command line.
     */
    if (rcvd_argv == 0 || rcvd_argv->argc < 1)
	msg_fatal_status(0, "empty command line array");

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
    vstring_sprintf(env_entry, "%s=%lu", PLIST_ENV_PEER_UID,
		    (unsigned long) peer_creds.uid);
    argv_add(rcvd_envp, STR(env_entry), (char *) 0);

    /*
     * Run the received command in a child process. The parent process holds
     * the peer connection open, and reports the command status back to the
     * peer.
     */
    path = concatenate(var_command_dir, "/", STR(rcvd_cmd), (char *) 0);
    status = run_with_redirect(path, rcvd_argv->argv, rcvd_envp->argv,
			       rcvd_stdin_fd, rcvd_stdout_fd);
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
