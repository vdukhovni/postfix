/*++
/* NAME
/*	crate_clnt 3
/* SUMMARY
/*	connection rate client interface
/* SYNOPSIS
/*	#include <crate_clnt.h>
/*
/*	CRATE_CLNT *crate_clnt_create(void)
/*
/*	void	crate_clnt_free(crate_clnt)
/*	CRATE_CLNT *crate_clnt;
/*
/*	int	crate_clnt_connect(crate_clnt, service, addr,
/*					count, rate)
/*	CRATE_CLNT *crate_clnt;
/*	const char *service;
/*	const char *addr;
/*	int	*count;
/*	int	*rate;
/*
/*	int	crate_clnt_disconnect(crate_clnt, service, addr)
/*	CRATE_CLNT *crate_clnt;
/*	const char *service;
/*	const char *addr;
/*
/*	int	crate_clnt_lookup(crate_clnt, service, addr,
/*					count, rate)
/*	CRATE_CLNT *crate_clnt;
/*	const char *service;
/*	const char *addr;
/*	int	*count;
/*	int	*rate;
/* DESCRIPTION
/*	crate_clnt_create() instantiates a crate service client endpoint.
/*
/*	crate_clnt_connect() informs the crate server that a
/*	client has connected, and returns the current connection
/*	count and connection rate for that client.
/*
/*	crate_clnt_disconnect() informs the crate server that a
/*	client has disconnected.
/*
/*	crate_clnt_lookup() looks up the current connection
/*	count and connection rate for that client.
/*
/*	crate_clnt_free() destroys a crate service client endpoint.
/*
/*	Arguments:
/* .IP crate_clnt
/*	Client rate control service handle.
/* .IP service
/*	The service that the remote client is connected to.
/* .IP addr
/*	Null terminated string that identifies the remote client.
/* .IP count
/*	Pointer to storage for the current number of connections from
/*	this remote client.
/* .IP rate
/*	Pointer to storage for the current connection rate for this
/*	remote client.
/* DIAGNOSTICS
/*	crate_clnt_connect() and crate_clnt_disconnect() return
/*	CRATE_STAT_OK in case of success, CRATE_STAT_FAIL otherwise
/*	(either the communication with the server is broken or the
/*	server experienced a problem).
/* SEE ALSO
/*	crate(8) Postfix client rate control service
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

/* Utility library. */

#include <mymalloc.h>
#include <msg.h>
#include <attr_clnt.h>
#include <stringops.h>

/* Global library. */

#include <mail_proto.h>
#include <mail_params.h>
#include <crate_clnt.h>

/* Application specific. */

#define CRATE_IDENT(service, addr) \
    printable(concatenate(service, ":", addr, (char *) 0), '?')

/* crate_clnt_create - instantiate connection rate service client */

CRATE_CLNT *crate_clnt_create(void)
{
    ATTR_CLNT *crate_clnt;

    crate_clnt = attr_clnt_create(var_crate_service, var_ipc_timeout, 0, 0);
    return ((CRATE_CLNT *) crate_clnt);
}

/* crate_clnt_free - destroy connection rate service client */

void    crate_clnt_free(CRATE_CLNT * crate_clnt)
{
    attr_clnt_free((ATTR_CLNT *) crate_clnt);
}

/* crate_clnt_lookup - status query */

int     crate_clnt_lookup(CRATE_CLNT * crate_clnt, const char *service,
			        const char *addr, int *count, int *rate)
{
    char   *ident = CRATE_IDENT(service, addr);
    int     status;

    if (attr_clnt_request((ATTR_CLNT *) crate_clnt,
			  ATTR_FLAG_NONE,	/* Query attributes. */
			  ATTR_TYPE_STR, CRATE_ATTR_REQ, CRATE_REQ_LOOKUP,
			  ATTR_TYPE_STR, CRATE_ATTR_IDENT, ident,
			  ATTR_TYPE_END,
			  ATTR_FLAG_MISSING,	/* Reply attributes. */
			  ATTR_TYPE_NUM, CRATE_ATTR_STATUS, &status,
			  ATTR_TYPE_NUM, CRATE_ATTR_COUNT, count,
			  ATTR_TYPE_NUM, CRATE_ATTR_RATE, rate,
			  ATTR_TYPE_END) != 3)
	status = CRATE_STAT_FAIL;
    myfree(ident);
    return (status);
}

/* crate_clnt_connect - heads-up and policy query */

int     crate_clnt_connect(CRATE_CLNT * crate_clnt, const char *service,
			           const char *addr, int *count, int *rate)
{
    char   *ident = CRATE_IDENT(service, addr);
    int     status;

    if (attr_clnt_request((ATTR_CLNT *) crate_clnt,
			  ATTR_FLAG_NONE,	/* Query attributes. */
			  ATTR_TYPE_STR, CRATE_ATTR_REQ, CRATE_REQ_CONN,
			  ATTR_TYPE_STR, CRATE_ATTR_IDENT, ident,
			  ATTR_TYPE_END,
			  ATTR_FLAG_MISSING,	/* Reply attributes. */
			  ATTR_TYPE_NUM, CRATE_ATTR_STATUS, &status,
			  ATTR_TYPE_NUM, CRATE_ATTR_COUNT, count,
			  ATTR_TYPE_NUM, CRATE_ATTR_RATE, rate,
			  ATTR_TYPE_END) != 3)
	status = CRATE_STAT_FAIL;
    myfree(ident);
    return (status);
}

/* crate_clnt_disconnect - heads-up only */

int     crate_clnt_disconnect(CRATE_CLNT * crate_clnt, const char *service,
			              const char *addr)
{
    char   *ident = CRATE_IDENT(service, addr);
    int     status;

    if (attr_clnt_request((ATTR_CLNT *) crate_clnt,
			  ATTR_FLAG_NONE,	/* Query attributes. */
			  ATTR_TYPE_STR, CRATE_ATTR_REQ, CRATE_REQ_DISC,
			  ATTR_TYPE_STR, CRATE_ATTR_IDENT, ident,
			  ATTR_TYPE_END,
			  ATTR_FLAG_MISSING,	/* Reply attributes. */
			  ATTR_TYPE_NUM, CRATE_ATTR_STATUS, &status,
			  ATTR_TYPE_END) != 1)
	status = CRATE_STAT_FAIL;
    myfree(ident);
    return (status);
}

#ifdef TEST

 /*
  * Stand-alone client for testing.
  */
#include <unistd.h>
#include <string.h>
#include <msg_vstream.h>
#include <mail_conf.h>
#include <mail_params.h>
#include <vstring_vstream.h>

int     main(int unused_argc, char **argv)
{
    VSTRING *inbuf = vstring_alloc(1);
    char   *bufp;
    char   *cmd;
    char   *service;
    char   *addr;
    int     count;
    int     rate;
    CRATE_CLNT *crate;

    msg_vstream_init(argv[0], VSTREAM_ERR);

    mail_conf_read();
    msg_info("using config files in %s", var_config_dir);
    if (chdir(var_queue_dir) < 0)
	msg_fatal("chdir %s: %m", var_queue_dir);

    msg_verbose++;

    crate = crate_clnt_create();

    while (vstring_fgets_nonl(inbuf, VSTREAM_IN)) {
	bufp = vstring_str(inbuf);
	if ((cmd = mystrtok(&bufp, " ")) == 0 || *bufp == 0
	    || (service = mystrtok(&bufp, " ")) == 0 || *service == 0
	    || (addr = mystrtok(&bufp, " ")) == 0 || *addr == 0
	    || mystrtok(&bufp, " ") != 0) {
	    vstream_printf("usage: connect service addr|disconnect service addr\n");
	    vstream_fflush(VSTREAM_OUT);
	    continue;
	}
	if (strncmp(cmd, "connect", 1) == 0) {
	    if (crate_clnt_connect(crate, service, addr, &count, &rate) != CRATE_STAT_OK)
		msg_warn("error!");
	    else
		vstream_printf("count=%d, rate=%d\n", count, rate);
	} else if (strncmp(cmd, "disconnect", 1) == 0) {
	    if (crate_clnt_disconnect(crate, service, addr) != CRATE_STAT_OK)
		msg_warn("error!");
	    else
		vstream_printf("OK\n");
	} else if (strncmp(cmd, "lookup", 1) == 0) {
	    if (crate_clnt_lookup(crate, service, addr, &count, &rate) != CRATE_STAT_OK)
		msg_warn("error!");
	    else
		vstream_printf("count=%d, rate=%d\n", count, rate);
	} else
	    vstream_printf("usage: connect ident|disconnect ident\n");
	vstream_fflush(VSTREAM_OUT);
    }
    vstring_free(inbuf);
    crate_clnt_free(crate);
    return (0);
}

#endif
