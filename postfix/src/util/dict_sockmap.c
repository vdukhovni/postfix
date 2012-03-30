/*++
/* NAME
/*	dict_sockmap 3
/* SUMMARY
/*	dictionary manager interface to Sendmail-style socketmap server
/* SYNOPSIS
/*	#include <dict_sockmap.h>
/*
/*	DICT	*dict_sockmap_open(map, open_flags, dict_flags)
/*	const char *map;
/*	int	open_flags;
/*	int	dict_flags;
/* DESCRIPTION
/*	dict_sockmap_open() makes a Sendmail-style socketmap server
/*	accessible via the generic dictionary operations described
/*	in dict_open(3).  The only implemented operation is dictionary
/*	lookup. This map type can be useful for simulating a dynamic
/*	lookup table.
/*
/*	Postfix socketmap names have the form inet:host:port:socketmap-name
/*	or unix:pathname:socketmap-name, where socketmap-name
/*	specifies the socketmap name that the socketmap server uses.
/* PROTOCOL
/* .ad
/* .fi
/*	The socketmap class implements a simple protocol: the client
/*	sends one request, and the server sends one reply.
/* ENCODING
/* .ad
/* .fi
/*	Each request and reply are sent as one netstring object.
/* REQUEST FORMAT
/* .ad
/* .fi
/* .IP "<mapname> <space> <key>"
/*	Search the specified socketmap under the specified key.
/* REPLY FORMAT
/* .ad
/* .fi
/*	Replies must be no longer than 100000 characters (not including
/*	the netstring encapsulation), and must have the following
/*	form:
/* .IP "OK <space> <data>"
/*	The requested data was found.
/* .IP "NOTFOUND <space>"
/*	The requested data was not found.
/* .IP "TEMP <space> <reason>"
/* .IP "TIMEOUT <space> <reason>"
/* .IP "PERM <space> <reason>"
/*	The request failed. The reason, if non-empty, is descriptive
/*	text.
/* SECURITY
/*	This map cannot be used for security-sensitive information,
/*	because neither the connection nor the server are authenticated.
/* SEE ALSO
/*	dict(3) generic dictionary manager
/*	netstring(3) netstring stream I/O support
/* DIAGNOSTICS
/*	Fatal errors: out of memory, unknown host or service name,
/*	attempt to update or iterate over map.
/* BUGS
/*	The protocol limits are not yet configurable.
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
  * System library.
  */
#include <sys_defs.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

 /*
  * Utility library.
  */
#include <mymalloc.h>
#include <msg.h>
#include <vstream.h>
#include <auto_clnt.h>
#include <netstring.h>
#include <split_at.h>
#include <stringops.h>
#include <dict_sockmap.h>

 /*
  * Socket map data structure.
  */
typedef struct {
    DICT    dict;			/* parent class */
    char   *sockmap_name;		/* on-the-wire socketmap name */
    VSTRING *rdwr_buf;			/* read/write buffer */
} DICT_SOCKMAP;

 /*
  * Default limits.
  */
#define DICT_SOCKMAP_DEF_TIMEOUT	100	/* connect/read/write timeout */
#define DICT_SOCKMAP_DEF_MAX_REPLY	100000	/* reply size limit */
#define DICT_SOCKMAP_DEF_MAX_IDLE	10	/* close idle socket */
#define DICT_SOCKMAP_DEF_MAX_TTL	100	/* close old socket */

 /*
  * Class variables.
  */
static AUTO_CLNT *dict_sockmap_clnt;	/* auto_clnt handle */
static int dict_sockmap_refcount;	/* handle reference count */
static int dict_sockmap_timeout = DICT_SOCKMAP_DEF_TIMEOUT;
static int dict_sockmap_max_reply = DICT_SOCKMAP_DEF_MAX_REPLY;
static int dict_sockmap_max_idle = DICT_SOCKMAP_DEF_MAX_IDLE;
static int dict_sockmap_max_ttl = DICT_SOCKMAP_DEF_MAX_TTL;

 /*
  * Socketmap protocol elements.
  */
#define DICT_SOCKMAP_PROT_OK		"OK"
#define DICT_SOCKMAP_PROT_NOTFOUND	"NOTFOUND"
#define DICT_SOCKMAP_PROT_TEMP		"TEMP"
#define DICT_SOCKMAP_PROT_TIMEOUT	"TIMEOUT"
#define DICT_SOCKMAP_PROT_PERM		"PERM"

 /*
  * SLMs.
  */
#define STR(x)	vstring_str(x)
#define LEN(x)	VSTRING_LEN(x)

/* dict_sockmap_lookup - socket map lookup */

static const char *dict_sockmap_lookup(DICT *dict, const char *key)
{
    const char *myname = "dict_sockmap_lookup";
    DICT_SOCKMAP *dp = (DICT_SOCKMAP *) dict;
    VSTREAM *fp;
    int     netstring_err;
    char   *reply_payload;
    int     except_count;
    const char *error_class;

    if (msg_verbose)
	msg_info("%s: key %s", myname, key);

    /*
     * Optionally fold the key.
     */
    if (dict->flags & DICT_FLAG_FOLD_MUL) {
	if (dict->fold_buf == 0)
	    dict->fold_buf = vstring_alloc(100);
	vstring_strcpy(dict->fold_buf, key);
	key = lowercase(STR(dict->fold_buf));
    }

    /*
     * We retry connection-level errors once, to make server restarts
     * transparent.
     */
    for (except_count = 0; /* see below */ ; except_count++) {

	/*
	 * Look up the stream.
	 */
	if ((fp = auto_clnt_access(dict_sockmap_clnt)) == 0) {
	    msg_warn("table %s:%s lookup error: %m", dict->type, dict->name);
	    dict->error = DICT_ERR_RETRY;
	    return (0);
	}

	/*
	 * Set up an exception handler.
	 */
	netstring_setup(fp, dict_sockmap_timeout);
	if ((netstring_err = vstream_setjmp(fp)) == 0) {

	    /*
	     * Send the query. This may raise an exception.
	     */
	    vstring_sprintf(dp->rdwr_buf, "%s %s", dp->sockmap_name, key);
	    NETSTRING_PUT_BUF(fp, dp->rdwr_buf);

	    /*
	     * Receive the response. This may raise an exception.
	     */
	    netstring_get(fp, dp->rdwr_buf, dict_sockmap_max_reply);

	    /*
	     * If we got here, then no exception was raised.
	     */
	    break;
	}

	/*
	 * Handle exceptions.
	 */
	else {

	    /*
	     * We retry a broken connection only once.
	     */
	    if (except_count == 0 && netstring_err == NETSTRING_ERR_EOF
		&& errno != ETIMEDOUT) {
		auto_clnt_recover(dict_sockmap_clnt);
		continue;
	    }

	    /*
	     * We do not retry other errors.
	     */
	    else {
		msg_warn("table %s:%s lookup error: %s",
			 dict->type, dict->name,
			 netstring_strerror(netstring_err));
		dict->error = DICT_ERR_RETRY;
		return (0);
	    }
	}
    }

    /*
     * Parse the reply.
     */
    VSTRING_TERMINATE(dp->rdwr_buf);
    reply_payload = split_at(STR(dp->rdwr_buf), ' ');
    if (strcmp(STR(dp->rdwr_buf), DICT_SOCKMAP_PROT_OK) == 0) {
	dict->error = 0;
	return (reply_payload);
    } else if (strcmp(STR(dp->rdwr_buf), DICT_SOCKMAP_PROT_NOTFOUND) == 0) {
	dict->error = 0;
	return (0);
    }
    /* We got no definitive reply. */
    if (strcmp(STR(dp->rdwr_buf), DICT_SOCKMAP_PROT_TEMP) == 0) {
	error_class = "temporary";
	dict->error = DICT_ERR_RETRY;
    } else if (strcmp(STR(dp->rdwr_buf), DICT_SOCKMAP_PROT_TIMEOUT) == 0) {
	error_class = "timeout";
	dict->error = DICT_ERR_RETRY;
    } else if (strcmp(STR(dp->rdwr_buf), DICT_SOCKMAP_PROT_PERM) == 0) {
	error_class = "permanent";
	dict->error = DICT_ERR_CONFIG;
    } else {
	error_class = "unknown";
	dict->error = DICT_ERR_RETRY;
    }
    while (reply_payload && ISSPACE(*reply_payload))
	reply_payload++;
    msg_warn("%s:%s socketmap server %s error%s%.200s",
	     dict->type, dict->name, error_class,
	     reply_payload && *reply_payload ? ": " : "",
	     reply_payload && *reply_payload ?
	     printable(reply_payload, '?') : "");
    return (0);
}

/* dict_sockmap_close - close socket map */

static void dict_sockmap_close(DICT *dict)
{
    DICT_SOCKMAP *dp = (DICT_SOCKMAP *) dict;

    vstring_free(dp->rdwr_buf);
    myfree(dp->sockmap_name);
    if (--dict_sockmap_refcount == 0) {
	auto_clnt_free(dict_sockmap_clnt);
	dict_sockmap_clnt = 0;
    }
    if (dict->fold_buf)
	vstring_free(dict->fold_buf);
    myfree((char *) dp);
}

/* dict_sockmap_open - open socket map */

DICT   *dict_sockmap_open(const char *mapname, int open_flags, int dict_flags)
{
    DICT_SOCKMAP *dp;
    char   *saved_name;
    char   *sockmap;

    /*
     * Sanity checks.
     */
    if (open_flags != O_RDONLY)
	return (dict_surrogate(DICT_TYPE_SOCKMAP, mapname,
			       open_flags, dict_flags,
			       "%s:%s map requires O_RDONLY access mode",
			       DICT_TYPE_SOCKMAP, mapname));
    if (dict_flags & DICT_FLAG_NO_UNAUTH)
	return (dict_surrogate(DICT_TYPE_SOCKMAP, mapname,
			       open_flags, dict_flags,
		     "%s:%s map is not allowed for security-sensitive data",
			       DICT_TYPE_SOCKMAP, mapname));

    /*
     * Split the socketmap name off the Postfix mapname.
     */
    saved_name = mystrdup(mapname);
    if ((sockmap = split_at_right(saved_name, ':')) == 0)
	return (dict_surrogate(DICT_TYPE_SOCKMAP, mapname,
			       open_flags, dict_flags,
			       "%s requires server:socketmap argument",
			       DICT_TYPE_SOCKMAP));

    /*
     * Instantiate the shared client handle.
     * 
     * XXX Todo: graceful degradation after endpoint syntax error.
     */
    if (dict_sockmap_refcount == 0)
	dict_sockmap_clnt = auto_clnt_create(saved_name, dict_sockmap_timeout,
			       dict_sockmap_max_idle, dict_sockmap_max_ttl);
    dict_sockmap_refcount += 1;

    /*
     * Instantiate a socket map handle.
     */
    dp = (DICT_SOCKMAP *) dict_alloc(DICT_TYPE_SOCKMAP, mapname, sizeof(*dp));
    dp->rdwr_buf = vstring_alloc(100);
    dp->sockmap_name = mystrdup(sockmap);
    dp->dict.lookup = dict_sockmap_lookup;
    dp->dict.close = dict_sockmap_close;
    /* Don't look up parent domains or network superblocks. */
    dp->dict.flags = dict_flags | DICT_FLAG_PATTERN;

    /*
     * Clean up.
     */
    myfree(saved_name);

    return (DICT_DEBUG (&dp->dict));
}
