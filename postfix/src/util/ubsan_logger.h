#ifndef _UBSAN_LOGGER_H_INCLUDED_
#define _UBSAN_LOGGER_H_INCLUDED_

/*++
/* NAME
/*	ubsan_logger 3h
/* SUMMARY
/*	log libubsan errors
/* SYNOPSIS
/*	#include <ubsan_logger.h>
/*
/*	void	__ubsan_on_report(void)
/* DESCRIPTION
/*	This file implements an __ubsan_on_report() function that is
/*	needed in every program that may be built with libubsan (the UBSAN
/*	library). If such a program isn't built with libubsan, then this
/*	file is ignored. The decision is based on the HAS_UBSAN macro
/*	value.
/*
/*	__ubsan_on_report() overrides a weak symbol in libubsan.
/*	It is called after libubsan detects an error, before
/*	libubsan reports the error to stderr, and before libubsan
/*	may terminate the process. The implementation below calls
/*	__ubsan_get_current_report_data() to retrieve error details,
/*	and passes them to msg_warn() so that they become visible in
/*	Postfix logging.
/* RATIONALE
/* .ad
/* .fi
/*	libubsan logs an error message to stderr and may terminate
/*	its process. This is bad for network daemons that run in
/*	the background - no-one will see stderr output from a daemon
/*	process. libubsan can append error messages to a logfile, but that
/*	is not a good solution for client and server programs that run
/*	with different privileges (sometimes determined at runtime). The
/*	logfile(s) would be difficult to manage and protect.
/* SEE ALSO
/*	Unfortunately the __ubsan_on_report() functionality is not
/*	documented.
/* BUGS
/*	Should suppress stderr output so that client programs won't
/*	report an error twice. Unfortunately, setting the UBSAN_OPTIONS
/*	environment variable in main() is already too late.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

#ifdef HAS_UBSAN

 /*
  * System library.
  */
#include <sys_defs.h>

 /*
  * Utility library.
  */
#include <msg.h>

 /*
  * Ubsan library.
  */
extern void __ubsan_get_current_report_data(const char **OutIssueKind,
					            const char **OutMessage,
					            const char **OutFilename,
					            unsigned *OutLine,
					            unsigned *OutCol,
					            char **OutMemoryAddr);
extern void __ubsan_on_report(void);

/* __ubsan_on_report - hook into libubsan by overriding a weak symbol */

void    __ubsan_on_report(void)
{
    const char *OutIssueKind = 0, *OutMessage = 0, *OutFilename = 0;
    char   *OutMemoryAddr = 0;
    unsigned OutLine = 0, OutCol = 0;

#define __STR_OR_NULL(s) ((s) ? (s) : "(null)")

    __ubsan_get_current_report_data(&OutIssueKind, &OutMessage,
				    &OutFilename, &OutLine,
				    &OutCol, &OutMemoryAddr);

    msg_warn("%s:%u:%u: %s: %s",
	     OutFilename, OutLine, OutCol, __STR_OR_NULL(OutIssueKind),
	     __STR_OR_NULL(OutMessage));
}

#endif
#define INIT_UBSAN_LOGGER()		/* unconditional, empty */
#endif					/* _UBSAN_LOGGER_H_INCLUDED_ */
