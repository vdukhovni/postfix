/*++
/* NAME
/*	valid_hostname 3
/* SUMMARY
/*	network name validation
/* SYNOPSIS
/*	#include <valid_hostname.h>
/*
/*	int	valid_hostname(name)
/*	const char *name;
/* DESCRIPTION
/*	valid_hostname() scrutinizes a hostname: the name should be no
/*	longer than VALID_HOSTNAME_LEN characters, should contain only
/*	letters, digits, dots and hyphens, no adjacent dots and hyphens,
/*	no leading or trailing dots or hyphens.
/* SEE ALSO
/*	RFC 952, 1123
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
#include <ctype.h>

/* Utility library. */

#include "msg.h"
#include "mymalloc.h"
#include "stringops.h"
#include "valid_hostname.h"

/* valid_hostname - screen out bad hostnames */

int     valid_hostname(const char *name)
{
    const char *cp;
    char   *str;
    int     ch;
    int     len;
    int     bad_val = 0;
    int     adjacent = 0;

#define DELIMITER(c) (c == '.' || c == '-')

    /*
     * Trivial cases first.
     */
    if (*name == 0) {
	msg_warn("valid_hostname: empty hostname");
	return (0);
    }

    /*
     * Find bad characters. Find adjacent delimiters.
     */
    for (cp = name; (ch = *cp) != 0; cp++) {
	if (DELIMITER(ch)) {
	    if (DELIMITER(cp[1]))
		adjacent = 1;
	} else if (!ISALNUM(ch) && ch != '_') {	/* grr.. */
	    if (bad_val == 0)
		bad_val = ch;
	}
    }

    /*
     * Before printing the name, validate its length.
     */
    if ((len = strlen(name)) > VALID_HOSTNAME_LEN) {
	str = printable(mystrdup(name), '?');
	msg_warn("valid_hostname: bad length %d for %.100s...", len, str);
	myfree(str);
	return (0);
    }

    /*
     * Report bad characters.
     */
    if (bad_val) {
	str = printable(mystrdup(name), '?');
	msg_warn("valid_hostname: invalid character %d(decimal) in %s",
		 bad_val, str);
	myfree(str);
	return (0);
    }

    /*
     * Misplaced delimiters.
     */
    if (DELIMITER(name[0]) || adjacent || DELIMITER(name[len - 1])) {
	msg_warn("valid_hostname: misplaced delimiter in %s", name);
	return (0);
    }
    return (1);
}

#ifdef TEST

 /*
  * Test program - reads hostnames from stdin, reports invalid hostnames to
  * stderr.
  */
#include <stdlib.h>

#include "vstring.h"
#include "vstream.h"
#include "vstring_vstream.h"
#include "msg_vstream.h"

int     main(int unused_argc, char **argv)
{
    VSTRING *buffer = vstring_alloc(1);

    msg_vstream_init(argv[0], VSTREAM_ERR);
    msg_verbose = 1;

    while (vstring_fgets_nonl(buffer, VSTREAM_IN))
	valid_hostname(vstring_str(buffer));
    exit(0);
}

#endif
