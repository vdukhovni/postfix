/*++
/* NAME
/*	flush 8
/* SUMMARY
/*	Postfix fast flush daemon
/* SYNOPSIS
/*	\fBflush\fR [generic Postfix daemon options]
/* DESCRIPTION
/*	The flush server maintains so-called "fast flush" logfiles with
/*	information about what messages are queued for a specific site.
/*	This program expects to be run from the \fBmaster\fR(8) process
/*	manager.
/*
/*	This server implements the following requests:
/* .IP "\fBFLUSH_REQ_ENABLE\fI sitename\fR"
/*	Enable fast flush logging for the specified site.
/* .IP "\fBFLUSH_REQ_APPEND\fI sitename queue_id\fR"
/*	Append \fIqueue_id\fR to the fast flush log for the
/*	specified site.
/* .IP "\fBFLUSH_REQ_SEND\fI sitename\fR"
/*	Arrange for the delivery of all messages that are listed in the fast
/*	flush logfile for the specified site.  After the logfile is processed,
/*	the file is truncated to length zero.
/* .IP "\fBTRIGGER_REQ_WAKEUP\fR (wakeup signal from master)"
/* .IP "\fBFLUSH_REQ_PURGE\fR"
/*	Pretend that \fBFLUSH_REQ_SEND\fR was received for all sites with
/*	a non-empty "fast flush" logfile, and delete empty "fast flush"
/*	logfiles that have not been updated in several days.  This operation
/*	completes in the background because it can take a noticeable
/*	amount of time.
/* .sp
/*	Fast flush logfiles are truncated only after a \fBFLUSH_REQ_SEND\fR
/*	request, not when mail is actually delivered, and therefore can 
/*	accumulate redundant or even outdated information. In order to 
/*	maintain sanity, \fBFLUSH_REQ_PURGE\fR must be requested occasionally.
/* .PP
/*	The response to the client is one of:
/* .IP \fBFLUSH_STAT_OK\fR
/*	The request completed normally.
/* .IP \fBFLUSH_STAT_BAD\fR
/*	The flush server rejected the request (bad request name, bad
/*	request parameter value).
/* .IP \fBFLUSH_STAT_UNKNOWN\fR
/*	The specified site has no fast flush log.
/* SECURITY
/* .ad
/* .fi
/*	The fast flush server is not security-sensitive. It does not
/*	talk to the network, and it does not talk to local users.
/*	The fast flush server can run chrooted at fixed low privilege.
/* DIAGNOSTICS
/*	Problems and transactions are logged to \fBsyslogd\fR(8).
/* BUGS
/*	In reality, this server schedules delivery of messages, regardless
/*	of their destination. This limitation is due to the fact that
/*	one queue runner has to handle mail for multiple destinations.
/* SEE ALSO
/*	smtpd(8) Postfix SMTP server
/*	qmgr(8) Postfix queue manager
/*	syslogd(8) system logging
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
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <utime.h>
#include <errno.h>

/* Utility library. */

#include <msg.h>
#include <events.h>
#include <vstream.h>
#include <vstring.h>
#include <vstring_vstream.h>
#include <myflock.h>
#include <valid_hostname.h>
#include <htable.h>
#include <dict.h>
#include <scan_dir.h>

/* Global library. */

#include <mail_params.h>
#include <mail_queue.h>
#include <mail_proto.h>
#include <mail_flush.h>
#include <mail_conf.h>
#include <mail_scan_dir.h>
#include <maps.h>

/* Single server skeleton. */

#include <mail_server.h>

/* Application-specific. */

#define STR(x)	vstring_str(x)
#define FLUSH_DUP_FILTER_SIZE	10000	/* graceful degradation */
#define FLUSH_MAX_UNUSED	(7 * 24 * 60 * 60)

/* flush_append - append queue ID to per-site fast flush log */

static int flush_append(const char *site, const char *queue_id)
{
    char   *myname = "flush_append";
    VSTREAM *log;

    if (msg_verbose)
	msg_info("%s: site %s queue_id %s", myname, site, queue_id);

    /*
     * Open the logfile.
     */
    if ((log = mail_queue_open(MAIL_QUEUE_FLUSH, site, O_APPEND | O_WRONLY, 0600)) == 0) {
	if (errno != ENOENT)
	    msg_fatal("%s: open fast flush log for site %s: %m", myname, site);
	return (FLUSH_STAT_UNKNOWN);
    }

    /*
     * We must lock the logfile, so that we don't lose information due to
     * concurrent access. If the lock takes too long, the Postfix watchdog
     * will eventually take care of the problem, but it will take a while.
     */
    if (myflock(vstream_fileno(log), MYFLOCK_EXCLUSIVE) < 0)
	msg_fatal("%s: lock fast flush log for site %s: %m", myname, site);

    /*
     * Append the queue ID. With 15 bits if microsecond time, a queue ID is
     * not recycled often enough for false hits to be a problem. If it does,
     * then we could add other signature information, such as the file size
     * in bytes.
     */
    vstream_fprintf(log, "%s\n", queue_id);

    /*
     * Clean up.
     */
    if (myflock(vstream_fileno(log), MYFLOCK_NONE) < 0)
	msg_fatal("%s: unlock fast flush log for site %s: %m",
		  myname, site);
    if (vstream_fclose(log) != 0)
	msg_warn("write fast flush log for site %s: %m", site);

    return (FLUSH_STAT_OK);
}

/* flush_site - flush mail queued for site */

static int flush_site(const char *site)
{
    char   *myname = "flush_site";
    VSTRING *queue_id;
    VSTRING *queue_file;
    VSTREAM *log;
    struct utimbuf tbuf;
    static char qmgr_trigger[] = {
	QMGR_REQ_SCAN_INCOMING,		/* scan incoming queue */
	QMGR_REQ_FLUSH_DEAD,		/* flush dead site/transport cache */
    };
    HTABLE *dup_filter;
    int     count;

    if (msg_verbose)
	msg_info("%s: site %s", myname, site);

    /*
     * Open the logfile.
     */
    if ((log = mail_queue_open(MAIL_QUEUE_FLUSH, site, O_RDWR, 0600)) == 0) {
	if (errno != ENOENT)
	    msg_fatal("%s: open fast flush log for site %s: %m", myname, site);
	return (FLUSH_STAT_UNKNOWN);
    }

    /*
     * We must lock the logfile, so that we don't lose information when it is
     * truncated. Unfortunately, this means that the file can be locked for a
     * significant amount of time. If things really get stuck the Postfix
     * watchdog will take care of it.
     */
    if (myflock(vstream_fileno(log), MYFLOCK_EXCLUSIVE) < 0)
	msg_fatal("%s: lock fast flush log for site %s: %m", myname, site);

    /*
     * This is the part that dominates running time: schedule the listed
     * queue files for delivery by updating their file time stamps. This
     * should take no more than a couple seconds under normal conditions
     * (sites that receive millions of messages in a day should not use fast
     * flush service). Filter out duplicate names to avoid hammering the file
     * system, with some finite limit on the amount of memory that we are
     * willing to sacrifice. Graceful degradation.
     */
    queue_id = vstring_alloc(10);
    queue_file = vstring_alloc(10);
    dup_filter = htable_create(10);
    tbuf.actime = tbuf.modtime = event_time();
    for (count = 0; vstring_get_nonl(queue_id, log) != VSTREAM_EOF; count++) {
	if (!mail_queue_id_ok(STR(queue_id))) {
	    msg_warn("bad queue id \"%.30s...\" in fast flush log for site %s",
		     STR(queue_id), site);
	    continue;
	}
	if (dup_filter->used >= FLUSH_DUP_FILTER_SIZE
	    || htable_find(dup_filter, STR(queue_id)) == 0) {
	    if (msg_verbose)
		msg_info("%s: site %s: update %s time stamps",
			 myname, site, STR(queue_id));
	    if (dup_filter->used <= FLUSH_DUP_FILTER_SIZE)
		htable_enter(dup_filter, STR(queue_id), 0);

	    mail_queue_path(queue_file, MAIL_QUEUE_DEFERRED, STR(queue_id));
	    if (utime(STR(queue_file), &tbuf) < 0) {
		if (errno != ENOENT)
		    msg_warn("%s: update %s time stamps: %m",
			     myname, STR(queue_file));
	    } else if (mail_queue_rename(STR(queue_id), MAIL_QUEUE_DEFERRED,
					 MAIL_QUEUE_INCOMING) < 0
		       && errno != ENOENT)
		msg_warn("%s: rename from %s to %s: %m",
			 STR(queue_file), MAIL_QUEUE_DEFERRED,
			 MAIL_QUEUE_INCOMING);
	} else {
	    if (msg_verbose)
		msg_info("%s: site %s: skip file %s as duplicate",
			 myname, site, STR(queue_file));
	}
    }
    htable_free(dup_filter, (void (*) (char *)) 0);
    vstring_free(queue_file);
    vstring_free(queue_id);

    /*
     * Truncate the fast flush log.
     */
    if (count > 0 && ftruncate(vstream_fileno(log), (off_t) 0) < 0)
	msg_fatal("%s: truncate fast flush log for site %s: %m",
		  myname, site);

    /*
     * Request delivery and clean up.
     */
    if (myflock(vstream_fileno(log), MYFLOCK_NONE) < 0)
	msg_fatal("%s: unlock fast flush log for site %s: %m",
		  myname, site);
    if (vstream_fclose(log) != 0)
	msg_warn("read fast flush log for site %s: %m", site);
    if (count > 0) {
	if (msg_verbose)
	    msg_info("%s: requesting delivery for site %s", myname, site);
	mail_trigger(MAIL_CLASS_PUBLIC, MAIL_SERVICE_QUEUE,
		     qmgr_trigger, sizeof(qmgr_trigger));
    }
    return (FLUSH_STAT_OK);
}

/* flush_enable - enable fast flush logging for site */

static int flush_enable(const char *site)
{
    char   *myname = "flush_enable";
    VSTREAM *log;

    if (msg_verbose)
	msg_info("%s: site %s", myname, site);

    /*
     * Open or create the logfile. Multiple requests may arrive in parallel,
     * so allow for the possibility that the file already exists.
     */
    if ((log = mail_queue_open(MAIL_QUEUE_FLUSH, site, O_CREAT | O_RDWR, 0600)) == 0)
	msg_fatal("%s: open fast flush log for site %s: %m", myname, site);

    if (vstream_fclose(log) != 0)
	msg_warn("write fast flush log for site %s: %m", site);

    return (FLUSH_STAT_OK);
}

/* flush_purge - housekeeping */

static void flush_purge(void)
{
    char   *myname = "flush_purge";
    SCAN_DIR *scan;
    char   *site;
    struct stat st;
    VSTRING *path = vstring_alloc(10);

    scan = scan_dir_open(MAIL_QUEUE_FLUSH);
    while ((site = mail_scan_dir_next(scan)) != 0) {
	if (valid_hostname(site) == 0) {
	    msg_warn("%s: bad fast flush logfile name: %s", myname, STR(path));
	    if (unlink(STR(path)) < 0)
		msg_warn("remove %s: %m", STR(path));
	    continue;
	}
	if (stat(mail_queue_path(path, MAIL_QUEUE_FLUSH, site), &st) < 0) {
	    if (errno != ENOENT)
		msg_warn("%s: stat %s: %m", myname, STR(path));
	    else if (msg_verbose)
		msg_info("%s: %s: %m", myname, STR(path));
	    continue;
	}
	if (st.st_size == 0) {
	    if (st.st_mtime + FLUSH_MAX_UNUSED < event_time()) {
		if (unlink(STR(path)) < 0)
		    msg_warn("remove %s: %m", STR(path));
		else if (msg_verbose)
		    msg_info("%s: unlink %s, unused for %d days",
			     myname, STR(path), FLUSH_MAX_UNUSED / 84600);
	    } else if (msg_verbose)
		msg_info("%s: skip site %s - empty log", myname, site);
	} else {
	    if (msg_verbose)
		msg_info("%s: flush site %s", myname, site);
	    flush_site(site);
	}
    }
    scan_dir_close(scan);
    vstring_free(path);
}

/* flush_service - perform service for client */

static void flush_service(VSTREAM *client_stream, char *unused_service,
			          char **argv)
{
    VSTRING *request = vstring_alloc(10);
    VSTRING *site = vstring_alloc(10);
    VSTRING *queue_id = vstring_alloc(10);
    int     status = FLUSH_STAT_BAD;
    static char wakeup[] = {		/* master wakeup request */
	TRIGGER_REQ_WAKEUP,
	0,
    };

    /*
     * Sanity check. This service takes no command-line arguments.
     */
    if (argv[0])
	msg_fatal("unexpected command-line argument: %s", argv[0]);

    /*
     * This routine runs whenever a client connects to the UNIX-domain socket
     * dedicated to the fast flush service. What we see below is a little
     * protocol to (1) read a request from the client (the name of the site)
     * and (2) acknowledge that we have received the request. Since the site
     * name maps onto the file system, make sure the site name is a valid
     * SMTP hostname.
     * 
     * All connection-management stuff is handled by the common code in
     * single_server.c.
     * 
     * Note that the purge operation only acknowledges receipt of the request
     * and proceeds in the background. All other operations send their result
     * status after the operation is completed.
     */
#define STREQ(x,y) (strcmp((x), (y)) == 0)

    if (mail_scan(client_stream, "%s", request) == 1) {
	if (STREQ(STR(request), FLUSH_REQ_APPEND)) {
	    if (mail_scan(client_stream, "%s %s", site, queue_id) == 2
		&& valid_hostname(STR(site))
		&& mail_queue_id_ok(STR(queue_id)))
		status = flush_append(STR(site), STR(queue_id));
	    mail_print(client_stream, "%d", status);
	} else if (STREQ(STR(request), FLUSH_REQ_SEND)) {
	    if (mail_scan(client_stream, "%s", site) == 1
		&& valid_hostname(STR(site)))
		status = flush_site(STR(site));
	    mail_print(client_stream, "%d", status);
	} else if (STREQ(STR(request), FLUSH_REQ_ENABLE)) {
	    if (mail_scan(client_stream, "%s", site) == 1
		&& valid_hostname(STR(site)))
		status = flush_enable(STR(site));
	    mail_print(client_stream, "%d", status);
	} else if (STREQ(STR(request), FLUSH_REQ_PURGE)
		   || STREQ(STR(request), wakeup)) {
	    mail_print(client_stream, "%d", FLUSH_STAT_OK);
	    flush_purge();
	}
    }
    vstring_free(request);
    vstring_free(site);
    vstring_free(queue_id);
}

/* main - pass control to the single-threaded skeleton */

int     main(int argc, char **argv)
{
    single_server_main(argc, argv, flush_service, 0);
}
