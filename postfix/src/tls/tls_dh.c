/*++
/* NAME
/*	tls_dh
/* SUMMARY
/*	Diffie-Hellman parameter support
/* SYNOPSIS
/*	#define TLS_INTERNAL
/*	#include <tls.h>
/*
/*	void	tls_set_dh_from_file(path, bits)
/*	const char *path;
/*	int	bits;
/*
/*	int	tls_set_eecdh_curve(server_ctx, grade)
/*	SSL_CTX	*server_ctx;
/*	const char *grade;
/*
/*	DH	*tls_tmp_dh_cb(ssl, export, keylength)
/*	SSL	*ssl; /* unused */
/*	int	export;
/*	int	keylength;
/* DESCRIPTION
/*	This module maintains parameters for Diffie-Hellman key generation.
/*
/*	tls_tmp_dh_cb() is a call-back routine for the
/*	SSL_CTX_set_tmp_dh_callback() function.
/*
/*	tls_set_dh_from_file() overrides compiled-in DH parameters
/*	with those specified in the named files. The file format
/*	is as expected by the PEM_read_DHparams() routine. The
/*	"bits" argument must be 512 or 1024.
/*
/*	tls_set_eecdh_curve() enables ephemeral Elliptic-Curve DH
/*	key exchange algorithms by instantiating in the server SSL
/*	context a suitable curve (corresponding to the specified
/*	EECDH security grade) from the set of named curves in RFC
/*	4492 Section 5.1.1. Errors generate warnings, but do not
/*	disable TLS, rather we continue without EECDH. A zero
/*	result indicates that the grade is invalid or the corresponding
/*	curve could not be used.
/* DIAGNOSTICS
/*	In case of error, tls_set_dh_from_file() logs a warning and
/*	ignores the request.
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
#include <stdio.h>

/* Utility library. */

#include <msg.h>

 /*
  * Global library
  */
#include <mail_params.h>

/* TLS library. */

#define TLS_INTERNAL
#include <tls.h>
#include <openssl/dh.h>

/* Application-specific. */

 /*
  * Compiled-in DH parameters.  Used when no parameters are explicitly loaded
  * from a site-specific file.  Using an ASN.1 DER encoding avoids the need
  * to explicitly manipulate the internal representation of DH parameter
  * objects.
  * 
  * 512-bit parameters are used for export ciphers, and 1024-bit parameters are
  * used for non-export ciphers.  It is recommended that users override the
  * built-in 1024-bit default with a 2048-bit group and/or use EECDH.
  */

 /*-
  * Generated via:
  *   $ openssl dhparam -2 -outform DER 512 2>/dev/null |
  *     hexdump -ve '/1 "0x%02x, "' | fmt
  * TODO: generate at compile-time. But that is no good for the majority of
  * sites that install pre-compiled binaries, and breaks reproducible builds.
  * Instead, generate at installation time and use main.cf configuration.
  */
static unsigned char dh512_der[] = {
    0x30, 0x46, 0x02, 0x41, 0x00, 0xd8, 0xbf, 0x11, 0xd6, 0x41, 0x2a, 0x7a,
    0x9c, 0x78, 0xb2, 0xaa, 0x41, 0x23, 0x0a, 0xdc, 0xcf, 0xb7, 0x19, 0xc5,
    0x16, 0x4c, 0xcb, 0x4a, 0xd0, 0xd2, 0x1f, 0x1f, 0x70, 0x24, 0x86, 0x6f,
    0x51, 0x52, 0xc6, 0x5b, 0x28, 0xbb, 0x82, 0xe1, 0x24, 0x91, 0x3d, 0x4d,
    0x95, 0x56, 0xf8, 0x0b, 0x2c, 0xe0, 0x36, 0x67, 0x88, 0x64, 0x15, 0x1f,
    0x45, 0xd5, 0xb8, 0x0a, 0x00, 0x03, 0x76, 0x32, 0x0b, 0x02, 0x01, 0x02,
};

 /*-
  * Generated via:
  *   $ openssl dhparam -2 -outform DER 1024 2>/dev/null |
  *     hexdump -ve '/1 "0x%02x, "' | fmt
  * TODO: generate at compile-time. But that is no good for the majority of
  * sites that install pre-compiled binaries, and breaks reproducible builds.
  * Instead, generate at installation time and use main.cf configuration.
  */
static unsigned char dh1024_der[] = {
    0x30, 0x81, 0x87, 0x02, 0x81, 0x81, 0x00, 0xe2, 0x5a, 0x94, 0xe3, 0xf7,
    0xa6, 0x73, 0x2a, 0x4f, 0x31, 0x2e, 0xf9, 0x91, 0x39, 0x6d, 0x84, 0xee,
    0x44, 0x95, 0x68, 0xc8, 0x50, 0x39, 0x82, 0x9b, 0x65, 0x3b, 0xe7, 0x37,
    0xbf, 0xc7, 0x91, 0xf0, 0x66, 0x15, 0x5b, 0x27, 0xab, 0x7e, 0x25, 0xf6,
    0x94, 0x1d, 0xfd, 0x3b, 0xd3, 0x78, 0x29, 0xfb, 0x63, 0x2a, 0xd0, 0x87,
    0x01, 0x0c, 0x3a, 0x29, 0xac, 0x20, 0x59, 0xc5, 0x01, 0x88, 0xd6, 0x09,
    0xf1, 0x64, 0x37, 0x63, 0x1c, 0x36, 0xbe, 0x99, 0xc1, 0x43, 0x90, 0x4a,
    0x78, 0x1b, 0x06, 0xd9, 0x75, 0x82, 0x6e, 0xc4, 0x9a, 0x01, 0x03, 0x3d,
    0x43, 0x7f, 0xc8, 0xca, 0x14, 0x77, 0x4d, 0x27, 0xc1, 0xef, 0x5b, 0xe5,
    0x62, 0xfb, 0xc5, 0x41, 0x1d, 0x30, 0x01, 0xa2, 0xaf, 0x9a, 0xe7, 0x16,
    0xd6, 0xa2, 0xaa, 0x93, 0xce, 0x95, 0xf8, 0x94, 0x08, 0x53, 0xcd, 0x7d,
    0x45, 0x31, 0x4b, 0x02, 0x01, 0x02,
};

 /*
  * Cached results.
  */
static DH *dh_1024 = 0;
static DH *dh_512 = 0;

/* tls_set_dh_from_file - set Diffie-Hellman parameters from file */

void    tls_set_dh_from_file(const char *path, int bits)
{
    FILE   *paramfile;
    DH    **dhPtr;

    switch (bits) {
    case 512:
	dhPtr = &dh_512;
	break;
    case 1024:
	dhPtr = &dh_1024;
	break;
    default:
	msg_panic("Invalid DH parameters size %d, file %s", bits, path);
    }

    /*
     * This function is the first to set the DH parameters, but free any
     * prior value just in case the call sequence changes some day.
     */
    if (*dhPtr) {
	DH_free(*dhPtr);
	*dhPtr = 0;
    }
    if ((paramfile = fopen(path, "r")) != 0) {
	if ((*dhPtr = PEM_read_DHparams(paramfile, 0, 0, 0)) == 0) {
	    msg_warn("cannot load %d-bit DH parameters from file %s"
		     " -- using compiled-in defaults", bits, path);
	    tls_print_errors();
	}
	(void) fclose(paramfile);		/* 200411 */
    } else {
	msg_warn("cannot load %d-bit DH parameters from file %s: %m"
		 " -- using compiled-in defaults", bits, path);
    }
}

/* tls_get_dh - get compiled-in DH parameters */

static DH *tls_get_dh(const unsigned char *p, size_t plen)
{
    const unsigned char *endp = p;
    DH     *dh = 0;

    if (d2i_DHparams(&dh, &endp, plen) && plen == endp - p)
	return (dh);

    msg_warn("cannot load compiled-in DH parameters");
    if (dh)
	DH_free(dh);
    return (0);
}

/* tls_tmp_dh_cb - call-back for Diffie-Hellman parameters */

DH     *tls_tmp_dh_cb(SSL *unused_ssl, int export, int keylength)
{
    DH     *dh_tmp;

    if (export && keylength == 512) {		/* 40-bit export cipher */
	if (dh_512 == 0)
	    dh_512 = tls_get_dh(dh512_der, sizeof(dh512_der));
	dh_tmp = dh_512;
    } else {					/* ADH, DHE-RSA or DSA */
	if (dh_1024 == 0)
	    dh_1024 = tls_get_dh(dh1024_der, sizeof(dh1024_der));
	dh_tmp = dh_1024;
    }
    return (dh_tmp);
}

int     tls_set_eecdh_curve(SSL_CTX *server_ctx, const char *grade)
{
#if OPENSSL_VERSION_NUMBER >= 0x1000000fL && !defined(OPENSSL_NO_ECDH)
    int     nid;
    EC_KEY *ecdh;
    const char *curve;
    int     g;

#define TLS_EECDH_INVALID	0
#define TLS_EECDH_NONE		1
#define TLS_EECDH_STRONG	2
#define TLS_EECDH_ULTRA		3
    static NAME_CODE eecdh_table[] = {
	"none", TLS_EECDH_NONE,
	"strong", TLS_EECDH_STRONG,
	"ultra", TLS_EECDH_ULTRA,
	0, TLS_EECDH_INVALID,
    };

    switch (g = name_code(eecdh_table, NAME_CODE_FLAG_NONE, grade)) {
    default:
	msg_panic("Invalid eecdh grade code: %d", g);
    case TLS_EECDH_INVALID:
	msg_warn("Invalid TLS eecdh grade \"%s\": EECDH disabled", grade);
	return (0);
    case TLS_EECDH_NONE:
	return (1);
    case TLS_EECDH_STRONG:
	curve = var_tls_eecdh_strong;
	break;
    case TLS_EECDH_ULTRA:
	curve = var_tls_eecdh_ultra;
	break;
    }

    /*
     * Elliptic-Curve Diffie-Hellman parameters are either "named curves"
     * from RFC 4492 section 5.1.1, or explicitly described curves over
     * binary fields. OpenSSL only supports the "named curves", which provide
     * maximum interoperability. The recommended curve for 128-bit
     * work-factor key exchange is "prime256v1" a.k.a. "secp256r1" from
     * Section 2.7 of http://www.secg.org/download/aid-386/sec2_final.pdf
     */

    if ((nid = OBJ_sn2nid(curve)) == NID_undef) {
	msg_warn("unknown curve \"%s\": disabling EECDH support", curve);
	return (0);
    }
    ERR_clear_error();
    if ((ecdh = EC_KEY_new_by_curve_name(nid)) == 0
	|| SSL_CTX_set_tmp_ecdh(server_ctx, ecdh) == 0) {
	EC_KEY_free(ecdh);			/* OK if NULL */
	msg_warn("unable to use curve \"%s\": disabling EECDH support", curve);
	tls_print_errors();
	return (0);
    }
    EC_KEY_free(ecdh);
#endif
    return (1);
}

#ifdef TEST

int     main(int unused_argc, char **unused_argv)
{
    tls_tmp_dh_cb(0, 1, 512);
    tls_tmp_dh_cb(0, 1, 1024);
    tls_tmp_dh_cb(0, 1, 2048);
    tls_tmp_dh_cb(0, 0, 512);
    return (0);
}

#endif

#endif
