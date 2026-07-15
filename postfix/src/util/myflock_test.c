 /*
  * Test program to verify that myflock() makes the expected flock() and
  * fcntl() calls.
  */

 /*
  * System library.
  */
#include <sys_defs.h>
#include <errno.h>
#include <string.h>

 /*
  * Utility library.
  */
#include <wrap_fcntl.h>
#include <myflock.h>

 /*
  * Test library.
  */
#include <ptest.h>
#include <mock_fcntl.h>

typedef struct PTEST_CASE {
    const char *testname;		/* identifies test case */
    void    (*action) (PTEST_CTX *t,
		               const struct PTEST_CASE *tp);
} PTEST_CASE;

static void test_myflock_fcntl_shared_nb(PTEST_CTX *t,
					         const struct PTEST_CASE *tp)
{
    MOCK_FCNTL_REQ lock_req;
    MOCK_FCNTL_REQ unlock_req;
    int     ret;

    ptest_info(t, "acquire lock");
    memset((void *) &lock_req, 0, sizeof(lock_req));
    lock_req.want_fd = 0;
    lock_req.want_cmd = F_SETLK;
    lock_req.want_arg.fl.l_type = F_RDLCK;
    setup_mock_fcntl(&lock_req);
    ret = myflock(0, MYFLOCK_STYLE_FCNTL,
		  MYFLOCK_OP_SHARED | MYFLOCK_OP_NOWAIT);
    if (ret != lock_req.out_ret)
	ptest_fatal(t, "myflock(fd, MYFLOCK_STYLE_FCNTL, "
		    "MYFLOCK_OP_SHARED|MYFLOCK_OP_NOWAIT): got %d, want %d",
		    ret, lock_req.out_ret);

    ptest_info(t, "release lock");
    memset((void *) &unlock_req, 0, sizeof(lock_req));
    unlock_req.want_fd = 0;
    unlock_req.want_cmd = F_SETLKW;
    unlock_req.want_arg.fl.l_type = F_UNLCK;
    setup_mock_fcntl(&unlock_req);
    ret = myflock(0, MYFLOCK_STYLE_FCNTL, MYFLOCK_OP_NONE);
    if (ret != unlock_req.out_ret)
	ptest_fatal(t, "myflock(fd, MYFLOCK_STYLE_FCNTL, "
		    "MYFLOCK_OP_NONE): got %d, want %d",
		    ret, unlock_req.out_ret);

    teardown_mock_fcntl();
}

static void test_myflock_fcntl_shared_blk(PTEST_CTX *t,
					        const struct PTEST_CASE *tp)
{
    MOCK_FCNTL_REQ lock_req;
    MOCK_FCNTL_REQ unlock_req;
    int     ret;

    ptest_info(t, "acquire lock");
    memset((void *) &lock_req, 0, sizeof(lock_req));
    lock_req.want_fd = 0;
    lock_req.want_cmd = F_SETLKW;
    lock_req.want_arg.fl.l_type = F_RDLCK;
    setup_mock_fcntl(&lock_req);
    ret = myflock(0, MYFLOCK_STYLE_FCNTL, MYFLOCK_OP_SHARED);
    if (ret != lock_req.out_ret)
	ptest_fatal(t, "myflock(fd, MYFLOCK_STYLE_FCNTL, "
		    "MYFLOCK_OP_SHARED): got %d, want %d",
		    ret, lock_req.out_ret);

    ptest_info(t, "release lock");
    memset((void *) &unlock_req, 0, sizeof(lock_req));
    unlock_req.want_fd = 0;
    unlock_req.want_cmd = F_SETLKW;
    unlock_req.want_arg.fl.l_type = F_UNLCK;
    setup_mock_fcntl(&unlock_req);
    ret = myflock(0, MYFLOCK_STYLE_FCNTL, MYFLOCK_OP_NONE);
    if (ret != unlock_req.out_ret)
	ptest_fatal(t, "myflock(fd, MYFLOCK_STYLE_FCNTL, "
		    "MYFLOCK_OP_NONE): got %d, want %d",
		    ret, unlock_req.out_ret);

    teardown_mock_fcntl();
}

static void test_myflock_fcntl_excl_nb(PTEST_CTX *t,
				               const struct PTEST_CASE *tp)
{
    MOCK_FCNTL_REQ lock_req;
    MOCK_FCNTL_REQ unlock_req;
    int     ret;

    ptest_info(t, "acquire lock");
    memset((void *) &lock_req, 0, sizeof(lock_req));
    lock_req.want_fd = 0;
    lock_req.want_cmd = F_SETLK;
    lock_req.want_arg.fl.l_type = F_WRLCK;
    setup_mock_fcntl(&lock_req);
    ret = myflock(0, MYFLOCK_STYLE_FCNTL,
		  MYFLOCK_OP_EXCLUSIVE | MYFLOCK_OP_NOWAIT);
    if (ret != lock_req.out_ret)
	ptest_fatal(t, "myflock(fd, MYFLOCK_STYLE_FCNTL, "
		 "MYFLOCK_OP_EXCLUSIVE|MYFLOCK_OP_NOWAIT): got %d, want %d",
		    ret, lock_req.out_ret);

    ptest_info(t, "release lock");
    memset((void *) &unlock_req, 0, sizeof(lock_req));
    unlock_req.want_fd = 0;
    unlock_req.want_cmd = F_SETLKW;
    unlock_req.want_arg.fl.l_type = F_UNLCK;
    setup_mock_fcntl(&unlock_req);
    ret = myflock(0, MYFLOCK_STYLE_FCNTL, MYFLOCK_OP_NONE);
    if (ret != unlock_req.out_ret)
	ptest_fatal(t, "myflock(fd, MYFLOCK_STYLE_FCNTL, "
		    "MYFLOCK_OP_NONE): got %d, want %d",
		    ret, unlock_req.out_ret);
}

static void test_myflock_fcntl_excl_blk(PTEST_CTX *t,
					        const struct PTEST_CASE *tp)
{
    MOCK_FCNTL_REQ lock_req;
    MOCK_FCNTL_REQ unlock_req;
    int     ret;

    ptest_info(t, "acquire lock");
    memset((void *) &lock_req, 0, sizeof(lock_req));
    lock_req.want_fd = 0;
    lock_req.want_cmd = F_SETLKW;
    lock_req.want_arg.fl.l_type = F_WRLCK;
    setup_mock_fcntl(&lock_req);
    ret = myflock(0, MYFLOCK_STYLE_FCNTL, MYFLOCK_OP_EXCLUSIVE);
    if (ret != lock_req.out_ret)
	ptest_fatal(t, "myflock(fd, MYFLOCK_STYLE_FCNTL, "
		    "MYFLOCK_OP_EXCLUSIVE): got %d, want %d",
		    ret, lock_req.out_ret);

    ptest_info(t, "release lock");
    memset((void *) &unlock_req, 0, sizeof(lock_req));
    unlock_req.want_fd = 0;
    unlock_req.want_cmd = F_SETLKW;
    unlock_req.want_arg.fl.l_type = F_UNLCK;
    setup_mock_fcntl(&unlock_req);
    ret = myflock(0, MYFLOCK_STYLE_FCNTL, MYFLOCK_OP_NONE);
    if (ret != unlock_req.out_ret)
	ptest_fatal(t, "myflock(fd, MYFLOCK_STYLE_FCNTL, "
		    "MYFLOCK_OP_NONE): got %d, want %d",
		    ret, unlock_req.out_ret);

    teardown_mock_fcntl();
}

static void test_myflock_flock_shared_nb(PTEST_CTX *t,
					         const struct PTEST_CASE *tp)
{
    MOCK_FLOCK_REQ lock_req;
    MOCK_FLOCK_REQ unlock_req;
    int     ret;

    ptest_info(t, "acquire lock");
    memset((void *) &lock_req, 0, sizeof(lock_req));
    lock_req.want_fd = 0;
    lock_req.want_cmd = LOCK_SH | LOCK_NB;
    setup_mock_flock(&lock_req);
    ret = myflock(0, MYFLOCK_STYLE_FLOCK,
		  MYFLOCK_OP_SHARED | MYFLOCK_OP_NOWAIT);
    if (ret != lock_req.out_ret)
	ptest_fatal(t, "myflock(fd, MYFLOCK_STYLE_FLOCK, MYFLOCK_OP_SHARED"
		    "|MYFLOCK_OP_NOWAIT): got %d, want %d",
		    ret, lock_req.out_ret);

    ptest_info(t, "release lock");
    memset((void *) &unlock_req, 0, sizeof(lock_req));
    unlock_req.want_fd = 0;
    unlock_req.want_cmd = LOCK_UN;
    setup_mock_flock(&unlock_req);
    ret = myflock(0, MYFLOCK_STYLE_FLOCK, MYFLOCK_OP_NONE);
    if (ret != unlock_req.out_ret)
	ptest_fatal(t, "myflock(fd, MYFLOCK_STYLE_FLOCK, "
		    "MYFLOCK_OP_NONE): got %d, want %d",
		    ret, unlock_req.out_ret);

    teardown_mock_flock();
}

static void test_myflock_flock_shared_blk(PTEST_CTX *t,
					        const struct PTEST_CASE *tp)
{
    MOCK_FLOCK_REQ lock_req;
    MOCK_FLOCK_REQ unlock_req;
    int     ret;

    ptest_info(t, "acquire lock");
    memset((void *) &lock_req, 0, sizeof(lock_req));
    lock_req.want_fd = 0;
    lock_req.want_cmd = LOCK_SH;
    setup_mock_flock(&lock_req);
    ret = myflock(0, MYFLOCK_STYLE_FLOCK, MYFLOCK_OP_SHARED);
    if (ret != lock_req.out_ret)
	ptest_fatal(t, "myflock(fd, MYFLOCK_STYLE_FLOCK, "
		    "MYFLOCK_OP_SHARED): got %d, want %d",
		    ret, lock_req.out_ret);

    ptest_info(t, "release lock");
    memset((void *) &unlock_req, 0, sizeof(lock_req));
    unlock_req.want_fd = 0;
    unlock_req.want_cmd = LOCK_UN;
    setup_mock_flock(&unlock_req);
    ret = myflock(0, MYFLOCK_STYLE_FLOCK, MYFLOCK_OP_NONE);
    if (ret != unlock_req.out_ret)
	ptest_fatal(t, "myflock(fd, MYFLOCK_STYLE_FLOCK, "
		    "MYFLOCK_OP_NONE): got %d, want %d",
		    ret, unlock_req.out_ret);

    teardown_mock_flock();
}

static void test_myflock_flock_excl_nb(PTEST_CTX *t,
				               const struct PTEST_CASE *tp)
{
    MOCK_FLOCK_REQ lock_req;
    MOCK_FLOCK_REQ unlock_req;
    int     ret;

    ptest_info(t, "acquire lock");
    memset((void *) &lock_req, 0, sizeof(lock_req));
    lock_req.want_fd = 0;
    lock_req.want_cmd = LOCK_EX | LOCK_NB;
    setup_mock_flock(&lock_req);
    ret = myflock(0, MYFLOCK_STYLE_FLOCK,
		  MYFLOCK_OP_EXCLUSIVE | MYFLOCK_OP_NOWAIT);
    if (ret != lock_req.out_ret)
	ptest_fatal(t, "myflock(fd, MYFLOCK_STYLE_FLOCK, MYFLOCK_OP_EXCLUSIVE"
		    "|MYFLOCK_OP_NOWAIT): got %d, want %d",
		    ret, lock_req.out_ret);

    ptest_info(t, "release lock");
    memset((void *) &unlock_req, 0, sizeof(lock_req));
    unlock_req.want_fd = 0;
    unlock_req.want_cmd = LOCK_UN;
    setup_mock_flock(&unlock_req);
    ret = myflock(0, MYFLOCK_STYLE_FLOCK, MYFLOCK_OP_NONE);
    if (ret != unlock_req.out_ret)
	ptest_fatal(t, "myflock(fd, MYFLOCK_STYLE_FLOCK, "
		    "MYFLOCK_OP_NONE): got %d, want %d",
		    ret, unlock_req.out_ret);

    teardown_mock_flock();
}

static void test_myflock_flock_excl_blk(PTEST_CTX *t,
					        const struct PTEST_CASE *tp)
{
    MOCK_FLOCK_REQ lock_req;
    MOCK_FLOCK_REQ unlock_req;
    int     ret;

    ptest_info(t, "acquire lock");
    memset((void *) &lock_req, 0, sizeof(lock_req));
    lock_req.want_fd = 0;
    lock_req.want_cmd = LOCK_EX;
    setup_mock_flock(&lock_req);
    ret = myflock(0, MYFLOCK_STYLE_FLOCK, MYFLOCK_OP_EXCLUSIVE);
    if (ret != lock_req.out_ret)
	ptest_fatal(t, "myflock(fd, MYFLOCK_STYLE_FLOCK, "
		    "MYFLOCK_OP_EXCLUSIVE): got %d, want %d",
		    ret, lock_req.out_ret);

    ptest_info(t, "release lock");
    memset((void *) &unlock_req, 0, sizeof(lock_req));
    unlock_req.want_fd = 0;
    unlock_req.want_cmd = LOCK_UN;
    setup_mock_flock(&unlock_req);
    ret = myflock(0, MYFLOCK_STYLE_FLOCK, MYFLOCK_OP_NONE);
    if (ret != unlock_req.out_ret)
	ptest_fatal(t, "myflock(fd, MYFLOCK_STYLE_FLOCK, "
		    "MYFLOCK_OP_NONE): got %d, want %d",
		    ret, unlock_req.out_ret);

    teardown_mock_flock();
}

static void test_myflock_bad_style_0(PTEST_CTX *t, const struct PTEST_CASE *tp)
{
    expect_ptest_log_event(t, "panic: myflock: unsupported lock style: 0x0");
    switch (myflock(0, MYFLOCK_STYLE_FLOCK - 1, -1)) {
    };
}

static void test_myflock_bad_style_3(PTEST_CTX *t, const struct PTEST_CASE *tp)
{
    expect_ptest_log_event(t, "panic: myflock: unsupported lock style: 0x3");
    switch (myflock(0, MYFLOCK_STYLE_FCNTL + 1, -1)) {
    };
}

static void test_myflock_bad_operation_1(PTEST_CTX *t,
					         const struct PTEST_CASE *tp)
{
    expect_ptest_log_event(t, "panic: myflock: improper operation type: 0xffffffff");
    switch (myflock(0, MYFLOCK_STYLE_FCNTL, -1)) {
    };
}

static void test_myflock_bad_operation_3(PTEST_CTX *t,
					         const struct PTEST_CASE *tp)
{
    expect_ptest_log_event(t, "panic: myflock: improper operation type: 0x3");
    switch (myflock(0, MYFLOCK_STYLE_FCNTL, 3)) {
    };
}

static void test_myflock_propagates_fcntl_error(PTEST_CTX *t,
					        const struct PTEST_CASE *tp)
{
    MOCK_FCNTL_REQ lock_req;
    int     fd = -1;
    int     ret;

    memset((void *) &lock_req, 0, sizeof(lock_req));
    lock_req.want_fd = fd;
    lock_req.want_cmd = F_SETLKW;
    lock_req.want_arg.fl.l_type = F_WRLCK;
    setup_mock_fcntl(&lock_req);
    lock_req.out_ret = -1;
    lock_req.out_errno = EBADF;
    setup_mock_fcntl(&lock_req);
    ret = myflock(fd, MYFLOCK_STYLE_FCNTL, MYFLOCK_OP_EXCLUSIVE);
    if (ret != lock_req.out_ret)
	ptest_error(t, "myflock(-1, MYFLOCK_STYLE_FLOCK, "
		    "MYFLOCK_OP_EXCLUSIVE): got %d, want %d",
		    ret, lock_req.out_ret);
    if (errno != lock_req.out_errno)
	ptest_error(t, "errno: got %d, want %d", errno, lock_req.out_errno);

    teardown_mock_fcntl();
}

static void test_myflock_propagates_flock_error(PTEST_CTX *t,
					        const struct PTEST_CASE *tp)
{
    MOCK_FLOCK_REQ lock_req;
    int     fd = -1;
    int     ret;

    memset((void *) &lock_req, 0, sizeof(lock_req));
    lock_req.want_fd = fd;
    lock_req.want_cmd = LOCK_EX;
    lock_req.out_ret = -1;
    lock_req.out_errno = EBADF;
    setup_mock_flock(&lock_req);
    ret = myflock(fd, MYFLOCK_STYLE_FLOCK, MYFLOCK_OP_EXCLUSIVE);
    if (ret != lock_req.out_ret)
	ptest_error(t, "myflock(-1, MYFLOCK_STYLE_FLOCK, "
		    "MYFLOCK_OP_EXCLUSIVE): got %d, want %d",
		    ret, lock_req.out_ret);
    if (errno != lock_req.out_errno)
	ptest_error(t, "errno: got %d, want %d", errno, lock_req.out_errno);

    teardown_mock_flock();
}

 /*
  * The test cases.
  */
static const PTEST_CASE ptestcases[] = {
    {
	.testname = "fcntl style, shared, non-blocking",
	.action = test_myflock_fcntl_shared_nb,
    },
    {
	.testname = "fcntl style, shared, blocking",
	.action = test_myflock_fcntl_shared_blk,
    },
    {
	.testname = "fcntl style, exclusive, non-blocking",
	.action = test_myflock_fcntl_excl_nb,
    },
    {
	.testname = "fcntl style, exclusive, blocking",
	.action = test_myflock_fcntl_excl_blk,
    },
    {
	.testname = "flock style, shared, non-blocking",
	.action = test_myflock_flock_shared_nb,
    },
    {
	.testname = "flock style, shared, blocking",
	.action = test_myflock_flock_shared_blk,
    },
    {
	.testname = "flock style, exclusive, non-blocking",
	.action = test_myflock_flock_excl_nb,
    },
    {
	.testname = "flock style, exclusive, blocking",
	.action = test_myflock_flock_excl_blk,
    },
    {
	.testname = "detects bad lock style 0",
	.action = test_myflock_bad_style_0,
    },
    {
	.testname = "detects bad lock style 3",
	.action = test_myflock_bad_style_3,
    },
    {
	.testname = "detects bad lock operation -1",
	.action = test_myflock_bad_operation_1,
    },
    {
	.testname = "detects bad lock operation 3",
	.action = test_myflock_bad_operation_3,
    },
    {
	.testname = "propagates fcntl error",
	.action = test_myflock_propagates_fcntl_error,
    },
    {
	.testname = "propagates flock error",
	.action = test_myflock_propagates_flock_error,
    },
};

#include <ptest_main.h>
