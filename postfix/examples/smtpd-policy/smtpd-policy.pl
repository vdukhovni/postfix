#!/usr/bin/perl

use DB_File;
use Fcntl;
use Sys::Syslog qw(:DEFAULT setlogsock);

#
# Usage: smtpd-policy.pl [-v]
#
# Demo delegated Postfix SMTPD policy server. This server implements
# greylisting. State is kept in a Berkeley DB database.  Logging is
# sent to syslogd.
#
# To run this from /etc/postfix/master.cf:
#
#    policy  unix  -       n       n       -       -       spawn
#      user=nobody argv=/usr/bin/perl /usr/libexec/postfix/smtpd-policy.pl
#
# To use this from Postfix SMTPD, use in /etc/postfix/main.cf:
#
#    smtpd_policy_service_endpoint = plain:unix:private/policy
#    smtpd_recipient_restrictions = ... check_policy_service ...
#
# This script runs as one PERL process per SMTP server process.
# By default, a Postfix SMTP server process terminates after 100
# seconds of idle time, or after serving 100 clients.
#

#
# greylist status database and greylist time interval. DO NOT create the
# greylist status database in a world-writable directory such as /tmp
# or /var/tmp. DO NOT create the greylist database in a file system
# that can run out of space.
#
$database_name="/var/mta/smtpd-policy.db";
$greylist_delay=3600;

#
# Syslogging options for verbose mode and for fatal errors. Comment
# out the $syslog_socktype line if syslogging does not work.
#
$syslog_socktype = 'unix'; # inet, unix, stream, console
$syslog_facility="mail";
$syslog_options="pid";
$syslog_priority="info";

#
# Demo policy routine. The result is an action just like it would
# be specified on the right-hand side of a Postfix access table.
# Request attributes are passed in via the %attr hash.
#
sub policy {
    local(*attr) = @_;
    my($key, $time_stamp, $now);

    # Open the database on the fly.
    open_database() unless $database_obj;

    # Lookup the time stamp for this client/sender/recipient.
    $key = $attr{"client_address"}."/".$attr{"sender"}."/".$attr{"recipient"};
    $time_stamp = read_database($key);
    $now = time();

    # If new request, add this client/sender/recipient to the database.
    if ($time_stamp == 0) {
	$time_stamp = $now;
	update_database($key, $time_stamp);
    }

    syslog $syslog_priority, "request age %d", $now - $time_stamp if $verbose;
    if ($time_stamp + $greylist_delay < $now) {
	return "ok";
    } else {
	return "450 request is greylisted";
    }
}

#
# You should not have to make changes below this point.
#
sub LOCK_SH { 1 };	# Shared lock (used for reading).
sub LOCK_EX { 2 };	# Exclusive lock (used for writing).
sub LOCK_NB { 4 };	# Don't block (for testing).
sub LOCK_UN { 8 };	# Release lock.

#
# Log an error and abort.
#
sub fatal_exit {
    syslog "err", @_;
    exit 1;
}

#
# Open hash database.
#
sub open_database {
    my($database_fd);

    $database_obj = tie(%db_hash, 'DB_File', $database_name, 
			    O_CREAT|O_RDWR, 0644) ||
	fatal_exit "Cannot open database %s: %m", $database_name;
    $database_fd = $database_obj->fd;
    open DATABASE_HANDLE, "+<&=$database_fd" ||
	fatal_exit "Cannot fdopen database %s: %m", $database_name;
    syslog $syslog_priority, "open %s", $database_name if $verbose;
}

#
# Read database.
#
sub read_database {
    my($key) = @_;
    my($value);

    flock DATABASE_HANDLE, LOCK_SH ||
	fatal_exit "Can't get shared lock on %s: %m", $database_name;
    $value = $db_hash{$key};
    syslog $syslog_priority, "lookup %s: %s", $key, $value if $verbose;
    flock DATABASE_HANDLE, LOCK_UN ||
	fatal_exit "Can't unlock %s: %m", $database_name;
    return $value;
}

#
# Update database.
#
sub update_database {
    my($key, $value) = @_;

    syslog $syslog_priority, "store %s: %s", $key, $value if $verbose;
    flock DATABASE_HANDLE, LOCK_EX ||
	fatal_exit "Can't exclusively lock %s: %m", $database_name;
    $db_hash{$key} = $value;
    $database_obj->sync() &&
	fatal_exit "Can't update %s: %m", $database_name;
    flock DATABASE_HANDLE, LOCK_UN ||
	fatal_exit "Can't unlock %s: %m", $database_name;
}

#
# This process runs as a daemon, so it can't log to a terminal. Use
# syslog so that people can actually see our messages.
#
setlogsock $syslog_socktype;
openlog $0, $syslog_options, $syslog_facility;

#
# We don't need getopt() for now.
#
while ($option = shift(@ARGV)) {
    if ($option eq "-v") {
	$verbose = 1;
    } else {
	syslog $syslog_priority, "Invalid option: %s. Usage: %s [-v]", 
		$option, $0;
	exit 1;
    }
}

#
# Unbuffer standard output.
#
select((select(STDOUT), $| = 1)[0]);

#
# Receive a bunch of attributes, evaluate the policy, send the result.
#
while (<STDIN>) {
    if (/([^=]+)=(.*)\n/) {
	$attr{$1} = $2;
    } else {
	if ($verbose) {
	    for (keys %attr) {
		syslog $syslog_priority, "Attribute: %s=%s", $_, $attr{$_};
	    }
	}
	$action = &policy(*attr);
	syslog $syslog_priority, "Action: %s", $action if $verbose;
	print STDOUT "action=$action\n\n";
	%attr = ();
    }
}
