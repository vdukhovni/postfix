/*++
/* NAME
/*	get_unix_peer_creds 3
/* SUMMARY
/*	local IPC peer lookup
/* SYNOPSIS
/*	int	get_unix_peer_creds(int sock, uid_t *uid, gid_t *gid)
/* DESCRIPTION
/*	get_unix_peer_creds() returns the UID and GID of a local IPC
/*	peer process at the point in time that a UNIX-domain server
/*	called listen(2) or that a UNIX-domain client() called
/*	connect(2). The peer UID or GID may have changed after that
/*	point in time.
/*
/*	This function is a wrapper for system-dependent infrastructure,
/*	supporting SO_PEERCRED on Linux >= 2.2; getpeereid() on FreeBSD >=
/*	4.6, NetBSD >= 5.0, OpenBSD >= 3.0, and MacOS >= 10.5; and
/*	getpeerucred() on Solaris >= 10. On other systems the call will
/*	always fail with error code ENOTSUP.
/* DIAGNOSTICS
/*	The result is 0 in case of success, -1 in case of failure with
/*	errno indicating the problem cause.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

 /*
  * System library.
  */
#ifdef USE_SO_PEERCRED
#define _GNU_SOURCE			/* Must be first */
#endif

#include <sys_defs.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>

#ifdef USE_STREAM_UCRED
#include <ucred.h>
#endif

 /*
  * Utility library.
  */
#include <msg.h>
#include <get_unix_peer_creds.h>

/* get_unix_peer_creds - identify a UNIX-domain connection peer */

int get_unix_peer_creds(int sock, uid_t *uid, gid_t *gid)
{
#if defined(USE_SO_PEERCRED)

    /*
     * LINUX >= 2.2 (1999).
     */
    struct ucred peer_creds;
    SOCKOPT_SIZE peer_cr_len = sizeof(peer_creds);
    int     res;

    res = getsockopt(sock, SOL_SOCKET, SO_PEERCRED, &peer_creds, &peer_cr_len);
    if (res == 0) {
	*uid = peer_creds.uid;
	*gid = peer_creds.gid;
    }
    return (res);
#elif defined(USE_GETPEEREID)

    /*
     * FreeBSD >= 4.6 (2002), NetBSD >= 5.0 (2009), OpenBSD >= 3.0 (2001),
     * MacOS >= 10.5 (2007). 
     */
    return (getpeereid(sock, uid, gid));
#elif defined(USE_STREAM_UCRED)

    /*
     * SunOS >= 5.10 (2005). 
     */
    ucred_t *uc = 0;

    res = getpeerucred(sock, &uc);
    if (res == 0) {
	*uid = ucred_geteuid(uc);
	*gid = ucred_getegid(uc);
	ucred_free(uc);
    }
    return (res);
#else
    msg_warn("missing support to get UNIX-domain peer credentials");
    errno = ENOTSUP;
    return (-1);
#endif
}
