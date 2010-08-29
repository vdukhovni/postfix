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
  * Utility library.
  */
#include <dict_cache.h>

 /*
  * Global library.
  */
#include <addr_match_list.h>

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

/* READ_EVENT_REQUEST - prepare for transition to next state */

#define READ_EVENT_REQUEST(fd, action, context, timeout) do { \
    if (msg_verbose) msg_info("%s: read-request fd=%d", myname, (fd)); \
    event_enable_read((fd), (action), (context)); \
    event_request_timer((action), (context), (timeout)); \
} while (0)

/* CLEAR_EVENT_REQUEST - complete state transition */

#define CLEAR_EVENT_REQUEST(fd, action, context) do { \
    if (msg_verbose) msg_info("%s: clear-request fd=%d", myname, (fd)); \
    event_disable_readwrite(fd); \
    event_cancel_timer((action), (context)); \
} while (0)

 /*
  * SLMs.
  */
#define STR(x)  vstring_str(x)
#define LEN(x)  VSTRING_LEN(x)

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
extern int ps_dnsbl_retrieve(const char *);
extern void ps_dnsbl_request(const char *);

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
