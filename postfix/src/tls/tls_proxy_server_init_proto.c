/*++
/* NAME
/*	tls_proxy_server_init_proto 3
/* SUMMARY
/*	TLS_SERVER_XXX structure support
/* SYNOPSIS
/*	#include <tls_proxy_server_init_proto.h>
/*
/*	char	*tls_proxy_server_init_serialize(print_fn, buf, init_props)
/*	ATTR_PRINT_COMMON_FN print_fn;
/*	VSTRING *buf;
/*	const TLS_SERVER_INIT_PROPS *init_props;
/*
/*	TLS_SERVER_INIT_PROPS *tls_proxy_server_init_from_string(
/*	ATTR_SCAN_COMMON_FN scan_fn,
/*	const VSTRING *buf)
/*
/*	int     tls_proxy_server_init_print(print_fn, stream, flags, ptr)
/*	ATTR_PRINT_COMMON_FN print_fn;
/*	VSTREAM *stream;
/*	int     flags;
/*	void    *ptr;
/*
/*	int	tls_proxy_server_init_scan(scan_fn, stream, flags, ptr)
/*	ATTR_SCAN_COMMON_FN scan_fn;
/*	VSTREAM *stream;
/*	int     flags;
/*	void    *ptr;
/*
/*	tls_proxy_server_init_free(init_props)
/*	TLS_SERVER_INIT_PROPS *init_props;
/* DESCRIPTION
/*	tls_proxy_server_init_serialize() serializes the specified object
/*	to a memory buffer, using the specified print function (typically,
/*	attr_print_plain). The result can be used determine whether
/*	there are any differences between instances of the same object
/*	type.
/*
/*      tls_proxy_server_init_from_string() deserializes the specified
/*      buffer into a TLS_SERVER_INIT_PROPS object, and returns null in case
/*      of error. The result if not null should be passed to
/*      tls_proxy_server_init_free().
/*
/*	tls_proxy_server_init_print() writes a TLS_SERVER_INIT_PROPS
/*	structure to the named stream using the specified attribute print
/*	routine. tls_proxy_server_init_print() is meant to be passed as
/*	a call-back to attr_print(), thusly:
/*
/*	... SEND_ATTR_FUNC(tls_proxy_server_init_print, (const void *) init_props), ...
/*
/*	tls_proxy_server_init_scan() reads a TLS_SERVER_INIT_PROPS
/*	structure from the named stream using the specified attribute
/*	scan routine. tls_proxy_server_init_scan() is meant to be passed
/*	as a call-back function to attr_scan(), as shown below.
/*
/*	tls_proxy_server_init_free() destroys a TLS_SERVER_INIT_PROPS
/*	structure that was created by tls_proxy_server_init_scan().
/*
/*	TLS_SERVER_INIT_PROPS *init_props = 0;
/*	...
/*	... RECV_ATTR_FUNC(tls_proxy_server_init_scan, (void *) &init_props)
/*	...
/*	if (init_props)
/*	    tls_proxy_server_init_free(init_props);
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	Google, Inc.
/*	111 8th Avenue
/*	New York, NY 10011, USA
/*
/*	Wietse Venema
/*	porcupine.org
/*--*/

#ifdef USE_TLS

/* System library. */

#include <sys_defs.h>

/* Utility library */

#include <attr.h>
#include <msg.h>

/* Global library. */

#include <mail_params.h>

/* TLS library. */

#include <tls.h>
#include <tls_proxy_attr.h>
#include <tls_proxy_server_init_proto.h>

/* tls_proxy_server_init_serialize - serialize to string */

char   *tls_proxy_server_init_serialize(ATTR_PRINT_COMMON_FN print_fn,
					        VSTRING *buf,
				         const TLS_SERVER_INIT_PROPS *props)
{
    const char myname[] = "tls_proxy_server_init_serialize";
    VSTREAM *mp;

    if ((mp = vstream_memopen(buf, O_WRONLY)) == 0
	|| print_fn(mp, ATTR_FLAG_NONE,
		    SEND_ATTR_FUNC(tls_proxy_server_init_print,
				   (const void *) props),
		    ATTR_TYPE_END) != 0
	|| vstream_fclose(mp) != 0)
	msg_fatal("%s: can't serialize properties: %m", myname);
    return (vstring_str(buf));
}

/* tls_proxy_server_init_from_string - deserialize TLS_SERVER_INIT_PROPS */

TLS_SERVER_INIT_PROPS *tls_proxy_server_init_from_string(
                                                ATTR_SCAN_COMMON_FN scan_fn,
                                                             VSTRING *buf)
{
    const char myname[] = "tls_proxy_server_init_from_string";
    TLS_SERVER_INIT_PROPS *props = 0;
    VSTREAM *mp;

    if ((mp = vstream_memopen(buf, O_RDONLY)) == 0
        || scan_fn(mp, ATTR_FLAG_NONE,
                   RECV_ATTR_FUNC(tls_proxy_server_init_scan,
                                  (void *) &props),
                   ATTR_TYPE_END) != 1
        || vstream_fclose(mp) != 0)
        msg_fatal("%s: can't deserialize properties: %m", myname);
    return (props);
}

/* tls_proxy_server_init_print - send TLS_SERVER_INIT_PROPS over stream */

int     tls_proxy_server_init_print(ATTR_PRINT_COMMON_FN print_fn, VSTREAM *fp,
				            int flags, const void *ptr)
{
    const TLS_SERVER_INIT_PROPS *props = (const TLS_SERVER_INIT_PROPS *) ptr;
    int     ret;

#define STRING_OR_EMPTY(s) ((s) ? (s) : "")

    ret = print_fn(fp, flags | ATTR_FLAG_MORE,
		   SEND_ATTR_STR(TLS_ATTR_LOG_PARAM,
				 STRING_OR_EMPTY(props->log_param)),
		   SEND_ATTR_STR(TLS_ATTR_LOG_LEVEL,
				 STRING_OR_EMPTY(props->log_level)),
		   SEND_ATTR_INT(TLS_ATTR_VERIFYDEPTH, props->verifydepth),
		   SEND_ATTR_STR(TLS_ATTR_CACHE_TYPE,
				 STRING_OR_EMPTY(props->cache_type)),
		   SEND_ATTR_INT(TLS_ATTR_SET_SESSID, props->set_sessid),
		   SEND_ATTR_STR(TLS_ATTR_CHAIN_FILES,
				 STRING_OR_EMPTY(props->chain_files)),
		   SEND_ATTR_STR(TLS_ATTR_CERT_FILE,
				 STRING_OR_EMPTY(props->cert_file)),
		   SEND_ATTR_STR(TLS_ATTR_KEY_FILE,
				 STRING_OR_EMPTY(props->key_file)),
		   SEND_ATTR_STR(TLS_ATTR_DCERT_FILE,
				 STRING_OR_EMPTY(props->dcert_file)),
		   SEND_ATTR_STR(TLS_ATTR_DKEY_FILE,
				 STRING_OR_EMPTY(props->dkey_file)),
		   SEND_ATTR_STR(TLS_ATTR_ECCERT_FILE,
				 STRING_OR_EMPTY(props->eccert_file)),
		   SEND_ATTR_STR(TLS_ATTR_ECKEY_FILE,
				 STRING_OR_EMPTY(props->eckey_file)),
		   SEND_ATTR_STR(TLS_ATTR_CAFILE,
				 STRING_OR_EMPTY(props->CAfile)),
		   SEND_ATTR_STR(TLS_ATTR_CAPATH,
				 STRING_OR_EMPTY(props->CApath)),
		   SEND_ATTR_STR(TLS_ATTR_PROTOCOLS,
				 STRING_OR_EMPTY(props->protocols)),
		   SEND_ATTR_STR(TLS_ATTR_EECDH_GRADE,
				 STRING_OR_EMPTY(props->eecdh_grade)),
		   SEND_ATTR_STR(TLS_ATTR_DH1K_PARAM_FILE,
				 STRING_OR_EMPTY(props->dh1024_param_file)),
		   SEND_ATTR_STR(TLS_ATTR_DH512_PARAM_FILE,
				 STRING_OR_EMPTY(props->dh512_param_file)),
		   SEND_ATTR_INT(TLS_ATTR_ASK_CCERT, props->ask_ccert),
		   SEND_ATTR_STR(TLS_ATTR_MDALG,
				 STRING_OR_EMPTY(props->mdalg)),
		   ATTR_TYPE_END);
    /* Do not flush the stream. */
    return (ret);
}

/* tls_proxy_server_init_scan - receive TLS_SERVER_INIT_PROPS from stream */

int     tls_proxy_server_init_scan(ATTR_SCAN_COMMON_FN scan_fn, VSTREAM *fp,
				           int flags, void *ptr)
{
    TLS_SERVER_INIT_PROPS *props
    = (TLS_SERVER_INIT_PROPS *) mymalloc(sizeof(*props));
    int     ret;
    VSTRING *log_param = vstring_alloc(25);
    VSTRING *log_level = vstring_alloc(25);
    VSTRING *cache_type = vstring_alloc(25);
    VSTRING *chain_files = vstring_alloc(25);
    VSTRING *cert_file = vstring_alloc(25);
    VSTRING *key_file = vstring_alloc(25);
    VSTRING *dcert_file = vstring_alloc(25);
    VSTRING *dkey_file = vstring_alloc(25);
    VSTRING *eccert_file = vstring_alloc(25);
    VSTRING *eckey_file = vstring_alloc(25);
    VSTRING *CAfile = vstring_alloc(25);
    VSTRING *CApath = vstring_alloc(25);
    VSTRING *protocols = vstring_alloc(25);
    VSTRING *eecdh_grade = vstring_alloc(25);
    VSTRING *dh1024_param_file = vstring_alloc(25);
    VSTRING *dh512_param_file = vstring_alloc(25);
    VSTRING *mdalg = vstring_alloc(25);

    /*
     * Note: memset() is not a portable way to initialize non-integer types.
     */
    memset(props, 0, sizeof(*props));
    ret = scan_fn(fp, flags | ATTR_FLAG_MORE,
		  RECV_ATTR_STR(TLS_ATTR_LOG_PARAM, log_param),
		  RECV_ATTR_STR(TLS_ATTR_LOG_LEVEL, log_level),
		  RECV_ATTR_INT(TLS_ATTR_VERIFYDEPTH, &props->verifydepth),
		  RECV_ATTR_STR(TLS_ATTR_CACHE_TYPE, cache_type),
		  RECV_ATTR_INT(TLS_ATTR_SET_SESSID, &props->set_sessid),
		  RECV_ATTR_STR(TLS_ATTR_CHAIN_FILES, chain_files),
		  RECV_ATTR_STR(TLS_ATTR_CERT_FILE, cert_file),
		  RECV_ATTR_STR(TLS_ATTR_KEY_FILE, key_file),
		  RECV_ATTR_STR(TLS_ATTR_DCERT_FILE, dcert_file),
		  RECV_ATTR_STR(TLS_ATTR_DKEY_FILE, dkey_file),
		  RECV_ATTR_STR(TLS_ATTR_ECCERT_FILE, eccert_file),
		  RECV_ATTR_STR(TLS_ATTR_ECKEY_FILE, eckey_file),
		  RECV_ATTR_STR(TLS_ATTR_CAFILE, CAfile),
		  RECV_ATTR_STR(TLS_ATTR_CAPATH, CApath),
		  RECV_ATTR_STR(TLS_ATTR_PROTOCOLS, protocols),
		  RECV_ATTR_STR(TLS_ATTR_EECDH_GRADE, eecdh_grade),
		  RECV_ATTR_STR(TLS_ATTR_DH1K_PARAM_FILE, dh1024_param_file),
		  RECV_ATTR_STR(TLS_ATTR_DH512_PARAM_FILE, dh512_param_file),
		  RECV_ATTR_INT(TLS_ATTR_ASK_CCERT, &props->ask_ccert),
		  RECV_ATTR_STR(TLS_ATTR_MDALG, mdalg),
		  ATTR_TYPE_END);
    /* Always construct a well-formed structure. */
    props->log_param = vstring_export(log_param);
    props->log_level = vstring_export(log_level);
    props->cache_type = vstring_export(cache_type);
    props->chain_files = vstring_export(chain_files);
    props->cert_file = vstring_export(cert_file);
    props->key_file = vstring_export(key_file);
    props->dcert_file = vstring_export(dcert_file);
    props->dkey_file = vstring_export(dkey_file);
    props->eccert_file = vstring_export(eccert_file);
    props->eckey_file = vstring_export(eckey_file);
    props->CAfile = vstring_export(CAfile);
    props->CApath = vstring_export(CApath);
    props->protocols = vstring_export(protocols);
    props->eecdh_grade = vstring_export(eecdh_grade);
    props->dh1024_param_file = vstring_export(dh1024_param_file);
    props->dh512_param_file = vstring_export(dh512_param_file);
    props->mdalg = vstring_export(mdalg);
    ret = (ret == 20 ? 1 : -1);
    if (ret != 1) {
	tls_proxy_server_init_free(props);
	props = 0;
    }
    *(TLS_SERVER_INIT_PROPS **) ptr = props;
    return (ret);
}

/* tls_proxy_server_init_free - destroy TLS_SERVER_INIT_PROPS structure */

void    tls_proxy_server_init_free(TLS_SERVER_INIT_PROPS *props)
{
    myfree((void *) props->log_param);
    myfree((void *) props->log_level);
    myfree((void *) props->cache_type);
    myfree((void *) props->chain_files);
    myfree((void *) props->cert_file);
    myfree((void *) props->key_file);
    myfree((void *) props->dcert_file);
    myfree((void *) props->dkey_file);
    myfree((void *) props->eccert_file);
    myfree((void *) props->eckey_file);
    myfree((void *) props->CAfile);
    myfree((void *) props->CApath);
    myfree((void *) props->protocols);
    myfree((void *) props->eecdh_grade);
    myfree((void *) props->dh1024_param_file);
    myfree((void *) props->dh512_param_file);
    myfree((void *) props->mdalg);
    myfree((void *) props);
}

#endif
