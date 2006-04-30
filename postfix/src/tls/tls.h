#ifndef _TLS_H_INCLUDED_
#define _TLS_H_INCLUDED_

/*++
/* NAME
/*      tls 3h
/* SUMMARY
/*      libtls internal interfaces
/* SYNOPSIS
/*      #include <tls.h>
/* DESCRIPTION
/* .nf

 /*
  * OpenSSL library.
  */
#include <openssl/lhash.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>

#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
#error "need OpenSSL version 0.9.5 or later"
#endif

 /*
  * Utility library.
  */
#include <vstream.h>

 /*
  * TLS session context, also used by the VSTREAM call-back routines for SMTP
  * input/output, and by OpenSSL call-back routines for key verification.
  */
#define CCERT_BUFSIZ	256
#define HOST_BUFSIZ  255		/* RFC 1035 */

typedef struct {
    SSL    *con;
    BIO    *internal_bio;		/* postfix/TLS side of pair */
    BIO    *network_bio;		/* network side of pair */
    char   *serverid;			/* unique server identifier */
    char   *peer_CN;			/* Peer Common Name */
    char   *issuer_CN;			/* Issuer Common Name */
    char   *peer_fingerprint;		/* ASCII fingerprint */
    char   *peername;
    int     enforce_verify_errors;
    int     enforce_CN;
    int     hostname_matched;
    int     peer_verified;
    const char *protocol;
    const char *cipher_name;
    int     cipher_usebits;
    int     cipher_algbits;
    int     log_level;			/* TLS library logging level */
    int     session_reused;		/* this session was reused */
} TLScontext_t;

#define TLS_BIO_BUFSIZE	8192

 /*
  * tls_client.c
  */
extern SSL_CTX *tls_client_init(int);
extern TLScontext_t *tls_client_start(SSL_CTX *, VSTREAM *, int, int,
				              const char *, const char *);

#define tls_client_stop(ctx , stream, timeout, failure, TLScontext) \
	tls_session_stop((ctx), (stream), (timeout), (failure), (TLScontext))

 /*
  * tls_server.c
  */
extern SSL_CTX *tls_server_init(int, int);
extern TLScontext_t *tls_server_start(SSL_CTX *, VSTREAM *, int,
				           const char *, const char *, int);

#define tls_server_stop(ctx , stream, timeout, failure, TLScontext) \
	tls_session_stop((ctx), (stream), (timeout), (failure), (TLScontext))

 /*
  * tls_session.c
  */
extern void tls_session_stop(SSL_CTX *, VSTREAM *, int, int,
			             TLScontext_t *);

#ifdef TLS_INTERNAL

#include <vstring.h>

extern VSTRING *tls_session_passivate(SSL_SESSION *);
extern SSL_SESSION *tls_session_activate(const char *, int);

 /*
  * tls_stream.c.
  */
extern void tls_stream_start(VSTREAM *, TLScontext_t *);
extern void tls_stream_stop(VSTREAM *);

 /*
  * tls_bio_ops.c: a generic multi-personality driver that retries SSL
  * operations until they are satisfied or until a hard error happens.
  * Because of its ugly multi-personality user interface we invoke it via
  * not-so-ugly single-personality wrappers.
  */
extern int tls_bio(int, int, TLScontext_t *,
		           int (*) (SSL *),	/* handshake */
		           int (*) (SSL *, void *, int),	/* read */
		           int (*) (SSL *, const void *, int),	/* write */
		           void *, int);

#define tls_bio_connect(fd, timeout, context) \
        tls_bio((fd), (timeout), (context), SSL_connect, \
		NULL, NULL, NULL, 0)
#define tls_bio_accept(fd, timeout, context) \
        tls_bio((fd), (timeout), (context), SSL_accept, \
		NULL, NULL, NULL, 0)
#define tls_bio_shutdown(fd, timeout, context) \
	tls_bio((fd), (timeout), (context), SSL_shutdown, \
		NULL, NULL, NULL, 0)
#define tls_bio_read(fd, buf, len, timeout, context) \
	tls_bio((fd), (timeout), (context), NULL, \
		SSL_read, NULL, (buf), (len))
#define tls_bio_write(fd, buf, len, timeout, context) \
	tls_bio((fd), (timeout), (context), NULL, \
		NULL, SSL_write, (buf), (len))

 /*
  * tls_dh.c
  */
extern void tls_set_dh_1024_from_file(const char *);
extern void tls_set_dh_512_from_file(const char *);
extern DH *tls_tmp_dh_cb(SSL *, int, int);

 /*
  * tls_rsa.c
  */
extern RSA *tls_tmp_rsa_cb(SSL *, int, int);

 /*
  * tls_verify.c
  */
extern char *tls_peer_CN(X509 *);
extern char *tls_issuer_CN(X509 *);
extern int tls_verify_certificate_callback(int, X509_STORE_CTX *);

 /*
  * tls_certkey.c
  */
extern int tls_set_ca_certificate_info(SSL_CTX *, const char *, const char *);
extern int tls_set_my_certificate_key_info(SSL_CTX *, const char *,
					           const char *,
					           const char *,
					           const char *);

 /*
  * tls_misc.c
  */
extern int TLScontext_index;

extern TLScontext_t *tls_alloc_context(int, const char *);
extern void tls_free_context(TLScontext_t *);
extern void tls_check_version(void);
extern long tls_bug_bits(void);
extern void tls_print_errors(void);
extern void tls_info_callback(const SSL *, int, int);
extern long tls_bio_dump_cb(BIO *, int, const char *, int, long, long);

 /*
  * tls_seed.c
  */
extern void tls_int_seed(void);
extern int tls_ext_seed(int);

 /*
  * tls_temp.c, code that is going away.
  */
#endif					/* TLS_INTERNAL */

/* LICENSE
/* .ad
/* .fi
/*      The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*      Wietse Venema
/*      IBM T.J. Watson Research
/*      P.O. Box 704
/*      Yorktown Heights, NY 10598, USA
/*--*/

#endif					/* _TLS_H_INCLUDED_ */
