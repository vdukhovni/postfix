/*++
/* NAME
/*	lmtp 3h
/* SUMMARY
/*	lmtp client program
/* SYNOPSIS
/*	#include "lmtp.h"
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
#include <dsn_buf.h>

 /*
  * State information associated with each LMTP delivery. We're bundling the
  * state so that we can give meaningful diagnostics in case of problems.
  */
typedef struct LMTP_STATE {
    VSTREAM *src;			/* queue file stream */
    DELIVER_REQUEST *request;		/* envelope info, offsets */
    struct LMTP_SESSION *session;	/* network connection */
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
    int     sndbufsize;			/* total window size */
    int     reuse;			/* connection being reused */
    VSTRING *dsn_reason;		/* human-readable, sort of */
} LMTP_STATE;

#define LMTP_FEATURE_ESMTP	(1<<0)
#define LMTP_FEATURE_8BITMIME	(1<<1)
#define LMTP_FEATURE_PIPELINING	(1<<2)
#define LMTP_FEATURE_SIZE	(1<<3)
#define LMTP_FEATURE_AUTH	(1<<5)
#define LMTP_FEATURE_XFORWARD_NAME (1<<6)
#define LMTP_FEATURE_XFORWARD_ADDR (1<<7)
#define LMTP_FEATURE_XFORWARD_PROTO (1<<8)
#define LMTP_FEATURE_XFORWARD_HELO (1<<9)
#define LMTP_FEATURE_XFORWARD_DOMAIN (1<<10)
#define LMTP_FEATURE_DSN 	(1<<11)
#define LMTP_FEATURE_RSET_REJECTED (1<<12)

 /*
  * lmtp.c
  */
extern int lmtp_errno;			/* XXX can we get rid of this? */

 /*
  * lmtp_session.c
  */
typedef struct LMTP_SESSION {
    VSTREAM *stream;			/* network connection */
    char   *host;			/* mail exchanger, name */
    char   *addr;			/* mail exchanger, address */
    char   *namaddr;			/* mail exchanger, for logging */
    char   *dest;			/* remote endpoint name */
} LMTP_SESSION;

extern LMTP_SESSION *lmtp_session_alloc(VSTREAM *, const char *, const char *, const char *);
extern LMTP_SESSION *lmtp_session_free(LMTP_SESSION *);

 /*
  * lmtp_connect.c
  */
extern LMTP_SESSION *lmtp_connect(const char *, DSN_BUF *);

 /*
  * lmtp_proto.c
  */
extern int lmtp_lhlo(LMTP_STATE *);
extern int lmtp_xfer(LMTP_STATE *);
extern int lmtp_quit(LMTP_STATE *);
extern int lmtp_rset(LMTP_STATE *);

 /*
  * lmtp_chat.c
  */
typedef struct LMTP_RESP {		/* server response */
    int     code;			/* LMTP code */
    char   *dsn;			/* enhanced status */
    char   *str;			/* full reply */
    VSTRING *dsn_buf;			/* status buffer */
    VSTRING *str_buf;			/* reply buffer */
} LMTP_RESP;

extern void PRINTFLIKE(2, 3) lmtp_chat_cmd(LMTP_STATE *, char *,...);
extern LMTP_RESP *lmtp_chat_resp(LMTP_STATE *);
extern void lmtp_chat_reset(LMTP_STATE *);
extern void lmtp_chat_notify(LMTP_STATE *);

#define LMTP_RESP_FAKE(resp, _code, _dsn, _str) \
    ((resp)->code = (_code), \
     (resp)->dsn = (_dsn), \
     (resp)->str = (_str), \
     (resp))

 /*
  * lmtp_trouble.c
  */
extern int lmtp_sess_fail(LMTP_STATE *, DSN_BUF *);
extern int PRINTFLIKE(4, 5) lmtp_site_fail(LMTP_STATE *, const char *,
				             LMTP_RESP *, const char *,...);
extern int PRINTFLIKE(4, 5) lmtp_mesg_fail(LMTP_STATE *, const char *,
				             LMTP_RESP *, const char *,...);
extern void PRINTFLIKE(5, 6) lmtp_rcpt_fail(LMTP_STATE *, const char *, LMTP_RESP *, RECIPIENT *, const char *,...);
extern int lmtp_stream_except(LMTP_STATE *, int, const char *);

 /*
  * lmtp_state.c
  */
extern LMTP_STATE *lmtp_state_alloc(void);
extern void lmtp_state_free(LMTP_STATE *);

 /*
  * lmtp_rcpt.c
  */
extern void lmtp_rcpt_done(LMTP_STATE *, LMTP_RESP *, RECIPIENT *);

 /*
  * Status codes. Errors must have negative codes so that they do not
  * interfere with useful counts of work done.
  */
#define LMTP_OK			0	/* so far, so good */
#define LMTP_RETRY		(-1)	/* transient error */
#define LMTP_FAIL		(-2)	/* hard error */

 /*
  * lmtp_dsn.c
  */
extern void PRINTFLIKE(6, 7) lmtp_dsn_update(DSN_BUF *, const char *,
					             const char *,
					             int, const char *,
					             const char *,...);
extern void vlmtp_dsn_update(DSN_BUF *, const char *, const char *,
			             int, const char *,
			             const char *, va_list);
extern void lmtp_dsn_formal(DSN_BUF *, const char *, const char *,
			            int, const char *);

#define LMTP_DSN_ASSIGN(dsn, mta, stat, resp, why) \
    DSN_ASSIGN((dsn), (stat), DSB_DEF_ACTION, (why), DSB_DTYPE_SMTP, (resp), \
	(mta) ? DSB_MTYPE_DNS : DSB_MTYPE_NONE, (mta))

#define DSN_BY_LOCAL_MTA	((char *) 0)	/* DSN issued by local MTA */

 /*
  * Silly little macros.
  */
#define STR(s)	vstring_str(s)

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*
/*      Alterations for LMTP by:
/*      Philip A. Prindeville
/*      Mirapoint, Inc.
/*      USA.
/*
/*      Additional work on LMTP by:
/*      Amos Gouaux
/*      University of Texas at Dallas
/*      P.O. Box 830688, MC34
/*      Richardson, TX 75083, USA
/*--*/
