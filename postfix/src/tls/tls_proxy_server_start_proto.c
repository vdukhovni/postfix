/*++
/* NAME
/*	tls_proxy_server_start_proto 3
/* SUMMARY
/*	Support for TLS_SERVER_START structures
/* SYNOPSIS
/*	#include <tls_proxy_server_start_proto.h>
/*
/*	TLS_PROXY_SERVER_START_PROPS(props, name1 = value1, ..., nameN = valueN)
/*
/*	int     tls_proxy_server_start_print(print_fn, stream, flags, ptr)
/*	ATTR_PRINT_COMMON_FN print_fn;
/*	VSTREAM *stream;
/*	int     flags;
/*	void    *ptr;
/*
/*	int	tls_proxy_server_start_scan(scan_fn, stream, flags, ptr)
/*	ATTR_SCAN_COMMON_FN scan_fn;
/*	VSTREAM *stream;
/*	int     flags;
/*	void    *ptr;
/*
/*	void	tls_proxy_server_start_free(start_props)
/*	TLS_SERVER_START_PROPS *start_props;
/* DESCRIPTION
/*	TLS_PROXY_SERVER_START_PROPS() makes shallow copies of the form
/*	(props)->name1 = value1, ..., (props)->nameN = valueN. The result
/*	should not be passed to tls_proxy_server_start_free().
/*
/*	tls_proxy_server_start_print() writes a TLS_SERVER_START_PROPS
/*	structure to the named stream using the specified attribute print
/*	routine. tls_proxy_server_start_print() is meant to be passed as
/*	a call-back to attr_print(), thusly:
/*
/*	... SEND_ATTR_FUNC(tls_proxy_server_start_print, (const void *) start_props), ...
/*
/*	tls_proxy_server_start_scan() reads a TLS_SERVER_START_PROPS
/*	structure from the named stream using the specified attribute
/*	scan routine. tls_proxy_server_start_scan() is meant to be passed
/*	as a call-back function to attr_scan(), as shown below.
/*
/*	tls_proxy_server_start_free() destroys a TLS_SERVER_START_PROPS
/*	structure that was created by tls_proxy_server_start_scan().
/*
/*	TLS_SERVER_START_PROPS *start_props = 0;
/*	...
/*	... RECV_ATTR_FUNC(tls_proxy_server_start_scan, (void *) &start_props)
/*	...
/*	if (start_props)
/*	    tls_proxy_server_start_free(start_props);
/* DIAGNOSTICS
/*	Fatal: out of memory.
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

#include <argv_attr.h>
#include <attr.h>
#include <msg.h>

/* Global library. */

#include <mail_params.h>

/* TLS library. */

#define TLS_INTERNAL
#include <tls.h>
#include <tls_proxy_attr.h>
#include <tls_proxy_server_start_proto.h>

#define STR(x) vstring_str(x)
#define LEN(x) VSTRING_LEN(x)

/* tls_proxy_server_start_print - send TLS_SERVER_START_PROPS over stream */

int     tls_proxy_server_start_print(ATTR_PRINT_COMMON_FN print_fn, VSTREAM *fp,
				             int flags, const void *ptr)
{
    const TLS_SERVER_START_PROPS *props = (const TLS_SERVER_START_PROPS *) ptr;
    int     ret;

#define STRING_OR_EMPTY(s) ((s) ? (s) : "")

    ret = print_fn(fp, flags | ATTR_FLAG_MORE,
		   SEND_ATTR_INT(TLS_ATTR_TIMEOUT, props->timeout),
		   SEND_ATTR_INT(TLS_ATTR_ENABLE_RPK, props->enable_rpk),
		   SEND_ATTR_INT(TLS_ATTR_REQUIRECERT, props->requirecert),
		   SEND_ATTR_STR(TLS_ATTR_SERVERID,
				 STRING_OR_EMPTY(props->serverid)),
		   SEND_ATTR_STR(TLS_ATTR_NAMADDR,
				 STRING_OR_EMPTY(props->namaddr)),
		   SEND_ATTR_STR(TLS_ATTR_CIPHER_GRADE,
				 STRING_OR_EMPTY(props->cipher_grade)),
		   SEND_ATTR_STR(TLS_ATTR_CIPHER_EXCLUSIONS,
				 STRING_OR_EMPTY(props->cipher_exclusions)),
		   SEND_ATTR_STR(TLS_ATTR_MDALG,
				 STRING_OR_EMPTY(props->mdalg)),
		   ATTR_TYPE_END);
    /* Do not flush the stream. */
    return (ret);
}

/* tls_proxy_server_start_scan - receive TLS_SERVER_START_PROPS from stream */

int     tls_proxy_server_start_scan(ATTR_SCAN_COMMON_FN scan_fn, VSTREAM *fp,
				            int flags, void *ptr)
{
    TLS_SERVER_START_PROPS *props
    = (TLS_SERVER_START_PROPS *) mymalloc(sizeof(*props));
    int     ret;
    VSTRING *serverid = vstring_alloc(25);
    VSTRING *namaddr = vstring_alloc(25);
    VSTRING *cipher_grade = vstring_alloc(25);
    VSTRING *cipher_exclusions = vstring_alloc(25);
    VSTRING *mdalg = vstring_alloc(25);

    /*
     * Note: memset() is not a portable way to initialize non-integer types.
     */
    memset(props, 0, sizeof(*props));
    props->ctx = 0;
    props->stream = 0;
    /* XXX Caller sets fd. */
    ret = scan_fn(fp, flags | ATTR_FLAG_MORE,
		  RECV_ATTR_INT(TLS_ATTR_TIMEOUT, &props->timeout),
		  RECV_ATTR_INT(TLS_ATTR_ENABLE_RPK, &props->enable_rpk),
		  RECV_ATTR_INT(TLS_ATTR_REQUIRECERT, &props->requirecert),
		  RECV_ATTR_STR(TLS_ATTR_SERVERID, serverid),
		  RECV_ATTR_STR(TLS_ATTR_NAMADDR, namaddr),
		  RECV_ATTR_STR(TLS_ATTR_CIPHER_GRADE, cipher_grade),
		  RECV_ATTR_STR(TLS_ATTR_CIPHER_EXCLUSIONS,
				cipher_exclusions),
		  RECV_ATTR_STR(TLS_ATTR_MDALG, mdalg),
		  ATTR_TYPE_END);
    props->serverid = vstring_export(serverid);
    props->namaddr = vstring_export(namaddr);
    props->cipher_grade = vstring_export(cipher_grade);
    props->cipher_exclusions = vstring_export(cipher_exclusions);
    props->mdalg = vstring_export(mdalg);
    ret = (ret == 8 ? 1 : -1);
    if (ret != 1) {
	tls_proxy_server_start_free(props);
	props = 0;
    }
    *(TLS_SERVER_START_PROPS **) ptr = props;
    return (ret);
}

/* tls_proxy_server_start_free - destroy TLS_SERVER_START_PROPS structure */

void    tls_proxy_server_start_free(TLS_SERVER_START_PROPS *props)
{
    /* XXX Caller closes fd. */
    myfree((void *) props->serverid);
    myfree((void *) props->namaddr);
    myfree((void *) props->cipher_grade);
    myfree((void *) props->cipher_exclusions);
    myfree((void *) props->mdalg);
    myfree((void *) props);
}

#endif
