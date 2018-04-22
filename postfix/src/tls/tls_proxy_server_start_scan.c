/*++
/* NAME
/*	tls_proxy_server_start_scan
/* SUMMARY
/*	read TLS_SERVER_START_PROPS from stream
/* SYNOPSIS
/*	#include <tls_proxy.h>
/*
/*	int	tls_proxy_server_start_scan(scan_fn, stream, flags, ptr)
/*	ATTR_SCAN_MASTER_FN scan_fn;
/*	VSTREAM *stream;
/*	int     flags;
/*	void    *ptr;
/*
/*	void	tls_proxy_server_start_free(props)
/*	TLS_SERVER_START_PROPS *props;
/* DESCRIPTION
/*	tls_proxy_server_start_scan() reads a TLS_SERVER_START_PROPS
/*	structure from the named stream using the specified attribute
/*	scan routine. tls_proxy_server_start_scan() is meant to be passed
/*	as a call-back function to attr_scan(), as shown below.
/*
/*	tls_proxy_server_start_free() destroys a TLS_SERVER_START_PROPS
/*	structure that was created by tls_proxy_server_start_scan().
/*	This must be called even if the tls_proxy_server_start_scan()
/*	call returned an error.
/*
/*	TLS_SERVER_START_PROPS *props = 0;
/*	...
/*	... RECV_ATTR_FUNC(tls_proxy_server_start_scan, (void *) &props), ...
/*	...
/*	if (props)
/*	    tls_proxy_server_start_free(props);
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
/*--*/

#ifdef USE_TLS

/* System library. */

#include <sys_defs.h>

/* Utility library */

#include <attr.h>

/* TLS library. */

#include <tls.h>
#include <tls_proxy.h>

/* tls_proxy_server_start_scan - receive TLS_SERVER_START_PROPS from stream */

int     tls_proxy_server_start_scan(ATTR_SCAN_MASTER_FN scan_fn, VSTREAM *fp,
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
		  RECV_ATTR_INT(TLS_ATTR_REQUIRECERT, &props->requirecert),
		  RECV_ATTR_STR(TLS_ATTR_SERVERID, serverid),
		  RECV_ATTR_STR(TLS_ATTR_NAMADDR, namaddr),
		  RECV_ATTR_STR(TLS_ATTR_CIPHER_GRADE, cipher_grade),
	       RECV_ATTR_STR(TLS_ATTR_CIPHER_EXCLUSIONS, cipher_exclusions),
		  RECV_ATTR_STR(TLS_ATTR_MDALG, mdalg),
		  ATTR_TYPE_END);
    props->serverid = vstring_export(serverid);
    props->namaddr = vstring_export(namaddr);
    props->cipher_grade = vstring_export(cipher_grade);
    props->cipher_exclusions = vstring_export(cipher_exclusions);
    props->mdalg = vstring_export(mdalg);
    *(TLS_SERVER_START_PROPS **) ptr = props;
    return (ret == 7 ? 1 : -1);
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
