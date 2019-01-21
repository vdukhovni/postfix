#ifndef _MSG_LOGGER_H_INCLUDED_
#define _MSG_LOGGER_H_INCLUDED_

/*++
/* NAME
/*	msg_logger 3h
/* SUMMARY
/*	direct diagnostics to logger service
/* SYNOPSIS
/*	#include <msg_logger.h>
/* DESCRIPTION
/* .nf

 /*
  * System library.
  */
#include <stdarg.h>

 /*
  * Utility library.
  */
#include <check_arg.h>

 /*
  * External interface.
  */
typedef void (*MSG_LOGGER_FALLBACK_FN) (const char *);

extern void msg_logger_init(const char *, const char *, const char *,
			            MSG_LOGGER_FALLBACK_FN);
extern void msg_logger_control(int,...);

/* Internal-only API: type-unchecked arguments. */
#define MSG_LOGGER_CTL_END		0
#define MSG_LOGGER_CTL_FALLBACK_ONLY	1
#define MSG_LOGGER_CTL_FALLBACK_FN	2

/* Safer API: type-checked arguments, external use. */
#define CA_MSG_LOGGER_CTL_END              MSG_LOGGER_CTL_END
#define CA_MSG_LOGGER_CTL_FALLBACK_ONLY(v) \
	MSG_LOGGER_CTL_FALLBACK_ONLY,  CHECK_VAL(MSG_LOGGER_CTL, int, (v))
#define CA_MSG_LOGGER_CTL_FALLBACK_FN(v) \
	MSG_LOGGER_CTL_FALLBACK_FN, CHECK_VAL(MSG_LOGGER_CTL, \
		MSG_LOGGER_FALLBACK_FN, (v))

CHECK_VAL_HELPER_DCL(MSG_LOGGER_CTL, int);
CHECK_VAL_HELPER_DCL(MSG_LOGGER_CTL, MSG_LOGGER_FALLBACK_FN);

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

#endif
