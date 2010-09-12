/*++
/* NAME
/*	postscreen_send 3
/* SUMMARY
/*	postscreen low-level output
/* SYNOPSIS
/*	#include <postscreen.h>
/*
/*	int	ps_send_reply(client_fd, client_addr, client_port, text)
/*	int	client_fd;
/*	const char *client_addr;
/*	const char *client_port;
/*	const char *text;
/*
/*	int	PS_SEND_REPLY(state, text)
/*	const char *text;
/*
/*	void	ps_send_socket(state)
/*	PS_STATE *state;
/* DESCRIPTION
/*	ps_send_reply() sends the specified text to the specified
/*	remote SMTP client.  In case of an immediate error, it logs
/*	a warning (except EPIPE) with the client address and port,
/*	and returns -1 (including EPIPE). Otherwise, the result
/*	value is the number of bytes sent.
/*
/*	PS_SEND_REPLY() is a convenience wrapper for ps_send_reply().
/*	It is an unsafe macro that evaluates its arguments multiple
/*	times.
/*
/*	ps_send_socket() sends the specified socket to the real
/*	Postfix SMTP server. The socket is delivered in the background.
/*	This function must be called after all other session-related
/*	work is finished including postscreen cache updates.
/*
/*	In case of an immediate error, ps_send_socket() sends a 421
/*	reply to the remote SMTP client and closes the connection.
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
#include <string.h>
#include <errno.h>

/* Utility library. */

#include <msg.h>
#include <iostuff.h>
#include <connect.h>

/* Application-specific. */

#include <postscreen.h>

 /*
  * This program screens all inbound SMTP connections, so it better not waste
  * time.
  */
#define PS_SEND_SOCK_CONNECT_TIMEOUT	1
#define PS_SEND_SOCK_NOTIFY_TIMEOUT	100
#define PS_SEND_TEXT_TIMEOUT		1

/* ps_send_reply - send reply to remote SMTP client */

int     ps_send_reply(int smtp_client_fd, const char *smtp_client_addr,
		              const char *smtp_client_port, const char *text)
{
    int     ret;

    if (msg_verbose)
	msg_info("> %s:%s: %.*s", smtp_client_addr, smtp_client_port,
		 (int) strlen(text) - 2, text);

    /*
     * XXX Need to make sure that the TCP send buffer is large enough for any
     * response, so that a nasty client can't cause this process to block.
     */
    ret = (write_buf(smtp_client_fd, text, strlen(text),
		     PS_SEND_TEXT_TIMEOUT) < 0);
    if (ret != 0 && errno != EPIPE)
	msg_warn("write %s:%s: %m", smtp_client_addr, smtp_client_port);
    return (ret);
}

/* ps_send_socket_close_event - file descriptor has arrived or timeout */

static void ps_send_socket_close_event(int event, char *context)
{
    const char *myname = "ps_send_socket_close_event";
    PS_STATE *state = (PS_STATE *) context;

    if (msg_verbose > 1)
	msg_info("%s: sq=%d cq=%d event %d on send socket %d from %s:%s",
		 myname, ps_post_queue_length, ps_check_queue_length,
		 event, state->smtp_server_fd, state->smtp_client_addr,
		 state->smtp_client_port);

    /*
     * The real SMTP server has closed the local IPC channel, or we have
     * reached the limit of our patience. In the latter case it is still
     * possible that the real SMTP server will receive the socket so we
     * should not interfere.
     */
    PS_CLEAR_EVENT_REQUEST(state->smtp_server_fd, ps_send_socket_close_event,
			   context);
    if (event == EVENT_TIME)
	msg_warn("timeout sending connection to service %s",
		 ps_smtpd_service_name);
    ps_free_session_state(state);
}

/* ps_send_socket - send socket to real SMTP server process */

void    ps_send_socket(PS_STATE *state)
{
    const char *myname = "ps_send_socket";
    int     server_fd;
    int     window_size;

    if (msg_verbose > 1)
	msg_info("%s: sq=%d cq=%d send socket %d from %s:%s",
		 myname, ps_post_queue_length, ps_check_queue_length,
		 vstream_fileno(state->smtp_client_stream),
		 state->smtp_client_addr, state->smtp_client_port);

    /*
     * This is where we would adjust the window size to a value that is
     * appropriate for this client class.
     */
#if 0
    window_size = 65535;
    if (setsockopt(vstream_fileno(state->smtp_client_stream), SOL_SOCKET, SO_RCVBUF,
		   (char *) &window_size, sizeof(window_size)) < 0)
	msg_warn("setsockopt SO_RCVBUF %d: %m", window_size);
#endif

    /*
     * Connect to the real SMTP service over a local IPC channel, send the
     * file descriptor, and close the file descriptor to save resources.
     * Experience has shown that some systems will discard information when
     * we close a channel immediately after writing. Thus, we waste resources
     * waiting for the remote side to close the local IPC channel first. The
     * good side of waiting is that we learn when the real SMTP server is
     * falling behind.
     * 
     * This is where we would forward the connection to an SMTP server that
     * provides an appropriate level of service for this client class. For
     * example, a server that is more forgiving, or one that is more
     * suspicious. Alternatively, we could send attributes along with the
     * socket with client reputation information, making everything even more
     * Postfix-specific.
     */
    if ((server_fd =
	 LOCAL_CONNECT(ps_smtpd_service_name, NON_BLOCKING,
		       PS_SEND_SOCK_CONNECT_TIMEOUT)) < 0) {
	msg_warn("cannot connect to service %s: %m", ps_smtpd_service_name);
	ps_send_reply(vstream_fileno(state->smtp_client_stream),
		      state->smtp_client_addr, state->smtp_client_port,
		      "421 4.3.2 All server ports are busy\r\n");
	ps_free_session_state(state);
	return;
    }
    PS_ADD_SERVER_STATE(state, server_fd);
    if (LOCAL_SEND_FD(state->smtp_server_fd,
		      vstream_fileno(state->smtp_client_stream)) < 0) {
	msg_warn("cannot pass connection to service %s: %m",
		 ps_smtpd_service_name);
	ps_send_reply(vstream_fileno(state->smtp_client_stream),
		      state->smtp_client_addr, state->smtp_client_port,
		      "421 4.3.2 No system resources\r\n");
	ps_free_session_state(state);
	return;
    } else {

	/*
	 * Closing the smtp_client_fd here triggers a FreeBSD 7.1 kernel bug
	 * where smtp-source sometimes sees the connection being closed after
	 * it has already received the real SMTP server's 220 greeting!
	 */
#if 0
	PS_DEL_CLIENT_STATE(state);
#endif
	PS_READ_EVENT_REQUEST(state->smtp_server_fd, ps_send_socket_close_event,
			      (char *) state, PS_SEND_SOCK_NOTIFY_TIMEOUT);
	return;
    }
}
