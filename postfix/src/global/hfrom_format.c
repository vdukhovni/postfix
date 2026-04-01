/*++
/* NAME
/*	hfrom_format 3
/* SUMMARY
/*	Parse a header_from_format setting
/* SYNOPSIS
/*	#include <hfrom_format.h>
/*
/*	int	hfrom_format_parse(
/*	const char *name,
/*	const char *value)
/*
/*	const char *str_hfrom_format_code(int code)
/* DESCRIPTION
/*	hfrom_format_parse() takes a parameter name (used for
/*	diagnostics) and value, and maps it to the corresponding
/*	code: HFROM_FORMAT_NAME_STD maps to HFROM_FORMAT_CODE_STD,
/*	and HFROM_FORMAT_NAME_OBS maps to HFROM_FORMAT_CODE_OBS.
/*
/*	str_hfrom_format_code() does the reverse mapping.
/* DIAGNOSTICS
/*	All input errors are fatal.
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

 /*
  * System library.
  */
#include <sys_defs.h>

 /*
  * Utility library.
  */
#include <name_code.h>
#include <msg.h>

 /*
  * Global library.
  */
#include <mail_params.h>

 /*
  * Application-specific.
  */
#include <hfrom_format.h>

 /*
  * Primitive dependency injection.
  */
#ifdef TEST
extern NORETURN PRINTFLIKE(1, 2) test_msg_fatal(const char *,...);

#define msg_fatal test_msg_fatal
#endif

 /*
  * The name-to-code mapping.
  */
static const NAME_CODE hfrom_format_table[] = {
    HFROM_FORMAT_NAME_STD, HFROM_FORMAT_CODE_STD,
    HFROM_FORMAT_NAME_OBS, HFROM_FORMAT_CODE_OBS,
    0, -1,
};

/* hfrom_format_parse - parse header_from_format setting */

int     hfrom_format_parse(const char *name, const char *value)
{
    int     code;

    if ((code = name_code(hfrom_format_table, NAME_CODE_FLAG_NONE, value)) < 0)
	msg_fatal("invalid setting: \"%s = %s\"", name, value);
    return (code);
}

/* str_hfrom_format_code - convert code to string */

const char *str_hfrom_format_code(int code)
{
    const char *name;

    if ((name = str_name_code(hfrom_format_table, code)) == 0)
	msg_fatal("invalid header format code: %d", code);
    return (name);
}
