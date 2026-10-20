#ifndef _POSTLIFT_REQ_H_INCLUDED_
#define _POSTLIFT_REQ_H_INCLUDED_

/*++
/* NAME
/*      postlift_req 3h
/* SUMMARY
/*	postlift request protocol
/* SYNOPSIS
/*      #include <postlift_req.h>
/* DESCRIPTION
/* .nf

 /*
  * External interface.
  */
extern void plift_req_greet(VSTREAM *fp);
extern void plift_req_fds(int fd, int *stdin_fd, int *stdout_fd,
			          int *stderr_fd);
extern void plift_req_data(VSTREAM *fp, const char **allowed_cmds,
			           VSTRING *rcvd_cmd, ARGV **rcvd_argv,
			           VSTRING *rcvd_config_dir);
extern void plift_req_reply(VSTREAM *fp, int status);

/* LICENSE
/* .ad
/* .fi
/*      The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*      Wietse Venema
/*      porcupine.org
/*--*/

#endif
