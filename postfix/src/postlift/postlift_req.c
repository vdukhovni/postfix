/*++
/* NAME
/*	postlift_run 8
/* SUMMARY
/*	postlift request protocol
/* SYNOPSIS
/*	void plift_req_greet(VSTREAM *fp)
/*
/*	void plift_req_fds(int fd, int *stdin_fd, int *stdout_fd,
/*				int *stderr_fd)
/*
/*	void plift_req_data(VSTREAM *fp, const char **allowed_cmds,
/*				VSTRING *rcvd_cmd, ARGV **rcvd_argv,
/*				VSTRING *rcvd_config_dir);
/*
/*	void plift_req_reply(VSTREAM *fp, int status);
/* DESCRIPTION
/*	This module encapsulates all client-postlift communication.
/*	All errors are fatal.
/*
/*	plift_req_greet() sends the protocol identification.
/*
/*	plift_req_fds() receives the client's std{in,out,err} file
/*	descriptors.
/*
/*	plift_req_data() receives information for running a child process:
/*	the requested command, command-line array, and MAIL_CONFIG
/*	environment setting. It validates the received command against
/*	a list of allowed commands, and that the command-line array is
/*	non-empty. Upon entry, *rcvd_argv must be null.
/*
/*	plift_req_reply() sends the child process exit status if the
/*	child terminated with exit(), EX_OSERR if fork() failed, or
/*	EX_SOFTWARE if no exit status is available.
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

 /*
  * Utility library.
  */
#include <argv.h>
#include <argv_attr.h>
#include <attr.h>
#include <msg.h>
#include <vstring.h>

 /*
  * Global library.
  */
#include <mail_conf.h>
#include <mail_proto.h>
#include <plift_clnt.h>

 /*
  * Application-specific.
  */
#include <postlift_req.h>

 /*
  * SLMs.
  */
#define STR(x)  vstring_str(x)

/* plift_req_greet - send server greeting */

void    plift_req_greet(VSTREAM *fp)
{
    /* TODO(wietse) eliminate the status. */
    attr_print(fp, ATTR_FLAG_NONE,
	       SEND_ATTR_STR(MAIL_ATTR_PROTO, MAIL_ATTR_PROTO_POSTLIFT),
	       SEND_ATTR_INT(MAIL_ATTR_STATUS, 0),
	       ATTR_TYPE_END);
    vstream_fflush(fp);
}

/* plift_req_fds - receive client std{in,out,err} descriptors */

void    plift_req_fds(int fd, int *rcvd_stdin_fd, int *rcvd_stdout_fd,
		              int *rcvd_stderr_fd)
{
    if ((*rcvd_stdin_fd = LOCAL_RECV_FD(STDIN_FILENO)) < 0)
	msg_fatal("receive client stdin: %m");
    if ((*rcvd_stdout_fd = LOCAL_RECV_FD(STDIN_FILENO)) < 0)
	msg_fatal("receive client stdout: %m");
    if ((*rcvd_stderr_fd = LOCAL_RECV_FD(STDIN_FILENO)) < 0)
	msg_fatal("receive client stderr: %m");
}

/* plift_req_data - receive and validate client request data */

void    plift_req_data(VSTREAM *client_fp, const char **allowed_cmds,
		               VSTRING *rcvd_cmd, ARGV **rcvd_argv,
		               VSTRING *rcvd_config_dir)
{
    VSTREAM *serialized_fp;
    int     serialized_size;
    VSTRING *serialized_buf;
    const char *const * cpp;

    if (*rcvd_argv != 0)
	msg_panic("%s: *rcvd_argv must be null", __func__);

    /*
     * Receive one size-limited blob with request data. This limit implies an
     * upper bound on the size of any strings in the request.
     * argv_attr_scan() enforces its own bound on the number of elements in
     * an array.
     */
    if (attr_scan(client_fp, ATTR_FLAG_NONE,
		  RECV_ATTR_INT(MAIL_ATTR_SIZE, &serialized_size),
		  ATTR_TYPE_END) != 1)
	msg_fatal("receive serialized size");
    if (serialized_size < 1 || serialized_size > PLIFT_REQ_MAXLEN)
	msg_fatal("serialized request too large");
    serialized_buf = vstring_alloc(serialized_size);
    if (vstream_fread(client_fp, STR(serialized_buf),
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
    if (attr_scan(serialized_fp, ATTR_FLAG_NONE,
		  RECV_ATTR_STR(MAIL_ATTR_ACTION, rcvd_cmd),
		  RECV_ATTR_FUNC(argv_attr_scan, (void *) rcvd_argv),
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
     * child process is responsible for sanitizing command content.
     */
    if (*rcvd_argv == 0 || (*rcvd_argv)->argc < 1)
	msg_fatal_status(0, "empty command line array for '%s'", STR(rcvd_cmd));
}

/* plift_req_reply - send final reply to client */

void    plift_req_reply(VSTREAM *fp, int status)
{
    attr_print(fp, ATTR_FLAG_NONE,
	       SEND_ATTR_INT(MAIL_ATTR_STATUS, status),
	       ATTR_TYPE_END);
    (void) vstream_fflush(fp);
}
