/*++
/* NAME
/*	flushd 8
/* SUMMARY
/*	Postfix fast flush daemon
/* SYNOPSIS
/*	\fBflushd\fR [generic Postfix daemon options]
/* DESCRIPTION
/*	The flush server maintains so-called "fast flush" logfiles with
/*	information about what messages are queued for a specific site.
/*	This program expects to be run from the \fBmaster\fR(8) process
/*	manager.
/*
/*	This server implements the following requests:
/* .IP "\fBFLUSH_REQ_ADD\fI sitename queue_id\fR"
/*	Append \fIqueue_id\fR to the fast flush logfile for the
/*	specified site.
/* .IP "\fBFLUSH_REQ_SEND\fI sitename\fR"
/*	Arrange for the delivery of all messages that are listed in the fast
/*	flush logfile for the specified site.  After the logfile is processed,
/*	the file is truncated to length zero.
/* .PP
/*	The response to the client is one of:
/* .IP \fBFLUSH_STAT_OK\fR
/*	The request completed normally.
/* .IP \fBFLUSH_STAT_BAD\fR
/*	The flush server rejected the request (bad request name, bad
/*	request parameter value).
/* .IP \fBFLUSH_STAT_UNKNOWN\fR
/*	The specified site has no fast flush logfile and is not configured
/*	to have one.
/* .PP
/*	Fast flush logfiles are truncated only after a flush request. In
/*	order to prevent fast flush logfiles from growing without bounds,
/*	and to prevent them from accumulating too much outdated information,
/*	the flush service generates a pro-active flush request once every
/*	every 1000 append requests. This should not impact operation.
/* SECURITY
/* .ad
/* .fi
/*	The fast flush server is moderately security-sensitive. It does not
/*	talk to the network, but it does talk to local unprivileged users, in
/*	order to emulate "sendmail -qRsite" behavior.  For this reason all
/*	strings in a request are truncated at \fIline_length_limit\fR.
/*
/*	The fast flush server can run chrooted at fixed low privilege.
/* DIAGNOSTICS
/*	Problems and transactions are logged to \fBsyslogd\fR(8).
/* BUGS
/*	In reality, this server schedules delivery of messages, regardless
/*	of their destination. This limitation is due to the fact that
/*	one queue runner has to handle mail for multiple destinations.
/* CONFIGURATION PARAMETERS
/* .ad
/* .fi
/*	The following \fBmain.cf\fR parameters are especially relevant to
/*	this program. See the Postfix \fBmain.cf\fR file for syntax details
/*	and for default values. Use the \fBpostfix reload\fR command after
/*	a configuration change.
/* .IP \fBetrn_maps\fR
/*	Tables that specify what domains have \fBETRN\fR service.  For each
/*	table entry, the left-hand side specifies a destination domain name
/*	that can be specified in an \fBETRN\fR request, and the right-hand
/*	side specifies a list of access restrictions for SMTP clients that
/*	issue \fBETRN\fR for the domain.
/* .IP \fBfast_flush_maps\fR
/*	The table with names of destinations that this MTA provides the
/*	fast flush service for. By default, this is set to $\fBetrn_maps\fR.
/* .IP \fBline_length_limit\fR
/*	Maximal length of strings in a fast flush client request.
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

/* Global library. */

#include <mail_params.h>
#include <mail_queue.h>
#include <mail_proto.h>
#include <mail_flush.h>
#include <mail_conf.h>
#include <maps.h>

/* Single server skeleton. */

#include <mail_server.h>

 /*
  * Tunable parameters.
  */
char   *var_etrn_maps;
char   *var_fflush_maps;

/* Application-specific. */

#define STR(x)	vstring_str(x)
#define MAX_DUP_FILTER	10000

static MAPS *fflush_maps;

/* flush_append - append queue ID to per-site fast flush logfile */

static int flush_append(const char *site, const char *queue_id)
{
    char   *myname = "flush_append";
    VSTREAM *log;

    if (msg_verbose)
	msg_info("%s: site %s queue_id %s", myname, site, queue_id);

    /*
     * Open or create the logfile. We allow for the fact that a logfile
     * exists for a site that is no longer listed in the fast flush maps.
     */
    if ((log = mail_queue_open(MAIL_QUEUE_FLUSH, site, O_APPEND | O_WRONLY, 0600)) == 0) {
	if (errno != ENOENT)
	    msg_fatal("%s: open fast flush log for site %s: %m", myname, site);
	if (maps_find(fflush_maps, site, 0) == 0) {
	    msg_warn("no fast flush support configured for site %s", site);
	    return (FLUSH_STAT_UNKNOWN);
	}
	log = mail_queue_open(MAIL_QUEUE_FLUSH, site, O_CREAT | O_APPEND | O_WRONLY, 0600);
	if (log == 0)
	    msg_fatal("%s: open fast flush log for site %s: %m", myname, site);
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
	msg_fatal("%s: unlock fast flush logfile for site %s: %m",
		  myname, site);
    if (vstream_fclose(log) != 0)
	msg_warn("write fast flush logfile for site %s: %m", site);

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
	QMGR_REQ_SCAN_DEFERRED,		/* scan deferred queue */
	QMGR_REQ_SCAN_INCOMING,		/* scan incoming queue */
    };
    HTABLE *dup_filter;

    if (msg_verbose)
	msg_info("%s: site %s", myname, site);

    /*
     * Open the logfile.
     */
    if ((log = mail_queue_open(MAIL_QUEUE_FLUSH, site, O_RDWR, 0600)) == 0) {
	if (errno != ENOENT)
	    msg_fatal("%s: open fast flush log for site %s: %m", myname, site);
	if (maps_find(fflush_maps, site, 0)) {
	    msg_warn("no fast flush log for site %s", site);
	    return (FLUSH_STAT_OK);
	} else {
	    msg_warn("no fast flush support configured for site %s", site);
	    return (FLUSH_STAT_UNKNOWN);
	}
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
    while (vstring_get_nonl(queue_id, log) != VSTREAM_EOF) {
	if (dup_filter->used >= MAX_DUP_FILTER
	    || htable_find(dup_filter, STR(queue_id)) == 0) {
	    if (msg_verbose)
		msg_info("%s: site %s: update %s time stamps",
			 myname, site, STR(queue_file));
	    if (dup_filter->used <= MAX_DUP_FILTER)
		htable_enter(dup_filter, STR(queue_id), 0);

	    mail_queue_path(queue_file, MAIL_QUEUE_DEFERRED, STR(queue_id));
	    if (utime(STR(queue_file), &tbuf) == 0)
		continue;
	    if (errno != ENOENT)
		msg_fatal("%s: update %s time stamps: %m",
			  myname, STR(queue_file));

	    mail_queue_path(queue_file, MAIL_QUEUE_INCOMING, STR(queue_id));
	    if (utime(STR(queue_file), &tbuf) == 0)
		continue;
	    if (errno != ENOENT)
		msg_fatal("%s: update %s time stamps: %m",
			  myname, STR(queue_file));
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
     * Truncate the fast flush logfile.
     */
    if (ftruncate(vstream_fileno(log), (off_t) 0) < 0)
	msg_fatal("%s: truncate fast flush logfile for site %s: %m",
		  myname, site);

    /*
     * Request delivery and clean up.
     */
    if (myflock(vstream_fileno(log), MYFLOCK_NONE) < 0)
	msg_fatal("%s: unlock fast flush logfile for site %s: %m",
		  myname, site);
    if (vstream_fclose(log) != 0)
	msg_warn("read fast flush logfile for site %s: %m", site);
    if (msg_verbose)
	msg_info("%s: requesting delivery for site %s", myname, site);
    mail_trigger(MAIL_CLASS_PUBLIC, MAIL_SERVICE_QUEUE,
		 qmgr_trigger, sizeof(qmgr_trigger));

    return (FLUSH_STAT_OK);
}

/* flush_service - perform service for client */

static void flush_service(VSTREAM *client_stream, char *unused_service,
			          char **argv)
{
    VSTRING *request = vstring_alloc(10);
    VSTRING *site = vstring_alloc(10);
    VSTRING *queue_id;
    int     status = FLUSH_STAT_BAD;
    static int counter;

    /*
     * Sanity check. This service takes no command-line arguments.
     */
    if (argv[0])
	msg_fatal("unexpected command-line argument: %s", argv[0]);

    /*
     * Vandalism control. Read no unlimited amounts of garbage from a public
     * socket.
     */
    vstring_ctl(request, VSTRING_CTL_MAXLEN, var_line_limit, VSTRING_CTL_END);
    vstring_ctl(site, VSTRING_CTL_MAXLEN, var_line_limit, VSTRING_CTL_END);

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
     */
#define STREQ(x,y) (strcmp((x), (y)) == 0)

    if (mail_scan(client_stream, "%s %s", request, site) == 2
	&& valid_hostname(STR(site))) {
	if (STREQ(STR(request), FLUSH_REQ_ADD)) {
	    queue_id = vstring_alloc(10);
	    vstring_ctl(queue_id, VSTRING_CTL_MAXLEN, var_line_limit,
			VSTRING_CTL_END);
	    if (mail_scan(client_stream, "%s", queue_id) == 1)
		status = flush_append(STR(site), STR(queue_id));
	    vstring_free(queue_id);
	} else if (STREQ(STR(request), FLUSH_REQ_SEND)) {
	    status = flush_site(STR(site));
	}
    }
    mail_print(client_stream, "%d", status);

    /*
     * Once in a while we generate a pro-active flush request to ensure that
     * the logfile does not grow unreasonably, and to ensure that it does not
     * contain too much outdated information. Flush our reply to the client
     * so that it does not have to wait while the pro-active flush happens.
     */
    if (status == FLUSH_STAT_OK && STREQ(STR(request), FLUSH_REQ_ADD)
	&& (++counter + event_time() + getpid()) % 1000 == 0) {
	vstream_fflush(client_stream);
	if (msg_verbose)
	    msg_info("site %s: time for a pro-active flush", STR(site));
	(void) flush_site(STR(site));
    }
    vstring_free(site);
    vstring_free(request);
}

/* pre_accept - see if tables have changed */

static void pre_accept(char *unused_name, char **unused_argv)
{
    if (dict_changed()) {
	msg_info("table has changed -- exiting");
	exit(0);
    }
}

/* pre_jail_init - pre-chroot initialization */

static void pre_jail_init(char *unused_service, char **unused_argv)
{
    fflush_maps = maps_create(VAR_FFLUSH_MAPS, var_fflush_maps,
			      DICT_FLAG_LOCK);
}

/* pre_accept_init - check map status */


/* main - pass control to the single-threaded skeleton */

int     main(int argc, char **argv)
{
    static CONFIG_STR_TABLE str_table[] = {
	VAR_ETRN_MAPS, DEF_ETRN_MAPS, &var_etrn_maps, 0, 0,
	VAR_FFLUSH_MAPS, DEF_FFLUSH_MAPS, &var_fflush_maps, 0, 0,
	0,
    };

    single_server_main(argc, argv, flush_service,
		       MAIL_SERVER_PRE_INIT, pre_jail_init,
		       MAIL_SERVER_PRE_ACCEPT, pre_accept,
		       MAIL_SERVER_STR_TABLE, str_table,
		       0);
}
