/*++
/* NAME
/*	known_tcp_ports 3
/* SUMMARY
/*	reduce dependency on the services(5) database
/* SYNOPSIS
/*	#include <known_tcp_ports.h>
/*
/*	const char *add_known_tcp_port(
/*	const char *name)
/*	const char *port)
/*
/*	const char *filter_known_tcp_port(
/*	const char *name_or_port)
/*
/*	void    clear_known_tcp_ports(void)
/* AUXILIARY FUNCTIONS
/*	char	*export_known_tcp_ports(
/*	VSTRING *result)
/* DESCRIPTION
/*	This module reduces dependency on the services(5) database.
/*
/*	add_known_tcp_port() associates a symbolic name with a numerical
/*	port. The function returns a pointer to error text if the
/*	arguments are malformed or if the symbolic name already has
/*	an association.
/*
/*	filter_known_tcp_port() returns the argument if it does not
/*	specify a symbolic name, or if the argument specifies a symbolic
/*	name that is not associated with a numerical port.  Otherwise,
/*	it returns the associated numerical port.
/*
/*	clear_known_tcp_ports() destroys all name-number associations.
/*	string.
/*
/*	export_known_tcp_ports() overwrites a VSTRING with all known
/*	name=port associations, sorted by service name, and separated
/*	by whitespace. The result is pointer to the VSTRING payload.
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
  * System library
  */
#include <sys_defs.h>
#include <stdlib.h>
#include <string.h>

 /*
  * Utility library
  */
#include <htable.h>
#include <mymalloc.h>
#include <stringops.h>

 /*
  * Application-specific.
  */
#include <known_tcp_ports.h>

#define STR(x)	vstring_str(x)

static HTABLE *known_tcp_ports;

/* add_known_tcp_port - associate symbolic name with numerical port */

const char *add_known_tcp_port(const char *name, const char *port)
{
    if (alldig(name))
	return ("numerical service name");
    if (!alldig(port))
	return ("non-numerical service port");
    if (strlen(port) > 5 || (strlen(port) == 5 && strcmp(port, "65535") > 0))
	return ("port number out of range");
    if (known_tcp_ports == 0)
	known_tcp_ports = htable_create(10);
    if (htable_locate(known_tcp_ports, name) != 0)
	return ("duplicate service name");
    (void) htable_enter(known_tcp_ports, name, mystrdup(port));
    return (0);
}

/* filter_known_tcp_port - replace argument if associated with known port */

const char *filter_known_tcp_port(const char *name_or_port)
{
    HTABLE_INFO *ht;

    if (name_or_port == 0 || known_tcp_ports == 0 || alldig(name_or_port)) {
	return (name_or_port);
    } else if ((ht = htable_locate(known_tcp_ports, name_or_port)) != 0) {
	return (ht->value);
    } else {
	return (name_or_port);
    }
}

/* clear_known_tcp_ports - destroy all name-port associations */

void    clear_known_tcp_ports(void)
{
    htable_free(known_tcp_ports, myfree);
    known_tcp_ports = 0;
}

/* compare_ht_keys - compare table keys */

static int compare_ht_keys(const void *a, const void *b)
{
    HTABLE_INFO **ap = (HTABLE_INFO **) a;
    HTABLE_INFO **bp = (HTABLE_INFO **) b;

    return (strcmp((const char *) ap[0]->key, (const char *) bp[0]->key));
}

/* export_known_tcp_ports - sorted dump */

char   *export_known_tcp_ports(VSTRING *out)
{
    HTABLE_INFO **list;
    HTABLE_INFO **ht;

    VSTRING_RESET(out);
    if (known_tcp_ports) {
	list = htable_list(known_tcp_ports);
	qsort((void *) list, known_tcp_ports->used, sizeof(*list),
	      compare_ht_keys);
	for (ht = list; *ht; ht++)
	    vstring_sprintf_append(out, "%s%s=%s", ht > list ? " " : "",
				   ht[0]->key, (const char *) ht[0]->value);
	myfree((void *) list);
    }
    VSTRING_TERMINATE(out);
    return (STR(out));
}
