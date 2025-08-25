/*++
/* NAME
/*	x_esmtp_verb_mask 3
/* SUMMARY
/*	Parse X-Esmtp-Verbs header value
/* SYNOPSIS
/*	#include <x_esmtp_verb_mask.h>
/*
/*	int	x_esmtp_verb_mask(const char *hdr_val)
/*
/*	const char *str_x_esmtp_verb_mask(int bitmask)
/* DESCRIPTION
/*	x_esmtp_verb_mask() parses an X-Esmtp-Verbs: header value. It
/*	recognizes "SMTPUTF8" and "REQUIRETLS" as defined in
/*	<ehlo_mask.h>, separated by comma and/or whitespace. It
/*	returns the corresponding bitmask values from <sendopts.h>. The
/*	matching is case-insensitive. This feature ignores unsupported
/*	verb names.
/*
/*	str_x_esmtp_verb_mask() converts a mask into its equivalent names.
/*	The result is written to a static buffer that is overwritten upon
/*	each call. Unrecognized bits cause a fatal run-time error.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>

 /*
  * Utility library.
  */
#include <name_mask.h>

 /*
  * Global library.
  */
#include <sendopts.h>
#include <ehlo_mask.h>
#include <x_esmtp_verb_mask.h>

static NAME_MASK x_esmtp_verb_mask_table[] = {
    EHLO_VERB_SMTPUTF8, SOPT_SMTPUTF8_REQUESTED,
    EHLO_VERB_REQUIRETLS, SOPT_REQUIRETLS_ESMTP,
    0,
};

/* x_esmtp_verb_mask - extract info from X-Esmtp-Verbs header */

int     x_esmtp_verb_mask(const char *hdr_val)
{

    /*
     * Ignore unknown names for forward compatibility.
     */
    return (name_mask_delim_opt("esmtp verbs", x_esmtp_verb_mask_table, hdr_val,
		    CHARS_COMMA_SP, NAME_MASK_IGNORE | NAME_MASK_ANY_CASE));
}

/* str_x_esmtp_verb_mask - mask to string */

const char *str_x_esmtp_verb_mask(int mask_bits)
{

    /*
     * Don't allow unknown bits. Doing so makes no sense at this time.
     */
    return (str_name_mask_delim_opt((VSTRING *) 0, "esmtp verbs",
				    x_esmtp_verb_mask_table,
				    mask_bits, ", ", NAME_MASK_FATAL));
}
