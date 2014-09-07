/*++
/* NAME
/*	smtp_tls_audit 3
/* SUMMARY
/*	report effective TLS policy
/* SYNOPSIS
/*	#include "smtp.h"
/*
/*	smtp_tls_audit(
/*	const char *queue_id,
/*	SMTP_SESSION *session)
/* DESCRIPTION
/*	smtp_tls_audit() logs a record with TLS session properties
/*	as specified with the smtp_tls_audit_template configuration
/*	parameter.
/*
/*	Arguments:
/* .IP queue_id
/*	Mail delivery transaction identifier.
/* .IP session
/*	Client-side SMTP/TLS session state.
/* DIAGNOSTICS
/*	Unrecognized macro name in audit template.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Viktor Dukhovni
/*--*/

#ifdef USE_TLS

/* System library. */

#include <sys_defs.h>
#include <stdlib.h>
#include <string.h>

/* Utility library. */

#include <msg.h>
#include <vstring.h>
#include <dict.h>
#include <mac_expand.h>

/* Global library. */

#include <mail_params.h>

/* Application-specific. */

#include "smtp.h"

 /*
  * The mini symbol table name and keys used for expanding macros in smtp tls
  * audit log entries.
  */
#define TLS_AUDIT_DICT_TABLE	"tls_audit_template"	/* table name */
#define TLS_AUDIT_DICT_RELAY	"relay"	/* key */
#define TLS_AUDIT_DICT_ALEVEL	"level"	/* key */
#define TLS_AUDIT_DICT_PLEVEL	"policy"/* key */
#define TLS_AUDIT_DICT_STATUS	"auth"	/* key */
#define TLS_AUDIT_DICT_PROTOCOL	"protocol"	/* key */
#define TLS_AUDIT_DICT_CIPHER	"cipher"/* key */
#define TLS_AUDIT_DICT_CERT	"cert_digest"	/* key */
#define TLS_AUDIT_DICT_SPKI	"spki_digest"	/* key */

/* audit_lookup - macro parser call-back routine */

static const char *audit_lookup(const char *key, int unused_mode, char *dict)
{
    const char *value = dict_lookup(dict, key);

    if (value == 0)
	msg_warn("%s: unknown TLS audit template macro name: \"%s\"",
		 SMTP_X(TLS_AUDIT_TEMPLATE), key);
    return value;
}

/* expand_template - expand macros in the audit template */

static int expand_template(char *template, VSTRING *result)
{

#define NO_SCAN_FILTER	((const char *) 0)
    return mac_expand(result, template, MAC_EXP_FLAG_NONE, NO_SCAN_FILTER,
		      audit_lookup, TLS_AUDIT_DICT_TABLE);
}

/* smtp_tls_audit - log TLS audit trail */

void    smtp_tls_audit(const char *queue_id, SMTP_SESSION *session)
{
    SMTP_TLS_POLICY *tls = session->tls;
    TLS_SESS_STATE *TLScontext = session->tls_context;
    const char *policy_level;
    const char *actual_level;
    VSTRING *result = vstring_alloc(100);
    int     status;

    if (!*var_smtp_tls_audit_template)
	return;

#ifndef TLS_AUDIT_NONE_POLICY
    /* Do we log policy "none" and cleartext status when TLS is disabled?  */
    if (tls->policy_level <= TLS_LEV_NONE)
	return;
#endif

    dict_update(TLS_AUDIT_DICT_TABLE, TLS_AUDIT_DICT_RELAY,
		session->namaddrport);

    actual_level = str_tls_level(session->tls_level);
    policy_level = (session->tls_level == tls->policy_level) ? "" :
	str_tls_level(tls->policy_level);
    dict_update(TLS_AUDIT_DICT_TABLE, TLS_AUDIT_DICT_ALEVEL,
		actual_level ? actual_level : "");
    dict_update(TLS_AUDIT_DICT_TABLE, TLS_AUDIT_DICT_PLEVEL,
		policy_level ? policy_level : "");

    dict_update(TLS_AUDIT_DICT_TABLE, TLS_AUDIT_DICT_STATUS,
		TLScontext == 0 ? "Cleartext" :
		!TLS_CERT_IS_PRESENT(TLScontext) ? "Anonymous" :
		TLS_CERT_IS_MATCHED(TLScontext) ? "Verified" :
		TLS_CERT_IS_TRUSTED(TLScontext) ? "Trusted" :
		"Untrusted");
    dict_update(TLS_AUDIT_DICT_TABLE, TLS_AUDIT_DICT_PROTOCOL,
		TLScontext == 0 ? "" : TLScontext->protocol);
    dict_update(TLS_AUDIT_DICT_TABLE, TLS_AUDIT_DICT_CIPHER,
		TLScontext == 0 ? "" : TLScontext->cipher_name);
    dict_update(TLS_AUDIT_DICT_TABLE, TLS_AUDIT_DICT_CERT,
		TLScontext == 0 ? "" : TLScontext->peer_cert_fprint);
    dict_update(TLS_AUDIT_DICT_TABLE, TLS_AUDIT_DICT_SPKI,
		TLScontext == 0 ? "" : TLScontext->peer_pkey_fprint);

    status = expand_template(var_smtp_tls_audit_template, result);
    if (status == 0)
	msg_info("%s: %s", queue_id, STR(result));
    vstring_free(result);
}

#endif					/* USE_TLS */
