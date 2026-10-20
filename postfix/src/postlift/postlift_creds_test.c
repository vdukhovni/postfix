 /*
  * System library.
  */
#include <sys_defs.h>
#include <unistd.h>
#include <string.h>

 /*
  * Global library.
  */
#include <mail_params.h>
#include <mypwd.h>

 /*
  * Test library.
  */
#include <ptest.h>

 /*
  * Application-specific.
  */
#include <postlift_creds.h>

 /*
  * Test case and data.
  */
typedef struct PTEST_CASE {
    const char *testname;
    void    (*action) (PTEST_CTX *t, const struct PTEST_CASE *);
} PTEST_CASE;

 /*
  * Surrogate parameter dependencies. We are not using mail_params_init() to
  * keep tests hermetic.
  */
char   *var_mail_owner;
uid_t   var_owner_uid;
gid_t   var_owner_gid;

#define OTHER_UID	2
#define OTHER_GID	2

#define OWNER_NAME	"postfix"
#define OWNER_UID	3
#define OWNER_GID	3

char   *var_default_privs;
uid_t   var_default_uid;
gid_t   var_default_gid;

#define DEF_PRIVS_NAME	"nobody"
#define DEF_UID		4
#define DEF_GID		4

char   *var_sgid_group;
struct mypasswd fake_pwd;
gid_t   var_sgid_gid;

#define SGID_GROUP	"postdrop"
#define POSTDROP_UID	5
#define POSTDROP_GID	5

int     my_uid;
int     my_gid;

#define MY_UID		POSTDROP_UID
#define MY_GID		POSTDROP_GID

int     my_euid;
int     my_egid;

#define MY_EUID		POSTDROP_UID
#define MY_EGID		POSTDROP_GID

/* setup_test - Initialize the normal case */

static void setup_test(void)
{
    my_uid = MY_UID;
    my_gid = MY_GID;
    my_euid = MY_EUID;
    my_egid = MY_EGID;
    var_mail_owner = OWNER_NAME;
    var_owner_uid = OWNER_UID;
    var_owner_gid = OWNER_GID;
    var_default_privs = DEF_PRIVS_NAME;
    var_default_uid = DEF_UID;
    var_default_gid = DEF_GID;
    var_sgid_group = SGID_GROUP;
    memset(&fake_pwd, 0xff, sizeof(fake_pwd));
    fake_pwd.pw_uid = POSTDROP_UID;
    var_sgid_gid = POSTDROP_GID;
}

 /*
  * Begin surrogate code dependencies.
  */
uid_t   getuid(void)
{
    return (my_uid);
}

uid_t   geteuid(void)
{
    return (my_euid);
}

gid_t   getgid(void)
{
    return (my_gid);
}

gid_t   getegid(void)
{
    return (my_egid);
}

struct mypasswd *mypwnam(const char *name)
{
    if (strcmp(name, SGID_GROUP) == 0)
	return (&fake_pwd);
    ptest_fatal(ptest_ctx_current(), "mypwnam: unexpected name: '%s'", name);
}

 /*
  * End surrogate code dependencies.
  */

 /*
  * Begin tests.
  */

static void normal(PTEST_CTX *t, const struct PTEST_CASE *tp)
{
    setup_test();
    plift_valid_creds_or_die(MY_UID, MY_GID);
}

static void expect_reject_creds(PTEST_CTX *t, uid_t my_uid, gid_t my_gid)
{
    plift_valid_creds_or_die(my_uid, my_gid);
    ptest_fatal(t, "plift_valid_creds_or_die() returned");
}

static void rejects_setuid(PTEST_CTX *t, const struct PTEST_CASE *tp)
{
    setup_test();
    my_euid = 0;
    expect_ptest_log_event(t, "MUST NOT run with set-uid privileges");
    expect_reject_creds(t, MY_UID, MY_GID);
}

static void rejects_other_uid(PTEST_CTX *t, const struct PTEST_CASE *tp)
{
    setup_test();
    expect_ptest_log_event(t, "MUST run with the 'postdrop' user ID (5)");
    expect_reject_creds(t, my_euid = OTHER_UID, MY_GID);
}

static void rejects_privileged_uid(PTEST_CTX *t, const struct PTEST_CASE *tp)
{
    setup_test();
    my_euid = my_uid = fake_pwd.pw_uid = 0;
    expect_ptest_log_event(t, "MUST NOT run with the super-user ID (0)");
    expect_reject_creds(t, my_uid, MY_GID);
}

static void rejects_reserved_uid(PTEST_CTX *t, const struct PTEST_CASE *tp)
{
    setup_test();
    my_euid = my_uid = fake_pwd.pw_uid = (uid_t) -1L;
    expect_ptest_log_event(t, "MUST NOT run with reserved user ID (-1)");
    expect_reject_creds(t, my_uid, MY_GID);
}

static void rejects_owner_uid(PTEST_CTX *t, const struct PTEST_CASE *tp)
{
    setup_test();
    my_euid = my_uid = fake_pwd.pw_uid = var_owner_uid;
    expect_ptest_log_event(t, "MUST NOT run with the 'postfix' user ID (3)");
    expect_reject_creds(t, my_uid, MY_GID);
}

static void rejects_default_uid(PTEST_CTX *t, const struct PTEST_CASE *tp)
{
    setup_test();
    my_euid = my_uid = fake_pwd.pw_uid = var_default_uid;
    expect_ptest_log_event(t, "MUST NOT run with the 'nobody' user ID (4)");
    expect_reject_creds(t, my_uid, MY_GID);
}

static void rejects_setgid(PTEST_CTX *t, const struct PTEST_CASE *tp)
{
    setup_test();
    my_egid = 0;
    expect_ptest_log_event(t, "MUST NOT run with set-gid privileges");
    expect_reject_creds(t, MY_UID, MY_GID);
}

static void rejects_other_gid(PTEST_CTX *t, const struct PTEST_CASE *tp)
{
    setup_test();
    expect_ptest_log_event(t, "MUST run with the 'postdrop' group ID (5)");
    expect_reject_creds(t, MY_UID, my_egid = OTHER_GID);
}

static void rejects_privileged_gid(PTEST_CTX *t, const struct PTEST_CASE *tp)
{
    setup_test();
    my_egid = my_gid = var_sgid_gid = 0;
    expect_ptest_log_event(t, "MUST NOT run with privileged group ID (0)");
    expect_reject_creds(t, MY_UID, my_gid);
}

static void rejects_reserved_gid(PTEST_CTX *t, const struct PTEST_CASE *tp)
{
    setup_test();
    my_egid = my_gid = var_sgid_gid = (gid_t) -1L;
    expect_ptest_log_event(t, "MUST NOT run with reserved group ID (-1)");
    expect_reject_creds(t, MY_UID, my_gid);
}

static void rejects_owner_gid(PTEST_CTX *t, const struct PTEST_CASE *tp)
{
    setup_test();
    my_egid = my_gid = var_sgid_gid = var_owner_gid;
    expect_ptest_log_event(t, "MUST NOT run with the 'postfix' group ID (3)");
    expect_reject_creds(t, MY_UID, my_gid);
}

static void rejects_default_gid(PTEST_CTX *t, const struct PTEST_CASE *tp)
{
    setup_test();
    my_egid = my_gid = var_sgid_gid = var_default_gid;
    expect_ptest_log_event(t, "MUST NOT run with the 'nobody' group ID (4)");
    expect_reject_creds(t, MY_UID, my_gid);
}

 /*
  * End tests.
  */

 /*
  * Test cases.
  */
const PTEST_CASE ptestcases[] = {
    {
	.testname = "normal case",
	.action = normal,
    },

    /*
     * Sanity checks: UID.
     */
    {
	.testname = "rejects setuid",
	.action = rejects_setuid,
    }, {
	.testname = "rejects other uid",
	.action = rejects_other_uid,
    },

    /*
     * Don't allow UID sharing with other accounts.
     */
    {
	.testname = "rejects privileged_uid",
	.action = rejects_privileged_uid,
    }, {
	.testname = "rejects uid -1",
	.action = rejects_reserved_uid,
    }, {
	.testname = "rejects owner uid",
	.action = rejects_owner_uid,
    }, {
	.testname = "rejects default_privs uid",
	.action = rejects_default_uid,
    },

    /*
     * Sanity checks: GID.
     */
    {
	.testname = "rejects setgid",
	.action = rejects_setgid,
    }, {
	.testname = "rejects other gid",
	.action = rejects_other_gid,
    },

    /*
     * Don't allow GID sharing with other accounts.
     */
    {
	.testname = "rejects privileged_gid",
	.action = rejects_privileged_gid,
    }, {
	.testname = "rejects gid -1",
	.action = rejects_reserved_gid,
    }, {
	.testname = "rejects owner gid",
	.action = rejects_owner_gid,
    }, {
	.testname = "rejects default_privs gid",
	.action = rejects_default_gid,
    }
};

#include <ptest_main.h>
