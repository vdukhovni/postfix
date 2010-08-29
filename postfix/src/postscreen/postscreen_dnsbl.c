/*++
/* NAME
/*	postscreen_dnsbl 3
/* SUMMARY
/*	postscreen DNSBL support
/* SYNOPSIS
/*	#include <postscreen.h>
/*
/*	void	ps_dnsbl_init(void)
/*
/*	void	ps_dnsbl_request(client_addr)
/*	char	*client_addr;
/*
/*	int	ps_dnsbl_retrieve(client_addr)
/*	char	*client_addr;
/* DESCRIPTION
/*	This module implements preliminary support for DNSBL lookups
/*	that complete in the background. Multiple requests for the
/*	same information are handled with reference counts.
/*
/*	ps_dnsbl_init() initializes this module, and must be called
/*	once before any of the other functions in this module.
/*
/*	ps_dnsbl_request() requests a blocklist score for the specified
/*	client IP address and increments the reference count. The
/*	client IP address must be in inet_ntop(3) output format.
/*
/*	ps_dnsbl_retrieve() retrieves the result score requested with
/*	ps_dnsbl_request() and decrements the reference count. It
/*	is an error to retrieve a score without requesting it first.
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
#include <stdio.h>			/* sscanf */

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <argv.h>
#include <htable.h>
#include <events.h>
#include <vstream.h>
#include <connect.h>
#include <split_at.h>
#include <valid_hostname.h>

/* Global library. */

#include <mail_params.h>
#include <mail_proto.h>

/* Application-specific. */

#include <postscreen.h>

#define DNSBL_SERVICE			"dnsblog"
#define DNSBLOG_TIMEOUT			10

 /*
  * Per-DNSBL filters and weights.
  * 
  * The postscreen_dnsbl_sites parameter specifies zero or more DNSBL domains.
  * We provide multiple access methods, one for quick iteration when sending
  * queries to all DNSBL servers, and one for quick location when receiving a
  * reply from one DNSBL server.
  * 
  * Each DNSBL domain can be specified more than once, each time with a
  * different (filter, weight) pair. We group (filter, weight) pairs in a
  * linked list under their DNSBL domain name.
  */
static HTABLE *dnsbl_site_cache;	/* indexed by DNSBNL domain */
static HTABLE_INFO **dnsbl_site_list;	/* flattened cache */

typedef struct PS_DNSBL_SITE {
    char   *filter;			/* reply filter (default: null) */
    int     weight;			/* reply weight (default: 1) */
    struct PS_DNSBL_SITE *next;		/* linked list */
} PS_DNSBL_SITE;

 /*
  * Per-client DNSBL scores.
  * 
  * One remote SMTP client may make parallel connections and thereby trigger
  * parallel blocklist score requests. We combine identical requests under
  * the client IP address in a single reference-counted entry. The reference
  * count goes up with each score request, and it goes down with each score
  * retrieval.
  */
static HTABLE *dnsbl_score_cache;	/* indexed by client address */

typedef struct {
    int     total;			/* combined blocklist score */
    int     refcount;			/* score reference count */
} PS_DNSBL_SCORE;

 /*
  * Per-request state.
  * 
  * This implementation stores the client IP address and DNSBL domain in the
  * DNSBLOG query/reply stream. This simplifies code, and allows the DNSBLOG
  * server to produce more informative logging.
  */
static VSTRING *reply_client;		/* client address in DNSBLOG reply */
static VSTRING *reply_dnsbl;		/* domain in DNSBLOG reply */
static VSTRING *reply_addr;		/* adress list in DNSBLOG reply */

/* ps_dnsbl_add_site - add DNSBL site information */

static void ps_dnsbl_add_site(const char *site)
{
    const char *myname = "ps_dnsbl_add_site";
    char   *saved_site = mystrdup(site);
    PS_DNSBL_SITE *old_site;
    PS_DNSBL_SITE *new_site;
    char    junk;
    const char *weight_text;
    const char *pattern_text;
    int     weight;

    /*
     * Parse the required DNSBL domain name, the optional reply filter and
     * the optional reply weight factor.
     */
#define DO_GRIPE	1

    /* Negative weight means whitelist. */
    if ((weight_text = split_at(saved_site, '*')) != 0) {
	if (sscanf(weight_text, "%d%c", &weight, &junk) != 1)
	    msg_fatal("bad DNSBL weight factor \"%s\" in \"%s\"",
		      weight_text, site);
    } else {
	weight = 1;
    }
    /* Preliminary fixed-string filter. */
    if ((pattern_text = split_at(saved_site, '=')) != 0) {
	if (valid_ipv4_hostaddr(pattern_text, DO_GRIPE) == 0)
	    msg_fatal("bad DNSBL filter syntax \"%s\" in \"%s\"",
		      pattern_text, site);
    }
    if (valid_hostname(saved_site, DO_GRIPE) == 0)
	msg_fatal("bad DNSBL domain name \"%s\" in \"%s\"",
		  saved_site, site);

    if (msg_verbose)
	msg_info("%s: \"%s\" -> domain=\"%s\" pattern=\"%s\" weight=%d",
		 myname, site, saved_site, pattern_text ? pattern_text :
		 "null", weight);

    /*
     * Add a new node for this DNSBL domain name. One DNSBL domain name can
     * be specified multiple times with different filters and weights. These
     * are stored as a linked list under the DNSBL domain name.
     */
    new_site = (PS_DNSBL_SITE *) mymalloc(sizeof(*new_site));
    new_site->filter = (pattern_text ? mystrdup(pattern_text) : 0);
    new_site->weight = weight;

    if ((old_site = (PS_DNSBL_SITE *)
	 htable_find(dnsbl_site_cache, saved_site)) != 0) {
	new_site->next = old_site->next;
	old_site->next = new_site;
    } else {
	(void) htable_enter(dnsbl_site_cache, saved_site, (char *) new_site);
	new_site->next = 0;
    }
    myfree(saved_site);
}

/* ps_dnsbl_match - match DNSBL reply filter */

static int ps_dnsbl_match(const char *filter, ARGV *reply)
{
    char  **cpp;

    /*
     * Preliminary fixed-string implementation.
     */
    for (cpp = reply->argv; *cpp != 0; cpp++)
	if (strcmp(filter, *cpp) == 0)
	    return (1);
    return (0);
}

/* ps_dnsbl_retrieve - retrieve blocklist score, decrement reference count */

int     ps_dnsbl_retrieve(const char *client_addr)
{
    const char *myname = "ps_dnsbl_retrieve";
    PS_DNSBL_SCORE *score;
    int     result_score;

    /*
     * Sanity check.
     */
    if ((score = (PS_DNSBL_SCORE *)
	 htable_find(dnsbl_score_cache, client_addr)) == 0)
	msg_panic("%s: no blocklist score for %s", myname, client_addr);

    /*
     * Reads are destructive.
     */
    result_score = score->total;
    score->refcount -= 1;
    if (score->refcount < 1) {
	if (msg_verbose)
	    msg_info("%s: delete blocklist score for %s", myname, client_addr);
	htable_delete(dnsbl_score_cache, client_addr, myfree);
    }
    return (result_score);
}

/* ps_dnsbl_receive - receive DNSBL reply, update blocklist score */

static void ps_dnsbl_receive(int event, char *context)
{
    const char *myname = "ps_dnsbl_receive";
    VSTREAM *stream = (VSTREAM *) context;
    PS_DNSBL_SCORE *score;
    PS_DNSBL_SITE *site;
    ARGV   *reply_argv;

    CLEAR_EVENT_REQUEST(vstream_fileno(stream), ps_dnsbl_receive, context);

    /*
     * Receive the DNSBL lookup result.
     * 
     * This is preliminary code to explore the field. Later, DNSBL lookup will
     * be handled by an UDP-based DNS client that is built directly into some
     * Postfix daemon.
     * 
     * Don't bother looking up the blocklist score when the client IP address is
     * not listed at the DNSBL.
     * 
     * Don't panic when the blocklist score no longer exists. It may be deleted
     * when the client triggers a "drop" action after pregreet, when the
     * client does not pregreet and the DNSBL reply arrives late, or when the
     * client triggers a "drop" action after hanging up.
     */
    if (event == EVENT_READ
	&& attr_scan(stream,
		     ATTR_FLAG_MORE | ATTR_FLAG_STRICT,
		     ATTR_TYPE_STR, MAIL_ATTR_RBL_DOMAIN, reply_dnsbl,
		     ATTR_TYPE_STR, MAIL_ATTR_ACT_CLIENT_ADDR, reply_client,
		     ATTR_TYPE_STR, MAIL_ATTR_RBL_ADDR, reply_addr,
		     ATTR_TYPE_END) == 3
	&& *STR(reply_addr) != 0
	&& (score = (PS_DNSBL_SCORE *)
	    htable_find(dnsbl_score_cache, STR(reply_client))) != 0) {

	/*
	 * Run this response past all applicable DNSBL filters and update the
	 * blocklist score for this client IP address.
	 * 
	 * Don't panic when the DNSBL domain name is not found. The DNSBLOG
	 * server may be messed up.
	 */
	if (msg_verbose)
	    msg_info("%s: client=\"%s\" score=%d domain=\"%s\" reply=\"%s\"",
		     myname, STR(reply_client), score->total,
		     STR(reply_dnsbl), STR(reply_addr));
	for (reply_argv = 0, site = (PS_DNSBL_SITE *)
	     htable_find(dnsbl_site_cache, STR(reply_dnsbl));
	     site != 0; site = site->next) {
	    if (site->filter == 0
		|| ps_dnsbl_match(site->filter, reply_argv ? reply_argv :
			 (reply_argv = argv_split(STR(reply_addr), " ")))) {
		score->total += site->weight;
		if (msg_verbose)
		    msg_info("%s: filter=\"%s\" weight=%d score=%d",
			     myname, site->filter ? site->filter : "null",
			     site->weight, score->total);
	    }
	}
	if (reply_argv != 0)
	    argv_free(reply_argv);
    }
    vstream_fclose(stream);
}

/* ps_dnsbl_request  - send dnsbl query, increment reference count */

void    ps_dnsbl_request(const char *client_addr)
{
    const char *myname = "ps_dnsbl_request";
    int     fd;
    VSTREAM *stream;
    HTABLE_INFO **ht;
    PS_DNSBL_SCORE *score;

    /*
     * Avoid duplicate effort when this lookup is already in progress. We
     * store a reference-counted DNSBL score under its client IP address. We
     * increment the reference count with each request, and decrement the
     * reference count with each retrieval.
     */
    if ((score = (PS_DNSBL_SCORE *)
	 htable_find(dnsbl_score_cache, client_addr)) != 0) {
	score->refcount += 1;
	return;
    }
    if (msg_verbose)
	msg_info("%s: create blocklist score for %s", myname, client_addr);
    score = (PS_DNSBL_SCORE *) mymalloc(sizeof(*score));
    score->total = 0;
    score->refcount = 1;
    (void) htable_enter(dnsbl_score_cache, client_addr, (char *) score);

    /*
     * Send a query to all DNSBL servers. Later, DNSBL lookup will be done
     * with an UDP-based DNS client that is built directly into Postfix code.
     * We therefore do not optimize the maximum out of this temporary
     * implementation.
     */
    for (ht = dnsbl_site_list; *ht; ht++) {
	if ((fd = LOCAL_CONNECT("private/" DNSBL_SERVICE, NON_BLOCKING, 1)) < 0) {
	    msg_warn("%s: connect to " DNSBL_SERVICE " service: %m", myname);
	    return;
	}
	stream = vstream_fdopen(fd, O_RDWR);
	attr_print(stream, ATTR_FLAG_NONE,
		   ATTR_TYPE_STR, MAIL_ATTR_RBL_DOMAIN, ht[0]->key,
		   ATTR_TYPE_STR, MAIL_ATTR_ACT_CLIENT_ADDR, client_addr,
		   ATTR_TYPE_END);
	if (vstream_fflush(stream) != 0) {
	    msg_warn("%s: error sending to " DNSBL_SERVICE " service: %m", myname);
	    vstream_fclose(stream);
	    return;
	}
	READ_EVENT_REQUEST(vstream_fileno(stream), ps_dnsbl_receive,
			   (char *) stream, DNSBLOG_TIMEOUT);
    }
}

/* ps_dnsbl_init - initialize */

void    ps_dnsbl_init(void)
{
    const char *myname = "ps_dnsbl_init";
    ARGV   *dnsbl_site = argv_split(var_ps_dnsbl_sites, ", \t\r\n");
    char  **cpp;

    /*
     * Sanity check.
     */
    if (dnsbl_site_cache != 0)
	msg_panic("%s: called more than once", myname);

    /*
     * Prepare for quick iteration when sending out queries to all DNSBL
     * servers, and for quick lookup when a reply arrives from a specific
     * DNSBL server.
     */
    dnsbl_site_cache = htable_create(13);
    for (cpp = dnsbl_site->argv; *cpp; cpp++)
	ps_dnsbl_add_site(*cpp);
    argv_free(dnsbl_site);
    dnsbl_site_list = htable_list(dnsbl_site_cache);

    /*
     * The per-client blocklist score.
     */
    dnsbl_score_cache = htable_create(13);

    /*
     * Space for ad-hoc DNSBLOG server request/reply parameters.
     */
    reply_client = vstring_alloc(100);
    reply_dnsbl = vstring_alloc(100);
    reply_addr = vstring_alloc(100);
}
