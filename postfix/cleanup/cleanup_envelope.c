/*++
/* NAME
/*	cleanup_envelope 3
/* SUMMARY
/*	process envelope segment
/* SYNOPSIS
/*	#include <cleanup.h>
/*
/*	void	cleanup_envelope_init(state, type, buf, len)
/*	CLEANUP_STATE *state;
/*	int	type;
/*	char	*buf;
/*	int	len;
/*
/*	void	cleanup_envelope_process(state, type, buf, len)
/*	CLEANUP_STATE *state;
/*	int	type;
/*	char	*buf;
/*	int	len;
/* DESCRIPTION
/*	This module processes the envelope segment of a mail message.
/*	While copying records from input to output it validates the
/*	message structure, rewrites sender/recipient addresses
/*	to canonical form, expands recipients according to
/*	entries in the virtual table, and updates the state structure.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include <sys_defs.h>
#include <string.h>
#include <stdlib.h>

/* Utility library. */

#include <msg.h>
#include <vstring.h>
#include <vstream.h>
#include <mymalloc.h>

/* Global library. */

#include <record.h>
#include <rec_type.h>
#include <cleanup_user.h>
#include <tok822.h>
#include <mail_params.h>
#include <ext_prop.h>

/* Application-specific. */

#include "cleanup.h"

#define STR	vstring_str

/* cleanup_envelope_init - initialization */

void    cleanup_envelope_init(CLEANUP_STATE *state, int type, char *str, int len)
{

    /*
     * The message content size record goes first, so it can easily be
     * updated in place. This information takes precedence over any size
     * estimate provided by the client. Size goes first so that it it easy to
     * produce queue file reports.
     */
    cleanup_out_format(state, REC_TYPE_SIZE, REC_TYPE_SIZE_FORMAT, 0L);
    state->action = cleanup_envelope_process;
    cleanup_envelope_process(state, type, str, len);
}

/* cleanup_envelope_process - process one envelope record */

void    cleanup_envelope_process(CLEANUP_STATE *state, int type, char *buf, int len)
{
    if (type == REC_TYPE_MESG) {
	if (state->sender == 0 || state->time == 0) {
	    msg_warn("%s: missing sender or time envelope record",
		     state->queue_id);
	    state->errs |= CLEANUP_STAT_BAD;
	} else {
	    if (state->warn_time == 0 && var_delay_warn_time > 0)
		state->warn_time = state->time + var_delay_warn_time * 3600L;
	    if (state->warn_time)
		cleanup_out_format(state, REC_TYPE_WARN, REC_TYPE_WARN_FORMAT,
				   state->warn_time);
	    state->action = cleanup_message_init;
	}
	return;
    }
    if (strchr(REC_TYPE_ENVELOPE, type) == 0) {
	msg_warn("%s: unexpected record type %d in envelope",
		 state->queue_id, type);
	state->errs |= CLEANUP_STAT_BAD;
	return;
    }
    if (msg_verbose)
	msg_info("envelope %c %.*s", type, len, buf);

    if (type == REC_TYPE_TIME) {
	state->time = atol(buf);
	cleanup_out(state, type, buf, len);
    } else if (type == REC_TYPE_FULL) {
	state->fullname = mystrdup(buf);
    } else if (type == REC_TYPE_FROM) {
	VSTRING *clean_addr = vstring_alloc(100);

	cleanup_rewrite_internal(clean_addr, buf);
	if (cleanup_send_canon_maps)
	    cleanup_map11_internal(state, clean_addr, cleanup_send_canon_maps,
				cleanup_ext_prop_mask & EXT_PROP_CANONICAL);
	if (cleanup_comm_canon_maps)
	    cleanup_map11_internal(state, clean_addr, cleanup_comm_canon_maps,
				cleanup_ext_prop_mask & EXT_PROP_CANONICAL);
	if (cleanup_masq_domains)
	    cleanup_masquerade_internal(clean_addr, cleanup_masq_domains);
	CLEANUP_OUT_BUF(state, type, clean_addr);
	if (state->sender == 0)
	    state->sender = mystrdup(STR(clean_addr));
	vstring_free(clean_addr);
    } else if (type == REC_TYPE_RCPT) {
	VSTRING *clean_addr = vstring_alloc(100);

	if (state->sender == 0) {		/* protect showq */
	    msg_warn("%s: envelope recipient precedes sender",
		     state->queue_id);
	    state->errs |= CLEANUP_STAT_BAD;
	    return;
	}
	cleanup_rewrite_internal(clean_addr, *buf ?
				 buf : var_empty_addr);
	if (cleanup_rcpt_canon_maps)
	    cleanup_map11_internal(state, clean_addr, cleanup_rcpt_canon_maps,
				cleanup_ext_prop_mask & EXT_PROP_CANONICAL);
	if (cleanup_comm_canon_maps)
	    cleanup_map11_internal(state, clean_addr, cleanup_comm_canon_maps,
				cleanup_ext_prop_mask & EXT_PROP_CANONICAL);
	cleanup_out_recipient(state, STR(clean_addr));
	if (state->recip == 0)
	    state->recip = mystrdup(STR(clean_addr));
	vstring_free(clean_addr);
    } else if (type == REC_TYPE_WARN) {
	if ((state->warn_time = atol(buf)) < 0) {
	    state->errs |= CLEANUP_STAT_BAD;
	    return;
	}
    } else {
	cleanup_out(state, type, buf, len);
    }
}
