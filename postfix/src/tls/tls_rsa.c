/*++
/* NAME
/*	tls_rsa
/* SUMMARY
/*	RSA support
/* SYNOPSIS
/*	#define TLS_INTERNAL
/*	#include <tls.h>
/*
/*	RSA	*tls_tmp_rsa_cb(ssl, export, keylength)
/*	SSL	*ssl; /* unused */
/*	int	export;
/*	int	keylength;
/* DESCRIPTION
/*	This module maintains parameters for Diffie-Hellman key generation.
/*
/*	tls_tmp_rsa_cb() is a call-back routine for the
/*	SSL_CTX_set_tmp_rsa_callback() function.
/* LICENSE
/* .ad
/* .fi
/*	This software is free. You can do with it whatever you want.
/*	The original author kindly requests that you acknowledge
/*	the use of his software.
/* AUTHOR(S)
/*	Originally written by:
/*	Lutz Jaenicke
/*	BTU Cottbus
/*	Allgemeine Elektrotechnik
/*	Universitaetsplatz 3-4
/*	D-03044 Cottbus, Germany
/*
/*	Updated by:
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include <sys_defs.h>

#ifdef USE_TLS

/* TLS library. */

#define TLS_INTERNAL
#include <tls.h>

/* tls_tmp_rsa_cb - call-back to generate ephemeral RSA key */

RSA    *tls_tmp_rsa_cb(SSL *unused_ssl, int unused_export, int keylength)
{
    static RSA *rsa_tmp;

    if (rsa_tmp == 0) {
	BIGNUM *e = BN_new();

	if (e != 0 && BN_set_word(e, RSA_F4) && (rsa_tmp = RSA_new()) != 0)
	    if (!RSA_generate_key_ex(rsa_tmp, keylength, e, 0)) {
		RSA_free(rsa_tmp);
		rsa_tmp = 0;
	    }
	if (e)
	    BN_free(e);
    }
    return (rsa_tmp);
}

#ifdef TEST

int     main(int unused_argc, char **unused_argv)
{
    tls_tmp_rsa_cb(0, 1, 512);
    tls_tmp_rsa_cb(0, 1, 1024);
    tls_tmp_rsa_cb(0, 1, 2048);
    tls_tmp_rsa_cb(0, 0, 512);
}

#endif

#endif
