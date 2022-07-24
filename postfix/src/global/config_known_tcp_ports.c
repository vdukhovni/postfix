/*++
/* NAME
/*	config_known_tcp_ports 3
/* SUMMARY
/*	parse and store known TCP port configuration
/* SYNOPSIS
/*	#include <config_known_tcp_ports.h>
/*
/*	void	config_known_tcp_ports(
/*	const char *source,
/*	const char *settings);
/* DESCRIPTION
/*	config_known_tcp_ports() parses the known TCP port information
/*	in the settings argument, and reports any warnings to the standard
/*	error stream. The source argument is used to provide warning
/*	context. It typically is a configuration parameter name.
/* .SH EXPECTED SYNTAX (ABNF)
/*	configuration = empty | name-to-port *("," name-to-port)
/*	name-to-port = 1*(name "=") port
/* SH EXAMPLES
/*	In the example below, the whitespace is optional.
/*	smtp = 25, smtps = submissions = 465, submission = 587
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
#include <argv.h>
#include <known_tcp_ports.h>
#include <msg.h>
#include <mymalloc.h>
#include <stringops.h>

 /*
  * Application-specific.
  */
#include <config_known_tcp_ports.h>

/* config_known_tcp_ports - parse configuration and store associations */

void    config_known_tcp_ports(const char *source, const char *settings)
{
    ARGV   *associations;
    ARGV   *association;
    char  **cpp;

    clear_known_tcp_ports();

    /*
     * The settings is in the form of associations separated by comma. Split
     * it into separate associations.
     */
    associations = argv_split(settings, ",");
    if (associations->argc == 0) {
	argv_free(associations);
	return;
    }

    /*
     * Each association is in the form of "1*(name =) port". We use
     * argv_split() to carve this up, then we use mystrtok() to validate the
     * individual fragments. But first we prepend and append space so that we
     * get sensible results when an association starts or ends in "=".
     */
    for (cpp = associations->argv; *cpp != 0; cpp++) {
	char   *temp = concatenate(" ", *cpp, " ", (char *) 0);

	association = argv_split_at(temp, '=');
	myfree(temp);

	if (association->argc == 0) {
	     /* empty, ignore */ ;
	} else if (association->argc == 1) {
	    msg_warn("%s: in \"%s\" is not in \"name = value\" form",
		     source, *cpp);
	} else {
	    char   *bp;
	    char   *lhs;
	    char   *rhs;
	    const char *err = 0;
	    int     n;

	    bp = association->argv[association->argc - 1];
	    if ((rhs = mystrtok(&bp, CHARS_SPACE)) == 0) {
		err = "missing port value after \"=\"";
	    } else if (mystrtok(&bp, CHARS_SPACE) != 0) {
		err = "whitespace in port number";
	    } else {
		for (n = 0; n < association->argc - 1; n++) {
		    const char *new_err;

		    bp = association->argv[n];
		    if ((lhs = mystrtok(&bp, CHARS_SPACE)) == 0) {
			new_err = "missing service name before \"=\"";
		    } else if (mystrtok(&bp, CHARS_SPACE) != 0) {
			new_err = "whitespace in service name";
		    } else {
			new_err = add_known_tcp_port(lhs, rhs);
		    }
		    if (new_err != 0 && err == 0)
			err = new_err;
		}
	    }
	    if (err != 0) {
		msg_warn("%s: in \"%s\": %s", source, *cpp, err);
	    }
	}
	argv_free(association);
    }
    argv_free(associations);
}
