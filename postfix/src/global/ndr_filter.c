/*++
/* NAME
/*	ndr_filter 3
/* SUMMARY
/*	bounce or defer NDR filter
/* SYNOPSIS
/*	#include <ndr_filter.h>
/*
/*	NDR_FILTER *ndr_filter_create(
/*	const char *title,
/*	const char *map_names)
/*
/*	DSN	*ndr_filter_lookup(
/*	NDR_FILTER *fp,
/*	DSN	dsn)
/*
/*	void	dsn_free(
/*	NDR_FILTER *fp)
/* DESCRIPTION
/*	This module maps a bounce or defer non-delivery status code
/*	and text into a bounce or defer non-delivery status code
/*	and text. The other DSN attributes are passed through without
/*	modification.
/*
/*	ndr_filter_create() instantiates a bounce or defer NDR filter.
/*
/*	ndr_filter_lookup() queries the specified filter. The DSN
/*	must be a bounce or defer DSN. If a match is found and the
/*	result is properly formatted, the result value must specify
/*	a bounce or defer DSN. The result is in part overwritten
/*	upon each call, and is in part a shallow copy of the dsn
/*	argument.  The result is a null pointer when no valid match
/*	is found. This function must not be called with the result
/*	from a ndr_filter_lookup() call.
/*
/*	dsn_free() destroys the specified NDR filter.
/*
/*	Arguments:
/* .IP title
/*	Origin of the mapnames argument, typically a configuration
/*	parameter name. This is reported in diagnostics.
/* .IP mapnames
/*	List of lookup tables, separated by whitespace or comma.
/* .IP fp
/*	filter created with ndr_filter_create()
/* .IP dsn
/*	A bounce or defer DSN data structure. The ndr_filter_lookup()
/*	result value is in part a shallow copy of this argument.
/* SEE ALSO
/*	maps(3) multi-table search
/* DIAGNOSTICS
/*	Panic: invalid dsn argument; recursive call. Fatal error:
/*	memory allocation problem. Warning: invalid DSN lookup
/*	result.
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

 /*
  * System libraries.
  */
#include <sys_defs.h>

 /*
  * Utility library.
  */
#include <msg.h>
#include <mymalloc.h>
#include <vstring.h>

 /*
  * Global library.
  */
#include <maps.h>
#include <dsn.h>
#include <dsn_util.h>
#include <maps.h>
#include <ndr_filter.h>

 /*
  * Private data structure.
  */
struct NDR_FILTER {
    MAPS   *maps;			/* Replacement (status, text) */
    VSTRING *buffer;			/* Status code and text */
    DSN_SPLIT dp;			/* Parsing aid */
    DSN     dsn;			/* Shallow copy */
};

 /*
  * SLMs.
  */
#define STR(x) vstring_str(x)

/* ndr_filter_create - create bounce/defer NDR filter */

NDR_FILTER *ndr_filter_create(const char *title, const char *map_names)
{
    const char myname[] = "ndr_filter_create";
    NDR_FILTER *fp;

    if (msg_verbose)
	msg_info("%s: %s %s", myname, title, map_names);

    fp = (NDR_FILTER *) mymalloc(sizeof(*fp));
    fp->buffer = vstring_alloc(100);
    fp->maps = maps_create(title, map_names, DICT_FLAG_LOCK);
    return (fp);
}

/* ndr_filter_lookup - apply bounce/defer NDR filter */

DSN    *ndr_filter_lookup(NDR_FILTER *fp, DSN *dsn)
{
    const char myname[] = "ndr_filter_lookup";
    const char *result;

    if (msg_verbose)
	msg_info("%s: %s %s", myname, dsn->status, dsn->reason);

#define IS_NDR_DSN(s) \
	(dsn_valid(s) && (s)[1] == '.' && ((s)[0] == '4' || (s)[0] == '5'))

    /*
     * Sanity check. We filter only bounce/defer DSNs.
     */
    if (!IS_NDR_DSN(dsn->status))
	msg_panic("%s: dsn argument with bad status code: %s",
		  myname, dsn->status);

    /*
     * Sanity check. An NDR filter must not be invoked with its own result.
     */
    if (dsn->reason == fp->dsn.reason)
	msg_panic("%s: recursive call is not allowed", myname);

    /*
     * Look up replacement status and text.
     */
    vstring_sprintf(fp->buffer, "%s %s", dsn->status, dsn->reason);
    if ((result = maps_find(fp->maps, STR(fp->buffer), 0)) != 0) {
	/* Sanity check. We accept only bounce/defer DSNs. */
	if (!IS_NDR_DSN(result)) {
	    msg_warn("%s: bad status code: %s", fp->maps->title, result);
	    return (0);
	} else {
	    vstring_strcpy(fp->buffer, result);
	    dsn_split(&fp->dp, "can't happen", STR(fp->buffer));
	    (void) DSN_ASSIGN(&fp->dsn, DSN_STATUS(fp->dp.dsn),
			      (result[0] == '4' ? "delayed" : "failed"),
			      fp->dp.text, dsn->dtype, dsn->dtext,
			      dsn->mtype, dsn->mname);
	    return (&fp->dsn);
	}
    }
    return (0);
}

/* ndr_filter_free - destroy bounce/defer NDR filter */

void    ndr_filter_free(NDR_FILTER *fp)
{
    const char myname[] = "ndr_filter_free";

    if (msg_verbose)
	msg_info("%s: %s", myname, fp->maps->title);

    maps_free(fp->maps);
    vstring_free(fp->buffer);
    myfree((char *) fp);
}
