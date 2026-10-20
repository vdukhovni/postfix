/*++
/* NAME
/*	ugid_from_string 3
/* SUMMARY
/*	Convert string to uid_t or gid_t
/* SYNOPSIS
/*	#include <ugid_from_string.h>
/*
/*	int	uid_from_string(
/*	const char *uid_str, 
/*	uid_t	*out)
/*
/*	int	gid_from_string(
/*	const char *gid_str, 
/*	gid_t	*out)
/* DESCRIPTION
/*	uid_from_string() and gid_from_string() convert string in decimal
/*	notation to uid_t and gid_t. The result value is 0 in case of
/*	success, -1 in case of error. These functions produce output
/*	only when successful.
/* DIAGNOSTICS
/*	Problems are reported to the standard error stream.
/* SEE ALSO
/*	sane_strtol(3)
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/	Wietse Venema
/*	porcupine.org
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>
#include <errno.h>
#include <stdlib.h>

 /*
  * Utility library.
  */
#include <ugid_from_string.h>
#include <sane_strtol.h>

/* uid_from_string - string to UID */

int     uid_from_string(const char *uid_str, uid_t *out)
{
    unsigned long n;
    char   *end;
    uid_t   uid;

    uid = (uid_t) (n = sane_strtoul(uid_str, &end, 10));
    if (*end != 0 || errno != 0 || uid != n)
	return (-1);
    *out = uid;
    return (0);
}

/* gid_from_string - string to GID */

int     gid_from_string(const char *gid_str, gid_t *out)
{
    unsigned long n;
    char   *end;
    gid_t   gid;

    gid = (gid_t) (n = sane_strtoul(gid_str, &end, 10));
    if (*end != 0 || errno != 0 || gid != n)
	return (-1);
    *out = gid;
    return (0);
}
