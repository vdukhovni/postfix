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
/*	void	ps_dnsbl_request(client_addr, callback, context)
/*	char	*client_addr;
/*	void	(*callback)(int, char *);
/*	char	*context;
/*
/*	int	ps_dnsbl_retrieve(client_addr, dnsbl_name)
/*	char	*client_addr;
/*	const char **dnsbl_name;
/* DESCRIPTION
/*	This module implements preliminary support for DNSBL lookups.
/*	Multiple requests for the same information are handled with
/*	reference counts.
/*
/*	ps_dnsbl_init() initializes this module, and must be called
/*	once before any of the other functions in this module.
/*
/*	ps_dnsbl_request() requests a blocklist score for the
/*	specified client IP address and increments the reference
/*	count.  The request completes in the background. The client
/*	IP address must be in inet_ntop(3) output format.  The
/*	callback argument specifies a function that is called when
/*	the requested result is available. The context is passed
/*	on to the callback function. The callback should ignore its
/*	first argument (it exists for compatibility with Postfix
/*	generic event infrastructure).
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
  * linked list under their DNSBL domain name. The list head has a reference
  * to a "safe name" for the DNSBL, in case the name includes a password.
  */
static HTABLE *dnsbl_site_cache;	/* indexed by DNSBNL domain */
static HTABLE_INFO **dnsbl_site_list;	/* flattened cache */

typedef struct {
    const char *safe_dnsbl;		/* from postscreen_dnsbl_reply_map */
    struct PS_DNSBL_SITE *first;	/* list of (filter, weight) tuples */
} PS_DNSBL_HEAD;

typedef struct PS_DNSBL_SITE {
    char   *filter;			/* reply filter (default: null) */
    int     weight;			/* reply weight (default: 1) */
    struct PS_DNSBL_SITE *next;		/* linked list */
} PS_DNSBL_SITE;

 /*
  * Per-client DNSBL scores.
  * 
  * Some SMTP clients make parallel connections. This can trigger parallel
  * blocklist score requests when the pre-handshake delays of the connections
  * overlap.
  * 
  * We combine requests for the same score under the client IP address in a
  * single reference-counted entry. The reference count goes up with each
  * request for a score, and it goes down with each score retrieval. Each
  * score has one or more requestors that need to be notified when the result
  * is ready, so that postscreen can terminate a pre-handshake delay when all
  * pre-handshake tests are completed.
  */
static HTABLE *dnsbl_score_cache;	/* indexed by client address */

typedef struct {
    void    (*callback) (int, char *);	/* generic call-back routine */
    char   *context;			/* generic call-back argument */
} PS_CALL_BACK_ENTRY;

typedef struct {
    const char *dnsbl;			/* one contributing DNSBL */
    int     total;			/* combined blocklist score */
    int     refcount;			/* score reference count */
    int     pending_lookups;		/* nr of DNS requests in flight */
    /* Call-back table support. */
    int     index;			/* next table index */
    int     limit;			/* last valid index */
    PS_CALL_BACK_ENTRY table[1];	/* actually a bunch */
} PS_DNSBL_SCORE;

#define PS_CALL_BACK_INIT(sp) do { \
	(sp)->limit = 0; \
	(sp)->index = 0; \
    } while (0)

#define PS_CALL_BACK_EXTEND(hp, sp) do { \
	if ((sp)->index >= (sp)->limit) { \
	    int _count_ = ((sp)->limit ? (sp)->limit * 2 : 5); \
	    (hp)->value = myrealloc((char *) (sp), sizeof(*(sp)) + \
				    _count_ * sizeof((sp)->table)); \
	    (sp) = (PS_DNSBL_SCORE *) (hp)->value; \
	    (sp)->limit = _count_; \
	} \
    } while (0)

#define PS_CALL_BACK_ENTER(sp, fn, ctx) do { \
	PS_CALL_BACK_ENTRY *_cb_ = (sp)->table + (sp)->index++; \
	_cb_->callback = (fn); \
	_cb_->context = (ctx); \
    } while (0)

#define PS_CALL_BACK_NOTIFY(sp, ev) do { \
	PS_CALL_BACK_ENTRY *_cb_; \
	for (_cb_ = (sp)->table; _cb_ < (sp)->table + (sp)->index; _cb_++) \
	    _cb_->callback((ev), _cb_->context); \
    } while (0)

#define PS_NULL_EVENT	(0)

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
    PS_DNSBL_HEAD *head;
    PS_DNSBL_SITE *new_site;
    char    junk;
    const char *weight_text;
    const char *pattern_text;
    int     weight;
    HTABLE_INFO *ht;

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

    if (msg_verbose > 1)
	msg_info("%s: \"%s\" -> domain=\"%s\" pattern=\"%s\" weight=%d",
		 myname, site, saved_site, pattern_text ? pattern_text :
		 "null", weight);

    /*
     * Look up or create the (filter, weight) list head for this DNSBL domain
     * name.
     */
    if ((head = (PS_DNSBL_HEAD *)
	 htable_find(dnsbl_site_cache, saved_site)) == 0) {
	head = (PS_DNSBL_HEAD *) mymalloc(sizeof(*head));
	ht = htable_enter(dnsbl_site_cache, saved_site, (char *) head);
	/* Translate the DNSBL name into a safe name if available. */
	if (ps_dnsbl_reply == 0
	  || (head->safe_dnsbl = dict_get(ps_dnsbl_reply, saved_site)) == 0)
	    head->safe_dnsbl = ht->key;
	head->first = 0;
    }

    /*
     * Append the new (filter, weight) node to the list for this DNSBL domain
     * name.
     */
    new_site = (PS_DNSBL_SITE *) mymalloc(sizeof(*new_site));
    new_site->filter = (pattern_text ? mystrdup(pattern_text) : 0);
    new_site->weight = weight;
    new_site->next = head->first;
    head->first = new_site;

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

int     ps_dnsbl_retrieve(const char *client_addr, const char **dnsbl_name)
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
    *dnsbl_name = score->dnsbl;
    score->refcount -= 1;
    if (score->refcount < 1) {
	if (msg_verbose > 1)
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
    PS_DNSBL_HEAD *head;
    PS_DNSBL_SITE *site;
    ARGV   *reply_argv;

    PS_CLEAR_EVENT_REQUEST(vstream_fileno(stream), ps_dnsbl_receive, context);

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
	if (msg_verbose > 1)
	    msg_info("%s: client=\"%s\" score=%d domain=\"%s\" reply=\"%s\"",
		     myname, STR(reply_client), score->total,
		     STR(reply_dnsbl), STR(reply_addr));
	head = (PS_DNSBL_HEAD *) htable_find(dnsbl_site_cache, STR(reply_dnsbl));
	site = (head ? head->first : (PS_DNSBL_SITE *) 0);
	for (reply_argv = 0; site != 0; site = site->next) {
	    if (site->filter == 0
		|| ps_dnsbl_match(site->filter, reply_argv ? reply_argv :
			 (reply_argv = argv_split(STR(reply_addr), " ")))) {
		score->dnsbl = head->safe_dnsbl;
		score->total += site->weight;
		if (msg_verbose > 1)
		    msg_info("%s: filter=\"%s\" weight=%d score=%d",
			     myname, site->filter ? site->filter : "null",
			     site->weight, score->total);
	    }
	}
	if (reply_argv != 0)
	    argv_free(reply_argv);

	/*
	 * Notify the requestor(s) that the result is ready to be picked up.
	 * If this call isn't made, clients have to sit out the entire
	 * pre-handshake delay.
	 */
	score->pending_lookups -= 1;
	if (score->pending_lookups == 0)
	    PS_CALL_BACK_NOTIFY(score, PS_NULL_EVENT);
    }
    vstream_fclose(stream);
}

/* ps_dnsbl_request  - send dnsbl query, increment reference count */

void    ps_dnsbl_request(const char *client_addr,
			         void (*callback) (int, char *),
			         char *context)
{
    const char *myname = "ps_dnsbl_request";
    int     fd;
    VSTREAM *stream;
    HTABLE_INFO **ht;
    PS_DNSBL_SCORE *score;
    HTABLE_INFO *hash_node;

    /*
     * Some spambots make several connections at nearly the same time,
     * causing their pregreet delays to overlap. Such connections can share
     * the efforts of DNSBL lookup.
     * 
     * We store a reference-counted DNSBL score under its client IP address. We
     * increment the reference count with each score request, and decrement
     * the reference count with each score retrieval.
     * 
     * Do not notify the requestor NOW when the DNS replies are already in.
     * Reason: we must not make a backwards call while we are still in the
     * middle of executing the corresponding forward call. Instead we create
     * a zero-delay timer request and call the notification function from
     * there.
     * 
     * ps_dnsbl_request() could instead return a result value to indicate that
     * the DNSBL score is already available, but that would complicate the
     * caller with two different notification code paths: one asynchronous
     * code path via the callback invocation, and one synchronous code path
     * via the ps_dnsbl_request() result value. That would be a source of
     * future bugs.
     */
    if ((hash_node = htable_locate(dnsbl_score_cache, client_addr)) != 0) {
	score = (PS_DNSBL_SCORE *) hash_node->value;
	score->refcount += 1;
	PS_CALL_BACK_EXTEND(hash_node, score);
	PS_CALL_BACK_ENTER(score, callback, context);
	if (msg_verbose > 1)
	    msg_info("%s: reuse blocklist score for %s refcount=%d pending=%d",
		     myname, client_addr, score->refcount,
		     score->pending_lookups);
	if (score->pending_lookups == 0)
	    event_request_timer(callback, context, EVENT_NULL_DELAY);
	return;
    }
    if (msg_verbose > 1)
	msg_info("%s: create blocklist score for %s", myname, client_addr);
    score = (PS_DNSBL_SCORE *) mymalloc(sizeof(*score));
    score->total = 0;
    score->refcount = 1;
    score->pending_lookups = 0;
    PS_CALL_BACK_INIT(score);
    PS_CALL_BACK_ENTER(score, callback, context);
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
	    continue;
	}
	stream = vstream_fdopen(fd, O_RDWR);
	attr_print(stream, ATTR_FLAG_NONE,
		   ATTR_TYPE_STR, MAIL_ATTR_RBL_DOMAIN, ht[0]->key,
		   ATTR_TYPE_STR, MAIL_ATTR_ACT_CLIENT_ADDR, client_addr,
		   ATTR_TYPE_END);
	if (vstream_fflush(stream) != 0) {
	    msg_warn("%s: error sending to " DNSBL_SERVICE " service: %m", myname);
	    vstream_fclose(stream);
	    continue;
	}
	PS_READ_EVENT_REQUEST(vstream_fileno(stream), ps_dnsbl_receive,
			      (char *) stream, DNSBLOG_TIMEOUT);
	score->pending_lookups += 1;
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
