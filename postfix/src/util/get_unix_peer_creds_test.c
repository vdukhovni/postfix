 /*
  * Test program to exercise get_unix_peer_creds.c. See PTEST_README for
  * documentation.
  */

 /*
  * System library.
  */
#include <sys_defs.h>

 /*
  * Utility library.
  */
#include <connect.h>
#include <listen.h>
#include <get_unix_peer_creds.h>

 /*
  * Test library.
  */
#include <ptest.h>

typedef struct PTEST_CASE {
    const char *testname;
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
} PTEST_CASE;

static void test_get_unix_peer_creds(PTEST_CTX *t, const PTEST_CASE *tp)
{
#if defined(USE_SO_PEERCRED) || defined(USE_GETPEEREID) \
	|| defined(USE_STREAM_UCRED)
    const char *path = "creds-test";
    int     backlog = 2;
    int     block_mode = BLOCKING;
    int     timeout = 2;
    uid_t   got_uid, want_uid = geteuid();
    gid_t   got_gid, want_gid = getegid();
    int     listen_fd;
    int     connect_fd;
    int     accept_fd;
    int     res;

    /*
     * Setup.
     */
    (void) unlink(path);
    if ((listen_fd = unix_listen(path, backlog, block_mode)) < 0)
	ptest_fatal(t, "unix_listen %s: %m", path);
    if ((connect_fd = unix_connect(path, block_mode, timeout)) < 0)
	ptest_fatal(t, "unix_connect %s: %m", path);
    if ((accept_fd = unix_accept(listen_fd)) < 0)
	ptest_fatal(t, "unix_accept: %m");;
    res = get_unix_peer_creds(connect_fd, &got_uid, &got_gid);

    /*
     * Evaluate results.
     */
    if (res < 0)
	ptest_fatal(t, "get_unix_peer_creds: %m");
    if (got_uid != want_uid)
	ptest_error(t, "got uid=%lu, want: %lu",
		    (unsigned long) got_uid, (unsigned long) want_uid);
    if (got_gid != want_gid)
	ptest_error(t, "got gid=%lu, want: %lu",
		    (unsigned long) got_gid, (unsigned long) want_gid);

    /*
     * Cleanup.
     */
    close(listen_fd);
    close(connect_fd);
    close(accept_fd);
    (void) unlink(path);
#else
    msg_info("unsupported platform");
    ptest_skip(t);
#endif
}

static const PTEST_CASE ptestcases[] = {
    {"test get_unix_peer_creds", test_get_unix_peer_creds,},
};

#include <ptest_main.h>
