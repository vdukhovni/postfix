/*++
/* NAME
/*	smtp 3h
/* SUMMARY
/*	smtp client program
/* SYNOPSIS
/*	#include "smtp.h"
/* DESCRIPTION
/* .nf

 /*
  * SASL library.
  */
#ifdef USE_SASL_AUTH
#include <sasl.h>
#include <saslutil.h>
#endif

 /*
  * Utility library.
  */
#include <vstream.h>
#include <vstring.h>
#include <argv.h>

 /*
  * Global library.
  */
#include <deliver_request.h>

 /*
  * State information associated with each SMTP delivery. We're bundling the
  * state so that we can give meaningful diagnostics in case of problems.
  */
typedef struct SMTP_STATE {
    VSTREAM *src;			/* queue file stream */
    DELIVER_REQUEST *request;		/* envelope info, offsets */
    struct SMTP_SESSION *session;	/* network connection */
    VSTRING *buffer;			/* I/O buffer */
    VSTRING *scratch;			/* scratch buffer */
    VSTRING *scratch2;			/* scratch buffer */
    int     status;			/* delivery status */
    int     features;			/* server features */
    ARGV   *history;			/* transaction log */
    int     error_mask;			/* error classes */
#ifdef USE_SASL_AUTH
    char   *sasl_mechanism_list;	/* server mechanism list */
    char   *sasl_username;		/* client username */
    char   *sasl_passwd;		/* client password */
    sasl_conn_t *sasl_conn;		/* SASL internal state */
    VSTRING *sasl_encoded;		/* encoding buffer */
    VSTRING *sasl_decoded;		/* decoding buffer */
    sasl_callback_t *sasl_callbacks;	/* stateful callbacks */
#endif
    off_t   size_limit;			/* server limit or unknown */
    int     space_left;			/* output length control */
    struct MIME_STATE *mime_state;	/* mime state machine */

    /*
     * Flags and counters to control the handling of mail delivery errors.
     * There is some redundancy for sanity checking. At the end of an SMTP
     * session all recipients should be marked one way or the other.
     */
    int     final_server;		/* final mail server */
    int     drop_count;			/* recipients marked as drop */
    int     keep_count;			/* recipients marked as keep */
} SMTP_STATE;

 /*
  * Server features.
  */
#define SMTP_FEATURE_ESMTP		(1<<0)
#define SMTP_FEATURE_8BITMIME		(1<<1)
#define SMTP_FEATURE_PIPELINING		(1<<2)
#define SMTP_FEATURE_SIZE		(1<<3)
#define SMTP_FEATURE_STARTTLS		(1<<4)
#define SMTP_FEATURE_AUTH		(1<<5)
#define SMTP_FEATURE_MAYBEPIX		(1<<6)	/* PIX smtp fixup mode */
#define SMTP_FEATURE_XFORWARD_NAME	(1<<7)
#define SMTP_FEATURE_XFORWARD_ADDR	(1<<8)
#define SMTP_FEATURE_XFORWARD_PROTO	(1<<9)
#define SMTP_FEATURE_XFORWARD_HELO	(1<<10)

 /*
  * Application-specific per-recipient status. At the end of each delivery
  * attempt each recipient is marked as DROP (remove from recipient list) or
  * KEEP (deliver to backup mail server). The ones marked DROP are deleted
  * before trying to deliver the remainder to a backup server. There's a bit
  * of redundcancy to ensure that all recipients are marked.
  * 
  * The single recipient list abstraction dates from the time that the SMTP
  * client would give up after one SMTP session, so that each recipient was
  * either bounced, delivered or deferred. Implicitly, all recipients were
  * marked as DROP.
  * 
  * This abstraction is less convenient when an SMTP client must be able to
  * deliver left-over recipients to a backup host. It might be more natural
  * to have an input list with recipients to deliver, and an output list with
  * the left-over recipients.
  */
#define SMTP_RCPT_KEEP	1		/* send to backup host */
#define SMTP_RCPT_DROP	2		/* remove from request */

#define SMTP_RCPT_MARK_INIT(state) do { \
	    (state)->drop_count = (state)->keep_count = 0; \
	} while (0)

#define SMTP_RCPT_MARK_DROP(state, rcpt) do { \
	    (rcpt)->status = SMTP_RCPT_DROP; (state)->drop_count++; \
	} while (0)

#define SMTP_RCPT_MARK_KEEP(state, rcpt) do { \
	    (rcpt)->status = SMTP_RCPT_KEEP; (state)->keep_count++; \
	} while (0)

#define SMTP_RCPT_MARK_ISSET(rcpt) ((rcpt)->status != 0)

extern int smtp_rcpt_mark_finish(SMTP_STATE *);

 /*
  * smtp.c
  */
extern int smtp_errno;			/* XXX can we get rid of this? */
 
#define SMTP_NONE	0		/* no error */
#define SMTP_FAIL	1		/* permanent error */
#define SMTP_RETRY	2		/* temporary error */
#define SMTP_LOOP	3		/* MX loop */

extern int smtp_host_lookup_mask;	/* host lookup methods to use */

#define SMTP_MASK_DNS		(1<<0)
#define SMTP_MASK_NATIVE	(1<<1)

 /*
  * What soft errors qualify for going to a backup host.
  */
extern int smtp_backup_mask;		/* when to try backup host */

#define SMTP_BACKUP_SESSION_FAILURE	(1<<0)
#define SMTP_BACKUP_MESSAGE_FAILURE	(1<<1)
#define SMTP_BACKUP_RECIPIENT_FAILURE	(1<<2)

 /*
  * smtp_session.c
  */
typedef struct SMTP_SESSION {
    VSTREAM *stream;			/* network connection */
    char   *host;			/* mail exchanger */
    char   *addr;			/* mail exchanger */
    char   *namaddr;			/* mail exchanger */
    int     best;			/* most preferred host */
} SMTP_SESSION;

extern SMTP_SESSION *smtp_session_alloc(VSTREAM *, char *, char *);
extern void smtp_session_free(SMTP_SESSION *);

 /*
  * smtp_connect.c
  */
extern int smtp_connect(SMTP_STATE *);

 /*
  * smtp_proto.c
  */
extern int smtp_helo(SMTP_STATE *);
extern int smtp_xfer(SMTP_STATE *);
extern void smtp_quit(SMTP_STATE *);

 /*
  * smtp_chat.c
  */
typedef struct SMTP_RESP {		/* server response */
    int     code;			/* status */
    char   *str;			/* text */
    VSTRING *buf;			/* origin of text */
} SMTP_RESP;

extern void PRINTFLIKE(2, 3) smtp_chat_cmd(SMTP_STATE *, char *,...);
extern SMTP_RESP *smtp_chat_resp(SMTP_STATE *);
extern void smtp_chat_reset(SMTP_STATE *);
extern void smtp_chat_notify(SMTP_STATE *);

 /*
  * smtp_misc.c.
  */
extern void smtp_rcpt_done(SMTP_STATE *, const char *, RECIPIENT *);

 /*
  * smtp_trouble.c
  */
extern int PRINTFLIKE(3, 4) smtp_conn_fail(SMTP_STATE *, int, char *,...);
extern int PRINTFLIKE(3, 4) smtp_site_fail(SMTP_STATE *, int, char *,...);
extern int PRINTFLIKE(3, 4) smtp_mesg_fail(SMTP_STATE *, int, char *,...);
extern void PRINTFLIKE(4, 5) smtp_rcpt_fail(SMTP_STATE *, int, RECIPIENT *, char *,...);
extern int smtp_stream_except(SMTP_STATE *, int, char *);

 /*
  * smtp_unalias.c
  */
extern const char *smtp_unalias_name(const char *);
extern VSTRING *smtp_unalias_addr(VSTRING *, const char *);

 /*
  * smtp_state.c
  */
extern SMTP_STATE *smtp_state_alloc(void);
extern void smtp_state_free(SMTP_STATE *);

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
