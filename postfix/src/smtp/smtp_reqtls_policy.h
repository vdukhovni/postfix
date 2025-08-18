#ifndef _SMTP_REQTLS_POLICY_INCLUDED_
#define _SMTP_REQTLS_POLICY_INCLUDED_

/*++
/* NAME
/*	smtp_reqtls_policy 3h
/* SUMMARY
/*	requiretls per-mx policy
/* SYNOPSIS
/*	#include <smtp_reqtls_policy.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <argv.h>

 /*
  * External interface.
  */
typedef ARGV SMTP_REQTLS_POLICY;

#define smtp_reqtls_policy_free argv_free
extern SMTP_REQTLS_POLICY *smtp_reqtls_policy_parse(const char *, const char *);
extern int smtp_reqtls_policy_eval(SMTP_REQTLS_POLICY *, const char *);

#define SMTP_REQTLS_POLICY_NAME_ENFORCE		"enforce"
#define SMTP_REQTLS_POLICY_NAME_BEST_EFFORT	"best-effort"
#define SMTP_REQTLS_POLICY_NAME_DISABLE		"disable"
#define SMTP_REQTLS_POLICY_NAME_ERROR		"error"

#define SMTP_REQTLS_POLICY_ACT_ENFORCE		2
#define SMTP_REQTLS_POLICY_ACT_BEST_EFFORT	1
#define SMTP_REQTLS_POLICY_ACT_DISABLE		0
#define SMTP_REQTLS_POLICY_ACT_ERROR		(-1)

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

#endif
