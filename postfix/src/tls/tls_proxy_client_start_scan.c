/*++
/* NAME
/*	tls_proxy_client_start_scan
/* SUMMARY
/*	read TLS_CLIENT_START_PROPS from stream
/* SYNOPSIS
/*	#include <tls_proxy.h>
/*
/*	int	tls_proxy_client_start_scan(scan_fn, stream, flags, ptr)
/*	ATTR_SCAN_MASTER_FN scan_fn;
/*	VSTREAM *stream;
/*	int     flags;
/*	void    *ptr;
/*
/*	void	tls_proxy_client_start_free(props)
/*	TLS_CLIENT_START_PROPS *props;
/* DESCRIPTION
/*	tls_proxy_client_start_scan() reads a TLS_CLIENT_START_PROPS
/*	structure, without the stream of file descriptor members,
/*	from the named stream using the specified attribute scan
/*	routine. tls_proxy_client_start_scan() is meant to be passed
/*	as a call-back function to attr_scan(), as shown below.
/*
/*	tls_proxy_client_start_free() destroys a TLS_CLIENT_START_PROPS
/*	structure that was created by tls_proxy_client_start_scan().
/*	This must be called even if the tls_proxy_client_start_scan()
/*	call returned an error.
/*
/*	TLS_CLIENT_START_PROPS *props = 0;
/*	...
/*	... RECV_ATTR_FUNC(tls_proxy_client_start_scan, (void *) &props), ...
/*	...
/*	if (props != 0)
/*	    tls_proxy_client_start_free(props);
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

#include <argv_attr.h>
#include <attr.h>

/* TLS library. */

#include <tls.h>
#include <tls_proxy.h>

/* tls_proxy_client_start_scan - receive TLS_CLIENT_START_PROPS from stream */

int     tls_proxy_client_start_scan(ATTR_SCAN_MASTER_FN scan_fn, VSTREAM *fp,
				            int flags, void *ptr)
{
    TLS_CLIENT_START_PROPS *props
    = (TLS_CLIENT_START_PROPS *) mymalloc(sizeof(*props));
    int     ret;
    VSTRING *nexthop = vstring_alloc(25);
    VSTRING *host = vstring_alloc(25);
    VSTRING *namaddr = vstring_alloc(25);
    VSTRING *serverid = vstring_alloc(25);
    VSTRING *helo = vstring_alloc(25);
    VSTRING *protocols = vstring_alloc(25);
    VSTRING *cipher_grade = vstring_alloc(25);
    VSTRING *cipher_exclusions = vstring_alloc(25);
    VSTRING *mdalg = vstring_alloc(25);

    /*
     * Note: memset() is not a portable way to initialize non-integer types.
     */
    memset(props, 0, sizeof(*props));
    props->ctx = 0;
    props->stream = 0;
    props->fd = -1;
    props->dane = 0;
    ret = scan_fn(fp, flags | ATTR_FLAG_MORE,
		  RECV_ATTR_INT(TLS_ATTR_TIMEOUT, &props->timeout),
		  RECV_ATTR_INT(TLS_ATTR_TLS_LEVEL, &props->tls_level),
		  RECV_ATTR_STR(TLS_ATTR_NEXTHOP, nexthop),
		  RECV_ATTR_STR(TLS_ATTR_HOST, host),
		  RECV_ATTR_STR(TLS_ATTR_NAMADDR, namaddr),
		  RECV_ATTR_STR(TLS_ATTR_SERVERID, serverid),
		  RECV_ATTR_STR(TLS_ATTR_HELO, helo),
		  RECV_ATTR_STR(TLS_ATTR_PROTOCOLS, protocols),
		  RECV_ATTR_STR(TLS_ATTR_CIPHER_GRADE, cipher_grade),
	       RECV_ATTR_STR(TLS_ATTR_CIPHER_EXCLUSIONS, cipher_exclusions),
		  RECV_ATTR_FUNC(argv_attr_scan, &props->matchargv),
		  RECV_ATTR_STR(TLS_ATTR_MDALG, mdalg),
    /* TODO: DANE. */
		  ATTR_TYPE_END);
    props->nexthop = vstring_export(nexthop);
    props->host = vstring_export(host);
    props->namaddr = vstring_export(namaddr);
    props->serverid = vstring_export(serverid);
    props->helo = vstring_export(helo);
    props->protocols = vstring_export(protocols);
    props->cipher_grade = vstring_export(cipher_grade);
    props->cipher_exclusions = vstring_export(cipher_exclusions);
    props->mdalg = vstring_export(mdalg);
    *(TLS_CLIENT_START_PROPS **) ptr = props;
    return (ret == 12 ? 1 : -1);
}

/* tls_proxy_client_start_free - destroy TLS_CLIENT_START_PROPS structure */

void    tls_proxy_client_start_free(TLS_CLIENT_START_PROPS *props)
{
    /* TODO: stream and file descriptor. */
    myfree((void *) props->nexthop);
    myfree((void *) props->host);
    myfree((void *) props->namaddr);
    myfree((void *) props->serverid);
    myfree((void *) props->helo);
    myfree((void *) props->protocols);
    myfree((void *) props->cipher_grade);
    myfree((void *) props->cipher_exclusions);
    if (props->matchargv)
	argv_free((ARGV *) props->matchargv);
    myfree((void *) props->mdalg);
    /* TODO: DANE. */
    myfree((void *) props);
}

#endif
