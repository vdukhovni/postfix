/*++
/* NAME
/*	smtp-sink 8
/* SUMMARY
/*	smtp test server
/* SYNOPSIS
/*	smtp-sink [-c] [-v] [host]:port backlog
/* DESCRIPTION
/*      \fIsmtp-sink\fR listens on the named host (or address) and port.
/*	It takes SMTP messages from the network and throws them away.
/*	This program is the complement of the \fIsmtp-source\fR program.
/* .IP -c
/*	Display a running counter that is updated whenever an SMTP
/*	QUIT command is executed.
/* .IP -v
/*	Show the SMTP conversations.
/* SEE ALSO
/*	smtp-source, SMTP test message generator
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
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

/* Utility library. */

#include <msg.h>
#include <vstring.h>
#include <vstream.h>
#include <vstring_vstream.h>
#include <get_hostname.h>
#include <listen.h>
#include <events.h>
#include <mymalloc.h>
#include <iostuff.h>
#include <msg_vstream.h>

/* Global library. */

#include <smtp_stream.h>

/* Application-specific. */

struct data_state {
    VSTREAM *stream;
    int     state;
};

#define ST_ANY			0
#define ST_CR			1
#define ST_CR_LF		2
#define ST_CR_LF_DOT		3
#define ST_CR_LF_DOT_CR		4
#define ST_CR_LF_DOT_CR_LF	5

static int var_tmout;
static int var_max_line_length;
static char *var_myhostname;
static VSTRING *buffer;
static void command_read(int, char *);
static void disconnected(VSTREAM *);
static int count;
static int counter;

/* helo - process HELO/EHLO command */

static void helo(VSTREAM *stream)
{
    smtp_printf(stream, "250-%s", var_myhostname);
    smtp_printf(stream, "250-8BITMIME");
    smtp_printf(stream, "250 PIPELINING");
}

/* ok - send 250 OK */

static void ok(VSTREAM *stream)
{
    smtp_printf(stream, "250 Ok");
}

/* data_read - read data from socket */

static void data_read(int unused_event, char *context)
{
    struct data_state *dstate = (struct data_state *) context;
    VSTREAM *stream = dstate->stream;
    int     avail;
    int     ch;
    struct data_trans {
	int     state;
	int     want;
	int     next_state;
    };
    static struct data_trans data_trans[] = {
	ST_ANY, '\r', ST_CR,
	ST_CR, '\n', ST_CR_LF,
	ST_CR_LF, '.', ST_CR_LF_DOT,
	ST_CR_LF_DOT, '\r', ST_CR_LF_DOT_CR,
	ST_CR_LF_DOT_CR, '\n', ST_CR_LF_DOT_CR_LF,
    };
    struct data_trans *dp;

    avail = peekfd(vstream_fileno(stream));
    while (avail-- > 0) {
	ch = VSTREAM_GETC(stream);
	for (dp = data_trans; dp->state != dstate->state; dp++)
	     /* void */ ;

	/*
	 * Try to match the current character desired by the state machine.
	 * If that fails, try to restart the machine with a match for its
	 * first state.  This covers the case of a CR/LF/CR/LF sequence
	 * (empty line) right before the end of the message data.
	 */
	if (ch == dp->want)
	    dstate->state = dp->next_state;
	else if (ch == data_trans[0].want)
	    dstate->state = data_trans[0].next_state;
	else
	    dstate->state = ST_ANY;
	if (dstate->state == ST_CR_LF_DOT_CR_LF) {
	    if (msg_verbose)
		msg_info(".");
	    smtp_printf(stream, "250 Ok");
	    event_disable_readwrite(vstream_fileno(stream));
	    event_enable_read(vstream_fileno(stream),
			      command_read, (char *) stream);
	    myfree((char *) dstate);
	}
    }
}

/* data - process DATA command */

static void data(VSTREAM *stream)
{
    struct data_state *dstate = (struct data_state *) mymalloc(sizeof(*dstate));

    dstate->stream = stream;
    dstate->state = ST_CR_LF;
    smtp_printf(stream, "354 End data with <CR><LF>.<CR><LF>");
    event_disable_readwrite(vstream_fileno(stream));
    event_enable_read(vstream_fileno(stream),
		      data_read, (char *) dstate);
}

/* quit - process QUIT command */

static void quit(VSTREAM *stream)
{
    smtp_printf(stream, "221 Bye");
    disconnected(stream);
    if (count) {
	counter++;
	vstream_printf("%d\r", counter);
	vstream_fflush(VSTREAM_OUT);
    }
}

 /*
  * The table of all SMTP commands that we can handle.
  */
typedef struct COMMAND {
    char   *name;
    void    (*action) (VSTREAM *);
}       COMMAND;

static COMMAND command_table[] = {
    "helo", helo,
    "ehlo", helo,
    "mail", ok,
    "rcpt", ok,
    "data", data,
    "rset", ok,
    "noop", ok,
    "vrfy", ok,
    "quit", quit,
    0,
};

/* command_read - talk the SMTP protocol, server side */

static void command_read(int unused_event, char *context)
{
    VSTREAM *stream = (VSTREAM *) context;
    char   *command;
    COMMAND *cmdp;

    switch (setjmp(smtp_timeout_buf)) {

    default:
	msg_panic("unknown error reading input");

    case SMTP_ERR_TIME:
	smtp_printf(stream, "421 Error: timeout exceeded");
	msg_warn("timeout reading input");
	disconnected(stream);
	break;

    case SMTP_ERR_EOF:
	msg_warn("lost connection");
	disconnected(stream);
	break;

    case 0:
	smtp_get(buffer, stream, var_max_line_length);
	if ((command = strtok(vstring_str(buffer), " \t")) == 0) {
	    smtp_printf(stream, "500 Error: unknown command");
	    break;
	}
	if (msg_verbose)
	    msg_info("%s", command);
	for (cmdp = command_table; cmdp->name != 0; cmdp++)
	    if (strcasecmp(command, cmdp->name) == 0)
		break;
	if (cmdp->name == 0) {
	    smtp_printf(stream, "500 Error: unknown command");
	    break;
	}
	cmdp->action(stream);
	break;
    }
}

/* disconnected - handle disconnection events */

static void disconnected(VSTREAM *stream)
{
    if (msg_verbose)
	msg_info("disconnect");
    event_disable_readwrite(vstream_fileno(stream));
    vstream_fclose(stream);
}

/* connected - connection established */

static void connected(int unused_event, char *context)
{
    int     sock = (int) context;
    VSTREAM *stream;
    int     fd;

    if ((fd = accept(sock, (struct sockaddr *) 0, (SOCKADDR_SIZE *) 0)) >= 0) {
	if (msg_verbose)
	    msg_info("connect");
	non_blocking(fd, NON_BLOCKING);
	stream = vstream_fdopen(fd, O_RDWR);
	smtp_timeout_setup(stream, var_tmout);
	smtp_printf(stream, "220 %s ESMTP", var_myhostname);
	event_enable_read(fd, command_read, (char *) stream);
    }
}

/* usage - explain */

static void usage(char *myname)
{
    msg_fatal("usage: %s [-c] [-v] [host]:port backlog", myname);
}

int     main(int argc, char **argv)
{
    int     sock;
    int     backlog;
    int     ch;

    /*
     * Initialize diagnostics.
     */
    msg_vstream_init(argv[0], VSTREAM_ERR);

    /*
     * Parse JCL.
     */
    while ((ch = GETOPT(argc, argv, "cv")) > 0) {
	switch (ch) {
	case 'c':
	    count++;
	    break;
	case 'v':
	    msg_verbose++;
	    break;
	default:
	    usage(argv[0]);
	}
    }
    if (argc - optind != 2)
	usage(argv[0]);
    if ((backlog = atoi(argv[optind + 1])) <= 0)
	usage(argv[0]);

    /*
     * Initialize.
     */
    buffer = vstring_alloc(1024);
    var_myhostname = "smtp-sink";
    sock = inet_listen(argv[optind], backlog, BLOCKING);

    /*
     * Start the event handler.
     */
    event_enable_read(sock, connected, (char *) sock);
    for (;;)
	event_loop(-1);
}
