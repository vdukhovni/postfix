/*++
/* NAME
/*	postqueue 1
/* SUMMARY
/*	Postfix queue control
/* SYNOPSIS
/*	\fBpostqueue\fR \fB-f\fR
/* .br
/*	\fBpostqueue\fR \fB-p\fR
/* .br
/*	\fBpostqueue\fR \fB-s \fIsite\fR
/* DESCRIPTION
/*	The \fBpostqueue\fR program implements the Postfix user interface
/*	for queue management. It implements all the operations that are
/*	traditionally available via the \fBsendmail\fR(1) command.
/*
/*	The following options are recognized:
/* .IP \fB-f\fR
/*	Flush the queue: attempt to deliver all queued mail.
/*
/*	This option implements the traditional \fBsendmail -q\fR command,
/*	by contacting the Postfix \fBqmgr\fR(8) daemon.
/* .IP \fB-p\fR
/*	Produce a traditional sendmail-style queue listing.
/*
/*	This option implements the traditional \fBmailq\fR command,
/*	by contacting the Postfix \fBshowq\fR(8) daemon.
/* .IP "\fB-s \fIsite\fR"
/*	Schedule immediate delivery of all mail that is queued for the named
/*	\fIsite\fR. The site must be eligible for the "fast flush" service.
/*	See \fBflush\fR(8) for more information about the "fast flush"
/*	service.
/*
/*	This option implements the traditional \fBsendmail -qR\fIsite\fR
/*	command, by connecting to the SMTP server at \fB$myhostname\fR.
/* .IP \fB-v\fR
/*	Enable verbose logging for debugging purposes. Multiple \fB-v\fR
/*	options make the software increasingly verbose.
/* SECURITY
/* .ad
/* .fi
/*	By design, this program is set-user (or group) id, so that it
/*	can connect to public, but protected, Postfix daemon processes.
/* DIAGNOSTICS
/*	Problems are logged to \fBsyslogd\fR(8) and to the standard error
/*	stream.
/* ENVIRONMENT
/* .ad
/* .fi
/*	The program deletes most environment information, because the C
/*	library can't be trusted.
/* FILES
/*	/var/spool/postfix, mail queue
/*	/etc/postfix, configuration files
/* CONFIGURATION PARAMETERS
/* .ad
/* .fi
/* .IP \fBimport_environment\fR
/*	List of names of environment parameters that can be imported
/*	from non-Postfix processes.
/* .IP \fBqueue_directory\fR
/*	Top-level directory of the Postfix queue. This is also the root
/*	directory of Postfix daemons that run chrooted.
/* .IP \fBfast_flush_domains\fR
/*	List of domains that will receive "fast flush" service (default: all
/*	domains that this system is willing to relay mail to). This list
/*	specifies the domains that Postfix accepts in the SMTP \fBETRN\fR
/*	request and in the \fBsendmail -qR\fR command.
/* SEE ALSO
/*	sendmail(8) sendmail-compatible user interface
/*	qmgr(8) queue manager
/*	showq(8) list mail queue
/*	flush(8) fast flush service
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
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sysexits.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <clean_env.h>
#include <vstream.h>
#include <msg_vstream.h>
#include <msg_syslog.h>
#include <argv.h>
#include <safe.h>
#include <connect.h>

/* Global library. */

#include <mail_proto.h>
#include <mail_params.h>
#include <mail_conf.h>
#include <mail_task.h>
#include <debug_process.h>
#include <mail_run.h>
#include <mail_flush.h>
#include <smtp_stream.h>

/* Application-specific. */

 /*
  * Modes of operation.
  */
#define PQ_MODE_DEFAULT		0	/* noop */
#define PQ_MODE_MAILQ_LIST	1	/* list mail queue */
#define PQ_MODE_FLUSH_QUEUE	2	/* flush queue */
#define PQ_MODE_FLUSH_SITE	3	/* flush site */

 /*
  * Error handlers.
  */
static void postqueue_cleanup(void);
static NORETURN PRINTFLIKE(2, 3) fatal_error(int, const char *,...);

 /*
  * Silly little macros (SLMs).
  */
#define STR	vstring_str

/* show_queue - show queue status */

static void show_queue(void)
{
    char    buf[VSTREAM_BUFSIZE];
    VSTREAM *showq;
    int     n;

    /*
     * Connect to the show queue service. Terminate silently when piping into
     * a program that terminates early.
     */
    if ((showq = mail_connect(MAIL_CLASS_PUBLIC, MAIL_SERVICE_SHOWQ, BLOCKING)) != 0) {
	while ((n = vstream_fread(showq, buf, sizeof(buf))) > 0)
	    if (vstream_fwrite(VSTREAM_OUT, buf, n) != n
		|| vstream_fflush(VSTREAM_OUT) != 0)
		msg_fatal("write error: %m");

	if (vstream_fclose(showq))
	    msg_warn("close: %m");
    }

    /*
     * When the mail system is down, the superuser can still access the queue
     * directly. Just run the showq program in stand-alone mode.
     */
    else if (geteuid() == 0) {
	ARGV   *argv;
	int     stat;

	msg_warn("Mail system is down -- accessing queue directly");
	argv = argv_alloc(6);
	argv_add(argv, MAIL_SERVICE_SHOWQ, "-c", "-u", "-S", (char *) 0);
	for (n = 0; n < msg_verbose; n++)
	    argv_add(argv, "-v", (char *) 0);
	argv_terminate(argv);
	stat = mail_run_foreground(var_daemon_dir, argv->argv);
	argv_free(argv);
    }

    /*
     * When the mail system is down, unprivileged users are stuck, because by
     * design the mail system contains no set_uid programs. The only way for
     * an unprivileged user to cross protection boundaries is to talk to the
     * showq daemon.
     */
    else {
	fatal_error(EX_UNAVAILABLE,
		    "Queue report unavailable - mail system is down");
    }
}

/* flush_queue - force delivery */

static void flush_queue(void)
{

    /*
     * Trigger the flush queue service.
     */
    if (mail_flush_deferred() < 0)
	fatal_error(EX_UNAVAILABLE,
		    "Cannot flush mail queue - mail system is down");
}

/* chat - send command and examine reply */

static void chat(VSTREAM * fp, VSTRING * buf, const char *fmt,...)
{
    va_list ap;

    smtp_get(buf, fp, var_line_limit);
    if (STR(buf)[0] != '2')
	fatal_error(EX_UNAVAILABLE, "server rejected ETRN request: %s",
		    STR(buf));

    if (msg_verbose)
	msg_info("<<< %s", STR(buf));

    if (msg_verbose) {
	va_start(ap, fmt);
	vstring_vsprintf(buf, fmt, ap);
	va_end(ap);
	msg_info(">>> %s", STR(buf));
    }
    va_start(ap, fmt);
    smtp_vprintf(fp, fmt, ap);
    va_end(ap);
}

/* flush_site - flush mail for site */

static void flush_site(const char *site)
{
    VSTRING *buf = vstring_alloc(10);
    VSTREAM *fp;
    int     sock;
    int     status;

    /*
     * Make connection to the local SMTP server. Translate "connection
     * refused" into something less misleading.
     */
    vstring_sprintf(buf, "%s:smtp", var_myhostname);
    if ((sock = inet_connect(STR(buf), BLOCKING, 10)) < 0) {
	if (errno == ECONNREFUSED)
	    fatal_error(EX_UNAVAILABLE, "mail service at %s is down",
			var_myhostname);
	fatal_error(EX_UNAVAILABLE, "connect to mail service at %s: %m",
		    var_myhostname);
    }
    fp = vstream_fdopen(sock, O_RDWR);

    /*
     * Prepare for trouble.
     */
    vstream_control(fp, VSTREAM_CTL_EXCEPT, VSTREAM_CTL_END);
    status = vstream_setjmp(fp);
    if (status != 0) {
	switch (status) {
	case SMTP_ERR_EOF:
	    fatal_error(EX_UNAVAILABLE, "server at %s aborted connection",
			var_myhostname);
	case SMTP_ERR_TIME:
	    fatal_error(EX_IOERR,
		   "timeout while talking to server at %s", var_myhostname);
	}
    }
    smtp_timeout_setup(fp, 60);

    /*
     * Chat with the SMTP server.
     */
    chat(fp, buf, "helo %s", var_myhostname);
    chat(fp, buf, "etrn %s", site);
    chat(fp, buf, "quit");

    vstream_fclose(fp);
    vstring_free(buf);
}

static int fatal_status;

/* postqueue_cleanup - callback for the runtime error handler */

static NORETURN postqueue_cleanup(void)
{
    exit(fatal_status > 0 ? fatal_status : 1);
}

/* fatal_error - give up and notify parent */

static void fatal_error(int status, const char *fmt,...)
{
    VSTRING *text = vstring_alloc(10);
    va_list ap;

    fatal_status = status;
    va_start(ap, fmt);
    vstring_vsprintf(text, fmt, ap);
    va_end(ap);
    msg_fatal("%s", vstring_str(text));
}

/* main - the main program */

int     main(int argc, char **argv)
{
    struct stat st;
    char   *slash;
    int     c;
    int     fd;
    int     mode = PQ_MODE_DEFAULT;
    int     debug_me = 0;
    char   *site_to_flush = 0;
    ARGV   *import_env;

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
	    fatal_error(EX_UNAVAILABLE, "open /dev/null: %m");

    /*
     * Initialize. Set up logging, read the global configuration file and
     * extract configuration information. Set up signal handlers so that we
     * can clean up incomplete output.
     */
    if ((slash = strrchr(argv[0], '/')) != 0)
	argv[0] = slash + 1;
    msg_vstream_init(argv[0], VSTREAM_ERR);
    msg_syslog_init(mail_task("postqueue"), LOG_PID, LOG_FACILITY);
    set_mail_conf_str(VAR_PROCNAME, var_procname = mystrdup(argv[0]));

    /*
     * Further initialization...
     */
    mail_conf_read();

    /*
     * Strip the environment so we don't have to trust the C library.
     */
    import_env = argv_split(var_import_environ, ", \t\r\n");
    clean_env(import_env->argv);
    argv_free(import_env);

    if (chdir(var_queue_dir))
	fatal_error(EX_UNAVAILABLE, "chdir %s: %m", var_queue_dir);

    signal(SIGPIPE, SIG_IGN);
    msg_cleanup(postqueue_cleanup);

    /*
     * Parse JCL.
     */
    while ((c = GETOPT(argc, argv, "fps:v")) > 0) {
	switch (c) {
	case 'f':				/* flush queue */
	    mode = PQ_MODE_FLUSH_QUEUE;
	    break;
	case 'p':				/* traditional mailq */
	    mode = PQ_MODE_MAILQ_LIST;
	    break;
	    break;
	case 's':				/* flush site */
	    mode = PQ_MODE_FLUSH_SITE;
	    site_to_flush = optarg;
	    break;
	case 'v':
	    msg_verbose++;
	    break;
	default:
	    fatal_error(EX_USAGE, "usage: %s -[fpsv]", argv[0]);
	}
    }

    /*
     * Start processing.
     */
    switch (mode) {
    default:
	msg_panic("unknown operation mode: %d", mode);
	/* NOTREACHED */
    case PQ_MODE_MAILQ_LIST:
	show_queue();
	exit(0);
	break;
    case PQ_MODE_FLUSH_SITE:
	flush_site(site_to_flush);
	exit(0);
	break;
    case PQ_MODE_FLUSH_QUEUE:
	flush_queue();
	exit(0);
	break;
    case PQ_MODE_DEFAULT:
	fatal_error(EX_USAGE, "usage: %s -[fpsv]", argv[0]);
	break;
    }
}
