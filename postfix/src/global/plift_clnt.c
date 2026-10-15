/*++
/* NAME
/*	plift_clnt 3
/* SUMMARY
/*	postlift(8) client support
/* SYNOPSIS
/*	#include <plift_clnt.h>
/*
/*	int	plift_clnt_request(
/*	const char *service,
/*	const char *target_cmd,
/*	int	argc,
/*	char	**argv,
/*	int	stdin_fd,
/*	int	stdout_fd,
/*	int	stderr_fd,
/*	const char *config_dir)
/* DESCRIPTION
/*	plift_clnt_request() connects to the named UNIX-domain socket and
/*	sends a service request that contains a target command, a command
/*	array, file descriptors that will become the std{in,out,err}
/*	of the target command and the configuration directory that the
/*	target command will use. The serialized request is limited to
/*	1024 bytes. This is enforced in postlift(8).
/*
/*	The target command environment will be populated with MAIL_CONFIG
/*	(the config_dir value) and PEER_UID (the effective UID of the
/*	calling process, in decimal notation).
/*
/*	After the request is sent, the client simply waits until it
/*	receives the target command's exit status (zero for success,
/*	or a status defined in <sysexits.h>.
/*
/*	Arguments:
/* .IP service
/*	The pathname of a UNIX-domain socket. On Linux systems this
/*	will be managed by systemd, configured to launch the postlift(8)
/*	server, which in turn launches the target command and reports
/*	back the command exit status.
/* .IP target_cmd
/*	The base name of the postdrop(1), postlog(1) or postqueue(1)
/*	target command that postlift(8) will execute; other commands
/*	are not allowed. This is enforced in postlift(8).
/* .IP argc, argv
/*	The target command line array length and elements. Any -c options
/*	must specify an authorized pathname as described below for the
/*	config_dir argument. This is enforced in the target command.
/* .IP stdin_fd, stdout_fd, stderr_fd
/*	File descriptors that will become the std{in,out,err} of the
/*	target command.
/* .IP config_dir
/*	The MAIL_CONFIG environment setting for the target command.
/*	The pathname must be authorized in the default main.cf file (must
/*	match the default config_directory, alternate_config_directories,
/*	or multi_instance_directories). This is enforced in the target
/*	command.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>
#include <sysexits.h>

 /*
  * Utility library.
  */
#include <argv.h>
#include <argv_attr.h>
#include <connect.h>
#include <msg.h>
#include <vstream.h>

 /*
  * Global library.
  */
#include <mail_conf.h>
#include <mail_proto.h>
#include <plift_clnt.h>

 /*
  * SLMs.
  */
#define STR(x)	vstring_str(x)
#define LEN(x)	VSTRING_LEN(x)

/* plift_clnt_request - send postlift(8) request and return exit status */

int     plift_clnt_request(const char *service, const char *target_cmd,
			           int argc, char **argv, int stdin_fd,
			           int stdout_fd, int stderr_fd,
			           const char *config_dir)
{
    VSTREAM *plift_fp = 0;
    VSTREAM *serialized_fp = 0;
    VSTRING *serialized_req = 0;
    int     plift_fd;
    ARGV    argcv_wrapper;
    int     status;

    /*
     * Connect to the postlift service and validate the initial response.
     */
    if ((plift_fd = LOCAL_CONNECT(service, BLOCKING, PLIST_INIT_TIMEOUT)) < 0) {
	msg_warn("connect to %s service: %m", service);
	return (EX_UNAVAILABLE);
    }
    plift_fp = vstream_fdopen(plift_fd, O_RDWR);
    if (attr_scan(plift_fp, ATTR_FLAG_STRICT,
		  RECV_ATTR_STREQ(MAIL_ATTR_PROTO, MAIL_ATTR_PROTO_POSTLIFT),
		  RECV_ATTR_INT(MAIL_ATTR_STATUS, &status),
		  ATTR_TYPE_END) != 1) {
	msg_warn("cannot receive %s service initial response", service);
	vstream_fclose(plift_fp);
	return (EX_UNAVAILABLE);
    }
    vstream_control(plift_fp,
		    CA_VSTREAM_CTL_PATH(service),
		    CA_VSTREAM_CTL_END);

    /*
     * Send the caller-specified file descriptors.
     */
    if (LOCAL_SEND_FD(plift_fd, dup(STDIN_FILENO)) < 0) {
	msg_warn("cannot send stdin descriptor to %s service: %m", service);
	vstream_fclose(plift_fp);
	return (EX_IOERR);
    }
    if (LOCAL_SEND_FD(plift_fd, STDOUT_FILENO) < 0) {
	msg_warn("cannot send stdout descriptor to %s service: %m", service);
	vstream_fclose(plift_fp);
	return (EX_IOERR);
    }
    if (LOCAL_SEND_FD(plift_fd, STDERR_FILENO) < 0) {
	msg_warn("cannot send stderr descriptor to %s service: %m", service);
	vstream_fclose(plift_fp);
	return (EX_IOERR);
    }

    /*
     * From here on, all returns must use this macro. serialized_fp is
     * excluded here, because it is always closed before returning.
     */
#define PLIST_CLNT_RETURN(st) do { \
	int __st = (st); \
	if (plift_fp) \
	    vstream_fclose(plift_fp); \
	if (serialized_req) \
	    vstring_free(serialized_req); \
	return (__st); \
    } while (0);

    /*
     * Build the serialized request blob.
     */
    ARGV_WRAP(argcv_wrapper, argc, argv);
    serialized_req = vstring_alloc(100);
    serialized_fp = vstream_memopen(serialized_req, O_WRONLY);
    attr_print(serialized_fp, ATTR_FLAG_NONE,
	       SEND_ATTR_STR(MAIL_ATTR_ACTION, target_cmd),
	       SEND_ATTR_FUNC(argv_attr_print, (void *) &argcv_wrapper),
	       SEND_ATTR_STR(CONF_ENV_PATH, config_dir),
	       ATTR_TYPE_END);
    if (vstream_fclose(serialized_fp) != 0) {
	msg_warn("cannot serialize %s service request: %m", service);
	PLIST_CLNT_RETURN(EX_IOERR);
    }

    /*
     * Send the serialized blob.
     */
    if (attr_print(plift_fp, ATTR_FLAG_NONE,
		   SEND_ATTR_INT(MAIL_ATTR_SIZE, LEN(serialized_req)),
		   ATTR_TYPE_END) != 0
	|| vstream_fwrite(plift_fp, STR(serialized_req),
			  LEN(serialized_req)) != LEN(serialized_req)
	|| vstream_fflush(plift_fp) != 0) {
	msg_warn("cannot send serialized request to %s service: %m", service);
	PLIST_CLNT_RETURN(EX_IOERR);
    }

    /*
     * Wait for the target command's exit status, while keeping the postlift
     * connection open.
     */
    status = EX_IOERR;				/* No disappointment */

    if (attr_scan(plift_fp, ATTR_FLAG_NONE,
		  RECV_ATTR_INT(MAIL_ATTR_STATUS, &status),
		  ATTR_TYPE_END) != 1)
	msg_warn("receive command status from %s service: %m",
		 VSTREAM_PATH(plift_fp));
    PLIST_CLNT_RETURN(status);
}
