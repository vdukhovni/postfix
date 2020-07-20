/*++
/* NAME
/*	tls_dh
/* SUMMARY
/*	Diffie-Hellman parameter support
/* SYNOPSIS
/*	#define TLS_INTERNAL
/*	#include <tls.h>
/*
/*	void	tls_set_dh_from_file(path)
/*	const char *path;
/*
/*	void	tls_auto_eecdh_curves(ctx, configured)
/*	SSL_CTX	*ctx;
/*	char	*configured;
/*
/*	void	tls_tmp_dh(ctx)
/*	SSL_CTX *ctx;
/* DESCRIPTION
/*	This module maintains parameters for Diffie-Hellman key generation.
/*
/*	tls_tmp_dh() returns the configured or compiled-in FFDHE
/*	group parameters.
/*
/*	tls_set_dh_from_file() overrides compiled-in DH parameters
/*	with those specified in the named files. The file format
/*	is as expected by the PEM_read_DHparams() routine.
/*
/*	tls_auto_eecdh_curves() enables negotiation of the most preferred curve
/*	among the curves specified by the "configured" argument.
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
#include <mymalloc.h>
#include <stringops.h>

 /*
  * Global library
  */
#include <mail_params.h>

/* TLS library. */

#define TLS_INTERNAL
#include <tls.h>
#include <openssl/dh.h>
#ifndef OPENSSL_NO_ECDH
#include <openssl/ec.h>
#endif

/* Application-specific. */

 /*
  * Compiled-in FFDHE (finite-field ephemeral Diffie-Hellman) parameters.
  * Used when no parameters are explicitly loaded from a site-specific file.
  * Using an ASN.1 DER encoding avoids the need to explicitly manipulate the
  * internal representation of DH parameter objects.
  * 
  * The FFDHE group is now 2048-bit, as 1024 bits is increasingly considered to
  * weak by clients.  When greater security is required, use EECDH.
  */

 /*-
  * Generated via:
  *   $ openssl dhparam -2 -outform DER 2048 2>/dev/null |
  *     hexdump -ve '/1 "0x%02x, "' | fmt
  * TODO: generate at compile-time. But that is no good for the majority of
  * sites that install pre-compiled binaries, and breaks reproducible builds.
  * Instead, generate at installation time and use main.cf configuration.
  */
static unsigned char dh2048_der[] = {
    0x30, 0x82, 0x01, 0x08, 0x02, 0x82, 0x01, 0x01, 0x00, 0x9e, 0x28, 0x15,
    0xc5, 0xcc, 0x9b, 0x5a, 0xb0, 0xe9, 0xab, 0x74, 0x8b, 0x2a, 0x23, 0xce,
    0xea, 0x87, 0xa0, 0x18, 0x09, 0xd0, 0x40, 0x2c, 0x93, 0x23, 0x5d, 0xc0,
    0xe9, 0x78, 0x2c, 0x53, 0xd9, 0x3e, 0x21, 0x14, 0x89, 0x5c, 0x79, 0x73,
    0x1e, 0xbd, 0x23, 0x1e, 0x18, 0x65, 0x6d, 0xd2, 0x3c, 0xeb, 0x41, 0xca,
    0xbb, 0xa9, 0x99, 0x55, 0x84, 0xae, 0x9e, 0x70, 0x57, 0x25, 0x21, 0x42,
    0xaa, 0xdb, 0x82, 0xc6, 0xe6, 0xf1, 0xcf, 0xb7, 0xbc, 0x2a, 0x56, 0xcc,
    0x55, 0x1f, 0xad, 0xe9, 0x68, 0x18, 0x22, 0xfc, 0x09, 0x62, 0xc3, 0x32,
    0x1b, 0x05, 0x1f, 0xce, 0xec, 0xe3, 0x6d, 0xb5, 0x79, 0xe0, 0x89, 0x45,
    0xf3, 0xf3, 0x26, 0xa3, 0x81, 0xd9, 0x59, 0xee, 0xed, 0x78, 0xbe, 0x0e,
    0xdd, 0xf7, 0xef, 0xcb, 0x81, 0x3f, 0x01, 0xb7, 0x10, 0x8f, 0x0d, 0xbe,
    0x29, 0x21, 0x13, 0xff, 0x2a, 0x13, 0x25, 0x75, 0x99, 0xec, 0xf5, 0x2d,
    0x49, 0x01, 0x1d, 0xa4, 0x13, 0xe8, 0x2c, 0xc8, 0x13, 0x60, 0x57, 0x98,
    0xb1, 0x06, 0x45, 0x77, 0xa4, 0x24, 0xf9, 0x27, 0x3f, 0x08, 0xe6, 0x9b,
    0x4b, 0x20, 0x3b, 0x43, 0x69, 0xa3, 0xcc, 0x9a, 0xc4, 0x3c, 0x1e, 0xec,
    0xb7, 0x35, 0xe4, 0x59, 0x6b, 0x6d, 0x2a, 0xdf, 0xf7, 0x0b, 0xd4, 0x5a,
    0x0f, 0x79, 0x80, 0xe1, 0x75, 0x4c, 0x10, 0xea, 0x26, 0xf0, 0xd5, 0xf3,
    0xa6, 0x15, 0xa9, 0x3e, 0x3d, 0x0d, 0xb8, 0x53, 0x50, 0x49, 0x77, 0x49,
    0x47, 0x43, 0x39, 0xee, 0xb8, 0x8a, 0xe5, 0x14, 0xc4, 0xe3, 0x10, 0xfb,
    0xf5, 0x52, 0xef, 0xa5, 0x8f, 0xa4, 0x7e, 0x57, 0xb9, 0x5f, 0xda, 0x00,
    0x18, 0xf0, 0x72, 0x29, 0xd4, 0xfe, 0x90, 0x5a, 0x1f, 0x1a, 0x40, 0xee,
    0x4e, 0xfa, 0x3e, 0xf3, 0x72, 0x4b, 0xea, 0x44, 0x53, 0x43, 0x53, 0x57,
    0x9b, 0x02, 0x01, 0x02,
};

 /*
  * Cached results.
  */
static DH *dh_2048 = 0;

/* tls_set_dh_from_file - set Diffie-Hellman parameters from file */

void    tls_set_dh_from_file(const char *path)
{
    FILE   *paramfile;

    /*
     * This function is the first to set the DH parameters, but free any
     * prior value just in case the call sequence changes some day.
     */
    if (dh_2048) {
	DH_free(dh_2048);
	dh_2048 = 0;
    }
    if ((paramfile = fopen(path, "r")) != 0) {
	if ((dh_2048 = PEM_read_DHparams(paramfile, 0, 0, 0)) == 0) {
	    msg_warn("cannot load DH parameters from file %s"
		     " -- using compiled-in defaults", path);
	    tls_print_errors();
	}
	(void) fclose(paramfile);		/* 200411 */
    } else {
	msg_warn("cannot load DH parameters from file %s: %m"
		 " -- using compiled-in defaults", path);
    }
}

/* tls_tmp_dh - configure FFDHE group */

void    tls_tmp_dh(SSL_CTX *ctx)
{
    if (dh_2048 == 0) {
	const unsigned char *endp = dh2048_der;
	DH     *dh = 0;

	if (d2i_DHparams(&dh, &endp, sizeof(dh2048_der))
	    && sizeof(dh2048_der) == endp - dh2048_der) {
	    dh_2048 = dh;
	} else {
	    DH_free(dh);			/* Unlikely non-zero, but by
						 * the book */
	    msg_warn("error loading compiled-in DH parameters");
	}
    }
    if (ctx != 0 && dh_2048 != 0)
	SSL_CTX_set_tmp_dh(ctx, dh_2048);
}

void    tls_auto_eecdh_curves(SSL_CTX *ctx, const char *configured)
{
#ifndef OPENSSL_NO_ECDH
    SSL_CTX *tmpctx;
    int    *nids;
    int     space = 5;
    int     n = 0;
    int     unknown = 0;
    char   *save;
    char   *curves;
    char   *curve;

    if ((tmpctx = SSL_CTX_new(TLS_method())) == 0) {
	msg_warn("cannot allocate temp SSL_CTX, using default ECDHE curves");
	tls_print_errors();
	return;
    }
    nids = mymalloc(space * sizeof(int));
    curves = save = mystrdup(configured);
#define RETURN do { \
	myfree(save); \
	myfree(nids); \
	SSL_CTX_free(tmpctx); \
	return; \
    } while (0)

    while ((curve = mystrtok(&curves, CHARS_COMMA_SP)) != 0) {
	int     nid = EC_curve_nist2nid(curve);

	if (nid == NID_undef)
	    nid = OBJ_sn2nid(curve);
	if (nid == NID_undef)
	    nid = OBJ_ln2nid(curve);
	if (nid == NID_undef) {
	    msg_warn("ignoring unknown ECDHE curve \"%s\"",
		     curve);
	    continue;
	}

	/*
	 * Validate the NID by trying it as the sole EC curve for a
	 * throw-away SSL context.  Silently skip unsupported code points.
	 * This way, we can list X25519 and X448 as soon as the nids are
	 * assigned, and before the supporting code is implemented.  They'll
	 * be silently skipped when not yet supported.
	 */
	if (SSL_CTX_set1_curves(tmpctx, &nid, 1) <= 0) {
	    ++unknown;
	    continue;
	}
	if (++n > space) {
	    space *= 2;
	    nids = myrealloc(nids, space * sizeof(int));
	}
	nids[n - 1] = nid;
    }

    if (n == 0) {
	if (unknown > 0)
	    msg_warn("none of the configured ECDHE curves are supported");
	RETURN;
    }
    if (SSL_CTX_set1_curves(ctx, nids, n) <= 0) {
	msg_warn("failed to configure ECDHE curves");
	tls_print_errors();
	RETURN;
    }
    RETURN;
#endif
}

#ifdef TEST

int     main(int unused_argc, char **unused_argv)
{
    tls_tmp_dh(0);
    return (dh_2048 == 0);
}

#endif

#endif
