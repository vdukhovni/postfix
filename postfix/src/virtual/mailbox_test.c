 /*
  * System library.
  */
#include <sys_defs.h>
#include <string.h>

 /*
  * Global library.
  */
#include <bounce.h>
#include <defer.h>
#include <deliver_request.h>
#include <mail_params.h>
#include <maps.h>

 /*
  * Application-specific.
  */
#include <virtual.h>

 /*
  * Test library.
  */
#include <ptest.h>

 /*
  * Test case and data.
  */
typedef struct PTEST_CASE {
    char *testname;
    void    (*action) (PTEST_CTX *t, const struct PTEST_CASE *);
    char *user;
    int     minimum_uid;
    char *mailbox_maps;
    char *uid_maps;
    char *gid_maps;
    char *mailbox_base;
    char *mailbox_lock;
    char **want_log;
    int     want_status;
    int     want_known;
} PTEST_CASE;

#define STATUS_FINAL	0
#define STATUS_DEFER	1
#define STATUS_UNSET	-1

#define USER_UNKNOWN	0
#define USER_KNOWN	1
 /*
  * Surriogate parameter dependencies.
  */
int     var_virt_minimum_uid;
char   *var_virt_mailbox_maps;
char   *var_virt_uid_maps;
char   *var_virt_gid_maps;
char   *var_virt_mailbox_base;
char   *var_virt_mailbox_lock;
bool    var_strict_mbox_owner;
int     virtual_mbox_lock_mask;
char   *var_rcpt_delim = "+";

 /*
  * Data dependencies.
  */
MAPS   *virtual_mailbox_maps;
MAPS   *virtual_uid_maps;
MAPS   *virtual_gid_maps;

static LOCAL_STATE state;
static USER_ATTR usr_attr;
static DELIVER_REQUEST request;

 /*
  * Surrogate code dependencies.
  * 
  * deliver_mailbox() will call deliver_mailbox_file() in the same file which
  * has too many dependencies. Instead we trigger maildir-style delivery and
  * use a fake deliver_maildir() to verify some of the arguments.
  * 
  * defer_append() has too many dependencies. Instead we use a fake
  * defer_append() to verify some of the arguments.
  */
int     deliver_maildir(LOCAL_STATE state, USER_ATTR user_attr)
{
    msg_info("fake deliver_maildir: mailbox='%s', uid=%lu, gid=%lu",
	     user_attr.mailbox, (unsigned long) user_attr.uid,
	     (unsigned long) user_attr.gid);
    return (0);
}

int     defer_append(int flags, const char *id, MSG_STATS *stats,
		             RECIPIENT *rcpt, const char *relay,
		             const POL_STATS *tstats, DSN *dsn)
{
    msg_info("fake defer_append: dsn=%s reason=%s", dsn->status, dsn->reason);
    return (1);
}

static void teardown_test(void)
{
    if (virtual_mailbox_maps) {
	maps_free(virtual_mailbox_maps);
	virtual_mailbox_maps = 0;
    }
    if (virtual_uid_maps) {
	maps_free(virtual_uid_maps);
	virtual_uid_maps = 0;
    }
    if (virtual_gid_maps) {
	maps_free(virtual_gid_maps);
	virtual_gid_maps = 0;
    }
    if (state.msg_attr.why) {
	dsb_free(state.msg_attr.why);
	state.msg_attr.why = 0;
    }
}

static void setup_test(const PTEST_CASE *tp)
{
    /* In case a previous test failed. */
    teardown_test();

    /*
     * Set parameters so that warning messages log as expected.
     */
    var_virt_minimum_uid = tp->minimum_uid;
    var_virt_mailbox_maps = tp->mailbox_maps;
    var_virt_uid_maps = tp->uid_maps;
    var_virt_gid_maps = tp->gid_maps;
    var_virt_mailbox_base = tp->mailbox_base;
    var_virt_mailbox_lock = tp->mailbox_lock;

    /*
     * Open databases.
     */
    if (var_virt_mailbox_maps)
	virtual_mailbox_maps =
	    maps_create(VAR_VIRT_MAILBOX_MAPS, var_virt_mailbox_maps,
			DICT_FLAG_LOCK | DICT_FLAG_PARANOID
			| DICT_FLAG_UTF8_REQUEST);
    if (var_virt_uid_maps)
	virtual_uid_maps =
	    maps_create(VAR_VIRT_UID_MAPS, var_virt_uid_maps,
			DICT_FLAG_LOCK | DICT_FLAG_PARANOID
			| DICT_FLAG_UTF8_REQUEST);
    if (var_virt_gid_maps)
	virtual_gid_maps =
	    maps_create(VAR_VIRT_GID_MAPS, var_virt_gid_maps,
			DICT_FLAG_LOCK | DICT_FLAG_PARANOID
			| DICT_FLAG_UTF8_REQUEST);

    /*
     * Initialize local state, request, and user attributes.
     */
    state.msg_attr.why = dsb_create();;
    state.msg_attr.user = tp->user;
    state.msg_attr.rcpt.address = tp->user;
    state.msg_attr.delivered = tp->user;
    request.flags = 0;
    state.request = &request;
}

static void test_deliver_mailbox(PTEST_CTX *t, const struct PTEST_CASE *tp)
{
    int     got_status = STATUS_UNSET;
    int     got_known;
    char  **cpp;

    if (sizeof(uid_t) > 4) {
	ptest_info(t, "uid_t is too large -- skipping this test");
	ptest_skip(t);
    }
    if (sizeof(long) <= 4) {
	ptest_info(t, "long is too small -- skipping this test");
	ptest_skip(t);
    }
    setup_test(tp);
    if (tp->want_log)
	for (cpp = (char **) tp->want_log; *cpp; cpp++)
	    expect_ptest_log_event(t, *cpp);
    got_known = deliver_mailbox(state, usr_attr, &got_status);
    if (got_known != tp->want_known)
	ptest_error(t, "user known: got %d, want %d",
		    got_known, tp->want_known);
    if (got_status != tp->want_status)
	ptest_error(t, "user status: got %d, want %d",
		    got_status, tp->want_status);
    teardown_test();
}

 /*
  * Test cases.
  */
const PTEST_CASE ptestcases[] = {
    {
	.testname = "normal case",
	.action = test_deliver_mailbox,
	.user = "example-user",
	.minimum_uid = 1,
	.mailbox_maps = "static:user-1-1/",
	.uid_maps = "static:1",
	.gid_maps = "static:1",
	.mailbox_base = "/base",
	.want_log = (char *[]) {"mailbox='/base/user-1-1/', uid=1, gid=1", 0,},
	.want_known = USER_KNOWN,
	.want_status = USER_UNKNOWN,
    },{
	.testname = "relative base",
	.action = test_deliver_mailbox,
	.user = "example-user",
	.mailbox_base = "base",
	.want_log = (char *[]) {"do not specify relative pathname", 0,},
	.want_known = USER_UNKNOWN,
	.want_status = STATUS_UNSET,
    },{
	.testname = "user unknown",
	.action = test_deliver_mailbox,
	.user = "example-user",
	.mailbox_maps = "inline:{x=x}",
	.mailbox_base = "/base",
	.want_known = USER_UNKNOWN,
	.want_status = STATUS_UNSET,
    },{
	.testname = "trailing .. in path",
	.action = test_deliver_mailbox,
	.user = "example-user",
	.mailbox_maps = "static:here/../",
	.mailbox_base = "/base",
	.want_log = (char *[]) {
	    "recipient example-user: bad mailbox path here/../",
	    "dsn=4.3.5 reason=mail system configuration error",
	    0,
	},
	.want_known = USER_KNOWN,
	.want_status = STATUS_DEFER,
    },{
	.testname = "leading .. in path",
	.action = test_deliver_mailbox,
	.user = "example-user",
	.mailbox_maps = "static:../here/",
	.mailbox_base = "/base",
	.want_log = (char *[]) {
	    "recipient example-user: bad mailbox path ../here/",
	    "dsn=4.3.5 reason=mail system configuration error",
	    0,
	},
	.want_known = USER_KNOWN,
	.want_status = STATUS_DEFER,
    },{
	.testname = "missing entry in virtual_gid_maps",
	.action = test_deliver_mailbox,
	.user = "example-user",
	.minimum_uid = 1,
	.mailbox_maps = "static:user-1-1/",
	.uid_maps = "static:1",
	.gid_maps = "inline:{x=x}",
	.mailbox_base = "/base",
	.want_log = (char *[]) {
	    "recipient example-user: not found in virtual_gid_maps", 
	    "dsn=4.3.5 reason=mail system configuration error",
	    0,
	},
	.want_known = USER_KNOWN,
	.want_status = STATUS_DEFER,
    },{
	.testname = "missing entry in virtual_uid_maps",
	.action = test_deliver_mailbox,
	.user = "example-user",
	.minimum_uid = 1,
	.mailbox_maps = "static:user-1-1/",
	.uid_maps = "inline:{x=x}",
	.mailbox_base = "/base",
	.want_log = (char *[]) {
	    "recipient example-user: not found in virtual_uid_maps", 
	    "dsn=4.3.5 reason=mail system configuration error",
	    0,
	},
	.want_known = USER_KNOWN,
	.want_status = STATUS_DEFER,
    },{
	.testname = "gid too large",
	.action = test_deliver_mailbox,
	.user = "example-user",
	.minimum_uid = 1,
	.mailbox_maps = "static:user-1-1/",
	.uid_maps = "static:1",
	.gid_maps = "static:9223372036854775807L",
	.mailbox_base = "/base",
	.want_log = (char *[]) {
	    "bad gid 9223372036854775807L in virtual_gid_maps", 
	    "dsn=4.3.5 reason=mail system configuration error",
	    0,
	},
	.want_known = USER_KNOWN,
	.want_status = STATUS_DEFER,
    },{
	.testname = "uid too large",
	.action = test_deliver_mailbox,
	.user = "example-user",
	.minimum_uid = 1,
	.mailbox_maps = "static:user-1-1/",
	.uid_maps = "static:9223372036854775807L",
	.gid_maps = "static:1",
	.mailbox_base = "/base",
	.want_log = (char *[]) {
	    "bad uid 9223372036854775807L in virtual_uid_maps", 
	    "dsn=4.3.5 reason=mail system configuration error",
	    0,
	},
	.want_known = USER_KNOWN,
	.want_status = STATUS_DEFER,
    },{
	.testname = "bad uid conversion",
	.action = test_deliver_mailbox,
	.user = "example-user",
	.minimum_uid = 1,
	.mailbox_maps = "static:user-1-1/",
	.uid_maps = "static:9223372X36854775807L",
	.gid_maps = "static:1",
	.mailbox_base = "/base",
	.want_log = (char *[]) {
	    "bad uid 9223372X36854775807L in virtual_uid_maps", 
	    "dsn=4.3.5 reason=mail system configuration error",
	    0,
	},
	.want_known = USER_KNOWN,
	.want_status = STATUS_DEFER,
    },{
	.testname = "bad gid conversion",
	.action = test_deliver_mailbox,
	.user = "example-user",
	.minimum_uid = 1,
	.mailbox_maps = "static:user-1-1/",
	.uid_maps = "static:1",
	.gid_maps = "static:9223372X36854775807L",
	.mailbox_base = "/base",
	.want_log = (char *[]) {
	    "bad gid 9223372X36854775807L in virtual_gid_maps", 
	    "dsn=4.3.5 reason=mail system configuration error",
	    0,
	},
	.want_known = USER_KNOWN,
	.want_status = STATUS_DEFER,
    }, {
	.testname = "uid too small",
	.action = test_deliver_mailbox,
	.user = "example-user",
	.minimum_uid = 1,
	.mailbox_maps = "static:user-1-1/",
	.uid_maps = "static:0",
	.gid_maps = "static:1",
	.mailbox_base = "/base",
	.want_log = (char *[]) {
	    "bad uid 0 in virtual_uid_maps", 
	    "dsn=4.3.5 reason=mail system configuration error",
	    0,
	},
	.want_known = USER_KNOWN,
	.want_status = STATUS_DEFER,
    },
    /* TODO(wietse) database error returns. */
};

#include <ptest_main.h>
