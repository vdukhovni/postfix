/*++
/* NAME
/*	conv_time 3
/* SUMMARY
/*	time value conversion
/* SYNOPSIS
/*	#include <conv_time.h>
/*
/*	int	conv_time(strval, intval, def_unit);
/*	const char *strval;
/*	int	*intval;
/*	int	def_unit;
/* DESCRIPTION
/*	conv_time() converts a numerical time value with optional
/*	one-letter suffix that specifies an explicit time unit: s
/*	(seconds), m (minutes), h (hours), d (days) or w (weeks).
/*	Internally, time is represented in seconds.
/*
/*	Arguments:
/* .IP strval
/*	Input value.
/* .IP intval
/*	Result pointer.
/* .IP def_unit
/*	The default time unit suffix character.
/* DIAGNOSTICS
/*	The result value is non-zero in case of success, zero in
/*	case of a bad time value or a bad time unit suffix.
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
#include <stdio.h>			/* sscanf() */

/* Utility library. */

#include <msg.h>

/* Global library. */

#include <conv_time.h>

#define MINUTE	(60)
#define HOUR	(60 * MINUTE)
#define DAY	(24 * HOUR)
#define WEEK	(7 * DAY)

/* conv_time - convert time value */

int     conv_time(const char *strval, int *intval, int def_unit)
{
    char    unit;
    char    junk;

    switch (sscanf(strval, "%d%c%c", intval, &unit, &junk)) {
    case 1:
	unit = def_unit;
    case 2:
	switch (unit) {
	case 'w':
	    *intval *= WEEK;
	    return (1);
	case 'd':
	    *intval *= DAY;
	    return (1);
	case 'h':
	    *intval *= HOUR;
	    return (1);
	case 'm':
	    *intval *= MINUTE;
	    return (1);
	case 's':
	    return (1);
	}
    }
    return (0);
}
