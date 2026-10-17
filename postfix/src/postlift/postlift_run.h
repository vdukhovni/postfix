#ifndef _POSTLIFT_RUN_H_INCLUDED_
#define _POSTLIFT_RUN_H_INCLUDED_

/*++
/* NAME
/*	postlift_run 3h
/* SUMMARY
/*	postlift process management
/* SYNOPSIS
/*	#include <postlift_run.h>
/* DESCRIPTION
/* .nf

 /*
  * External interface.
  */
extern pid_t plift_run(const char *path, char **argv, char **envp,
		               int stdin_fd, int stdout_fdd);
extern int plift_wait(pid_t pid, int client_fd, int status);

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

#endif
