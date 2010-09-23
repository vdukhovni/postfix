/*++
/* NAME
/*	postscreen 3h
/* SUMMARY
/*	postscreen internal interfaces
/* SYNOPSIS
/*	#include <postscreen.h>
/* DESCRIPTION
/* .nf

 /*
  * System library.
  */

 /*
  * Utility library.
  */
#include <dict_cache.h>
#include <vstream.h>
#include <vstring.h>
#include <events.h>

 /*
  * Global library.
  */
#include <addr_match_list.h>
#include <string_list.h>

 /*
  * Preliminary stuff, to be fixed.
  */
#define PS_READ_BUF_SIZE	1024

 /*
  * Per-session state.
  */
typedef struct {
    int     flags;			/* see below */
    /* Socket state. */
    VSTREAM *smtp_client_stream;	/* remote SMTP client */
    int     smtp_server_fd;		/* real SMTP server */
    char   *smtp_client_addr;		/* client address */
    char   *smtp_client_port;		/* client port */
    const char *final_reply;		/* cause for hanging up */
    /* Test context. */
    struct timeval start_time;		/* start of current test */
    const char *test_name;		/* name of current test */
    /* Before-handshake tests. */
    time_t  pregr_stamp;		/* pregreet expiration time */
    time_t  dnsbl_stamp;		/* dnsbl expiration time */
    VSTRING *dnsbl_reply;		/* dnsbl reject text */
    /* Built-in SMTP protocol engine. */
    time_t  pipel_stamp;		/* pipelining expiration time */
    time_t  nsmtp_stamp;		/* non-smtp command expiration time */
    time_t  barlf_stamp;		/* bare newline expiration time */
    const char *rcpt_reply;		/* how to reject recipients */
    int     command_count;		/* error + junk command count */
    const char *protocol;		/* SMTP or ESMTP */
    char   *helo_name;			/* SMTP helo/ehlo */
    char   *sender;			/* MAIL FROM */
    VSTRING *cmd_buffer;		/* command read buffer */
    int     read_state;			/* command read state machine */
} PS_STATE;

#define PS_TIME_STAMP_NEW		(0)	/* test was never passed */
#define PS_TIME_STAMP_DISABLED		(1)	/* never passed but disabled */
#define PS_TIME_STAMP_INVALID		(-1)	/* must not be cached */

#define PS_STATE_FLAG_NOFORWARD		(1<<0)	/* don't forward this session */
#define PS_STATE_FLAG_UNUSED1		(1<<1)	/* use me! */
#define PS_STATE_FLAG_UNUSED2		(1<<2)	/* use me! */
#define PS_STATE_FLAG_NEW		(1<<3)	/* some test was never passed */
#define PS_STATE_FLAG_BLIST_FAIL	(1<<4)	/* blacklisted */
#define PS_STATE_FLAG_HANGUP		(1<<5)	/* NOT a test failure */
#define PS_STATE_FLAG_CACHE_EXPIRED	(1<<6)	/* cache retention expired */

 /*
  * Important: every MUMBLE_TODO flag must have a MUMBLE_PASS flag, such that
  * MUMBLE_PASS == PS_STATE_FLAGS_TODO_TO_PASS(MUMBLE_TODO).
  * 
  * MUMBLE_TODO flags must not be cleared once raised. The _TODO_TO_PASS and
  * _TODO_TO_DONE macros depend on this to decide that a group of tests is
  * passed or completed.
  * 
  * MUMBLE_DONE flags are used for "early" tests that have final results.
  * 
  * MUMBLE_SKIP flags are used for "deep" tests where the client messed up.
  * These flags look like MUMBLE_DONE but they are different. Deep tests can
  * tentatively pass, but can still fail later in a session. The "ignore"
  * action introduces an additional complication. MUMBLE_PASS indicates
  * either that a deep test passed tentatively, or that the test failed but
  * the result was ignored. MUMBLE_FAIL, on the other hand, is always final.
  * We use MUMBLE_SKIP to indicate that a decision was either "fail" or
  * forced "pass".
  */
#define PS_STATE_FLAGS_TODO_TO_PASS(todo_flags) ((todo_flags) >> 1)
#define PS_STATE_FLAGS_TODO_TO_DONE(todo_flags) ((todo_flags) << 1)

#define PS_STATE_FLAG_PREGR_FAIL	(1<<8)	/* failed pregreet test */
#define PS_STATE_FLAG_PREGR_PASS	(1<<9)	/* passed pregreet test */
#define PS_STATE_FLAG_PREGR_TODO	(1<<10)	/* pregreet test expired */
#define PS_STATE_FLAG_PREGR_DONE	(1<<11)	/* decision is final */

#define PS_STATE_FLAG_DNSBL_FAIL	(1<<12)	/* failed DNSBL test */
#define PS_STATE_FLAG_DNSBL_PASS	(1<<13)	/* passed DNSBL test */
#define PS_STATE_FLAG_DNSBL_TODO	(1<<14)	/* DNSBL test expired */
#define PS_STATE_FLAG_DNSBL_DONE	(1<<15)	/* decision is final */

 /* Room here for one more after-handshake test. */

#define PS_STATE_FLAG_PIPEL_FAIL	(1<<20)	/* failed pipelining test */
#define PS_STATE_FLAG_PIPEL_PASS	(1<<21)	/* passed pipelining test */
#define PS_STATE_FLAG_PIPEL_TODO	(1<<22)	/* pipelining test expired */
#define PS_STATE_FLAG_PIPEL_SKIP	(1<<23)	/* action is already logged */

#define PS_STATE_FLAG_NSMTP_FAIL	(1<<24)	/* failed non-SMTP test */
#define PS_STATE_FLAG_NSMTP_PASS	(1<<25)	/* passed non-SMTP test */
#define PS_STATE_FLAG_NSMTP_TODO	(1<<26)	/* non-SMTP test expired */
#define PS_STATE_FLAG_NSMTP_SKIP	(1<<27)	/* action is already logged */

#define PS_STATE_FLAG_BARLF_FAIL	(1<<28)	/* failed bare newline test */
#define PS_STATE_FLAG_BARLF_PASS	(1<<29)	/* passed bare newline test */
#define PS_STATE_FLAG_BARLF_TODO	(1<<30)	/* bare newline test expired */
#define PS_STATE_FLAG_BARLF_SKIP	(1<<31)	/* action is already logged */

 /*
  * Aggregates for individual tests.
  */
#define PS_STATE_FLAG_PREGR_TODO_FAIL \
	(PS_STATE_FLAG_PREGR_TODO | PS_STATE_FLAG_PREGR_FAIL)
#define PS_STATE_FLAG_DNSBL_TODO_FAIL \
	(PS_STATE_FLAG_DNSBL_TODO | PS_STATE_FLAG_DNSBL_FAIL)
#define PS_STATE_FLAG_PIPEL_TODO_FAIL \
	(PS_STATE_FLAG_PIPEL_TODO | PS_STATE_FLAG_PIPEL_FAIL)
#define PS_STATE_FLAG_NSMTP_TODO_FAIL \
	(PS_STATE_FLAG_NSMTP_TODO | PS_STATE_FLAG_NSMTP_FAIL)
#define PS_STATE_FLAG_BARLF_TODO_FAIL \
	(PS_STATE_FLAG_BARLF_TODO | PS_STATE_FLAG_BARLF_FAIL)

#define PS_STATE_FLAG_PIPEL_TODO_SKIP \
	(PS_STATE_FLAG_PIPEL_TODO | PS_STATE_FLAG_PIPEL_SKIP)
#define PS_STATE_FLAG_NSMTP_TODO_SKIP \
	(PS_STATE_FLAG_NSMTP_TODO | PS_STATE_FLAG_NSMTP_SKIP)
#define PS_STATE_FLAG_BARLF_TODO_SKIP \
	(PS_STATE_FLAG_BARLF_TODO | PS_STATE_FLAG_BARLF_SKIP)

#define PS_STATE_FLAG_PIPEL_TODO_PASS_FAIL \
	(PS_STATE_FLAG_PIPEL_TODO_FAIL | PS_STATE_FLAG_PIPEL_PASS)
#define PS_STATE_FLAG_NSMTP_TODO_PASS_FAIL \
	(PS_STATE_FLAG_NSMTP_TODO_FAIL | PS_STATE_FLAG_NSMTP_PASS)
#define PS_STATE_FLAG_BARLF_TODO_PASS_FAIL \
	(PS_STATE_FLAG_BARLF_TODO_FAIL | PS_STATE_FLAG_BARLF_PASS)

 /*
  * Separate aggregates for early tests and deep tests.
  */
#define PS_STATE_FLAG_EARLY_DONE \
	(PS_STATE_FLAG_PREGR_DONE | PS_STATE_FLAG_DNSBL_DONE)
#define PS_STATE_FLAG_EARLY_TODO \
	(PS_STATE_FLAG_PREGR_TODO | PS_STATE_FLAG_DNSBL_TODO)
#define PS_STATE_FLAG_EARLY_PASS \
	(PS_STATE_FLAG_PREGR_PASS | PS_STATE_FLAG_DNSBL_PASS)
#define PS_STATE_FLAG_EARLY_FAIL \
	(PS_STATE_FLAG_PREGR_FAIL | PS_STATE_FLAG_DNSBL_FAIL)

#define PS_STATE_FLAG_SMTPD_TODO \
	(PS_STATE_FLAG_PIPEL_TODO | PS_STATE_FLAG_NSMTP_TODO | \
	PS_STATE_FLAG_BARLF_TODO)
#define PS_STATE_FLAG_SMTPD_PASS \
	(PS_STATE_FLAG_PIPEL_PASS | PS_STATE_FLAG_NSMTP_PASS | \
	PS_STATE_FLAG_BARLF_PASS)
#define PS_STATE_FLAG_SMTPD_FAIL \
	(PS_STATE_FLAG_PIPEL_FAIL | PS_STATE_FLAG_NSMTP_FAIL | \
	PS_STATE_FLAG_BARLF_FAIL)

 /*
  * Super-aggregates for all tests combined.
  */
#define PS_STATE_FLAG_ANY_FAIL \
	(PS_STATE_FLAG_BLIST_FAIL | \
	PS_STATE_FLAG_EARLY_FAIL | PS_STATE_FLAG_SMTPD_FAIL)

#define PS_STATE_FLAG_ANY_PASS \
	(PS_STATE_FLAG_EARLY_PASS | PS_STATE_FLAG_SMTPD_PASS)

#define PS_STATE_FLAG_ANY_TODO \
	(PS_STATE_FLAG_EARLY_TODO | PS_STATE_FLAG_SMTPD_TODO)

#define PS_STATE_FLAG_ANY_TODO_FAIL \
	(PS_STATE_FLAG_ANY_TODO | PS_STATE_FLAG_ANY_FAIL)

#define PS_STATE_FLAG_ANY_UPDATE \
	(PS_STATE_FLAG_ANY_PASS)

 /*
  * See log_adhoc.c for discussion.
  */
typedef struct {
    int     dt_sec;			/* make sure it's signed */
    int     dt_usec;			/* make sure it's signed */
} DELTA_TIME;

#define PS_CALC_DELTA(x, y, z) \
    do { \
	(x).dt_sec = (y).tv_sec - (z).tv_sec; \
	(x).dt_usec = (y).tv_usec - (z).tv_usec; \
	while ((x).dt_usec < 0) { \
	    (x).dt_usec += 1000000; \
	    (x).dt_sec -= 1; \
	} \
	while ((x).dt_usec >= 1000000) { \
	    (x).dt_usec -= 1000000; \
	    (x).dt_sec += 1; \
	} \
	if ((x).dt_sec < 0) \
	    (x).dt_sec = (x).dt_usec = 0; \
    } while (0)

#define SIG_DIGS        2

 /*
  * Event management.
  */

/* PS_READ_EVENT_REQUEST - prepare for transition to next state */

#define PS_READ_EVENT_REQUEST(fd, action, context, timeout) do { \
	if (msg_verbose > 1) \
	    msg_info("%s: read-request fd=%d", myname, (fd)); \
	event_enable_read((fd), (action), (context)); \
	event_request_timer((action), (context), (timeout)); \
    } while (0)

#define PS_READ_EVENT_REQUEST2(fd, read_act, time_act, context, timeout) do { \
	if (msg_verbose > 1) \
	    msg_info("%s: read-request fd=%d", myname, (fd)); \
	event_enable_read((fd), (read_act), (context)); \
	event_request_timer((time_act), (context), (timeout)); \
    } while (0)

/* PS_CLEAR_EVENT_REQUEST - complete state transition */

#define PS_CLEAR_EVENT_REQUEST(fd, time_act, context) do { \
	if (msg_verbose > 1) \
	    msg_info("%s: clear-request fd=%d", myname, (fd)); \
	event_disable_readwrite(fd); \
	event_cancel_timer((time_act), (context)); \
    } while (0)

 /*
  * Failure enforcement policies.
  */
#define PS_NAME_ACT_DROP	"drop"
#define PS_NAME_ACT_ENFORCE	"enforce"
#define PS_NAME_ACT_IGNORE	"ignore"
#define PS_NAME_ACT_CONT	"continue"

#define PS_ACT_DROP		1
#define PS_ACT_ENFORCE		2
#define PS_ACT_IGNORE		3

 /*
  * Global variables.
  */
extern int ps_check_queue_length;	/* connections being checked */
extern int ps_post_queue_length;	/* being sent to real SMTPD */
extern DICT_CACHE *ps_cache_map;	/* cache table handle */
extern VSTRING *ps_temp;		/* scratchpad */
extern char *ps_smtpd_service_name;	/* path to real SMTPD */
extern int ps_pregr_action;		/* PS_ACT_DROP etc. */
extern int ps_dnsbl_action;		/* PS_ACT_DROP etc. */
extern int ps_pipel_action;		/* PS_ACT_DROP etc. */
extern int ps_nsmtp_action;		/* PS_ACT_DROP etc. */
extern int ps_barlf_action;		/* PS_ACT_DROP etc. */
extern int ps_min_ttl;			/* Update with new tests! */
extern int ps_max_ttl;			/* Update with new tests! */
extern STRING_LIST *ps_forbid_cmds;	/* CONNECT GET POST */
extern int ps_stress_greet_wait;	/* stressed greet wait */
extern int ps_normal_greet_wait;	/* stressed greet wait */
extern int ps_stress_cmd_time_limit;	/* stressed command limit */
extern int ps_normal_cmd_time_limit;	/* normal command time limit */
extern int ps_stress;			/* stress level */
extern int ps_check_queue_length_lowat;	/* stress low-water mark */
extern int ps_check_queue_length_hiwat;	/* stress high-water mark */
extern DICT *ps_dnsbl_reply;		/* DNSBL name mapper */

#define PS_EFF_GREET_WAIT \
	(ps_stress ? ps_stress_greet_wait : ps_normal_greet_wait)
#define PS_EFF_CMD_TIME_LIMIT \
	(ps_stress ? ps_stress_cmd_time_limit : ps_normal_cmd_time_limit)

 /*
  * String plumbing macros.
  */
#define PS_STRING_UPDATE(str, text) do { \
	if (str) myfree(str); \
	(str) = ((text) ? mystrdup(text) : 0); \
    } while (0)

#define PS_STRING_RESET(str) do { \
	if (str) { \
	    myfree(str); \
	    (str) = 0; \
	} \
    } while (0)

 /*
  * SLMs.
  */
#define STR(x)  vstring_str(x)
#define LEN(x)  VSTRING_LEN(x)

 /*
  * postscreen_state.c
  */
#define PS_CLIENT_ADDR_PORT(state) \
	(state)->smtp_client_addr, (state)->smtp_client_port

#define PS_PASS_SESSION_STATE(state, what, bits) do { \
	if (msg_verbose) \
	    msg_info("PASS %s %s:%s", (what), PS_CLIENT_ADDR_PORT(state)); \
	(state)->flags |= (bits); \
    } while (0)
#define PS_FAIL_SESSION_STATE(state, bits) do { \
	if (msg_verbose) \
	    msg_info("FAIL %s:%s", PS_CLIENT_ADDR_PORT(state)); \
	(state)->flags |= (bits); \
    } while (0)
#define PS_SKIP_SESSION_STATE(state, what, bits) do { \
	if (msg_verbose) \
	    msg_info("SKIP %s %s:%s", (what), PS_CLIENT_ADDR_PORT(state)); \
	(state)->flags |= (bits); \
    } while (0)
#define PS_DROP_SESSION_STATE(state, reply) do { \
	if (msg_verbose) \
	    msg_info("DROP %s:%s", PS_CLIENT_ADDR_PORT(state)); \
	(state)->flags |= PS_STATE_FLAG_NOFORWARD; \
	(state)->final_reply = (reply); \
	ps_conclude(state); \
    } while (0)
#define PS_ENFORCE_SESSION_STATE(state, reply) do { \
	if (msg_verbose) \
	    msg_info("ENFORCE %s:%s", PS_CLIENT_ADDR_PORT(state)); \
	(state)->rcpt_reply = (reply); \
	(state)->flags |= PS_STATE_FLAG_NOFORWARD; \
    } while (0)
#define PS_UNPASS_SESSION_STATE(state, bits) do { \
	if (msg_verbose) \
	    msg_info("UNPASS %s:%s", PS_CLIENT_ADDR_PORT(state)); \
	(state)->flags &= ~(bits); \
    } while (0)
#define PS_UNFAIL_SESSION_STATE(state, bits) do { \
	if (msg_verbose) \
	    msg_info("UNFAIL %s:%s", PS_CLIENT_ADDR_PORT(state)); \
	(state)->flags &= ~(bits); \
    } while (0)
#define PS_ADD_SERVER_STATE(state, fd) do { \
	(state)->smtp_server_fd = (fd); \
	ps_post_queue_length++; \
    } while (0)
#define PS_DEL_CLIENT_STATE(state) do { \
	event_server_disconnect((state)->smtp_client_stream); \
	(state)->smtp_client_stream = 0; \
	ps_check_queue_length--; \
    } while (0)
extern PS_STATE *ps_new_session_state(VSTREAM *, const char *, const char *);
extern void ps_free_session_state(PS_STATE *);
extern const char *ps_print_state_flags(int, const char *);

 /*
  * postscreen_dict.c
  */
extern int ps_addr_match_list_match(ADDR_MATCH_LIST *, const char *);
extern const char *ps_cache_lookup(DICT_CACHE *, const char *);
extern void ps_cache_update(DICT_CACHE *, const char *, const char *);

 /*
  * postscreen_dnsbl.c
  */
extern void ps_dnsbl_init(void);
extern int ps_dnsbl_retrieve(const char *, const char **);
extern void ps_dnsbl_request(const char *, void (*) (int, char *), char *);

 /*
  * postscreen_tests.c
  */
#define PS_INIT_TESTS(dst) do { \
	(dst)->flags = 0; \
	(dst)->pregr_stamp = PS_TIME_STAMP_INVALID; \
	(dst)->dnsbl_stamp = PS_TIME_STAMP_INVALID; \
	(dst)->pipel_stamp = PS_TIME_STAMP_INVALID; \
	(dst)->barlf_stamp = PS_TIME_STAMP_INVALID; \
    } while (0)
#define PS_BEGIN_TESTS(state, name) do { \
	(state)->test_name = (name); \
	GETTIMEOFDAY(&(state)->start_time); \
    } while (0)
extern void ps_new_tests(PS_STATE *);
extern void ps_parse_tests(PS_STATE *, const char *, time_t);
extern char *ps_print_tests(VSTRING *, PS_STATE *);
extern char *ps_print_grey_key(VSTRING *, const char *, const char *, const char *, const char *);

#define PS_MIN(x, y) ((x) < (y) ? (x) : (y))
#define PS_MAX(x, y) ((x) > (y) ? (x) : (y))

 /*
  * postscreen_early.c
  */
extern void ps_early_tests(PS_STATE *);
extern void ps_early_init(void);

 /*
  * postscreen_smtpd.c
  */
extern void ps_smtpd_tests(PS_STATE *);
extern void ps_smtpd_init(void);

 /*
  * postscreen_misc.c
  */
extern char *ps_format_delta_time(VSTRING *, struct timeval, DELTA_TIME *);
extern void ps_conclude(PS_STATE *);
extern void ps_hangup_event(PS_STATE *);

 /*
  * postscreen_send.c
  */
#define PS_SEND_REPLY(state, text) \
    ps_send_reply(vstream_fileno((state)->smtp_client_stream), \
		  (state)->smtp_client_addr, \
		  (state)->smtp_client_port, \
		  (text))
extern int ps_send_reply(int, const char *, const char *, const char *);
extern void ps_send_socket(PS_STATE *);

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
