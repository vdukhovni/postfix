#! /usr/bin/perl

use strict;
use warnings;

# Postfix delivery agents
my @agents = qw(discard error lmtp local pipe smtp virtual);

my $instre = qr{(?x)
	\A			# Absolute line start
	(?:\S+ \s+){3} 		# Timestamp, adjust for other time formats
	\S+ \s+ 		# Hostname
	(postfix(?:-[^/\s]+)?)	# Capture instance name stopping before first '/'
	(?:/\S+)*		# Optional non-captured '/'-delimited qualifiers
	/			# Final '/' before the daemon program name
	};

my $cmdpidre = qr{(?x)
	\G			# Continue from previous match
	(\S+)\[(\d+)\]:\s+	# command[pid]:
};

my %smtpd;
my %smtp;
my %transaction;
my $i = 0;
my %seqno;

my %isagent = map { ($_, 1) } @agents;

while (<>) {
	next unless m{$instre}ogc; my $inst = $1;
	next unless m{$cmdpidre}ogc; my $command = $1; my $pid = $2;

	if ($command eq "smtpd") {
		if (m{\Gconnect from }gc) {
			# Start new log
			$smtpd{$pid}->{"log"} = $_; next;
		}

		$smtpd{$pid}->{"log"} .= $_;

		if (m{\G(\w+): client=}gc) {
			# Fresh transaction 
			my $qid = "$inst/$1";
			$smtpd{$pid}->{"qid"} = $qid;
			$transaction{$qid} = $smtpd{$pid}->{"log"};
			$seqno{$qid} = ++$i;
			next;
		}

		my $qid = $smtpd{$pid}->{"qid"};
		$transaction{$qid} .= $_
			if (defined($qid) && exists $transaction{$qid});
		delete $smtpd{$pid} if (m{\Gdisconnect from}gc);
		next;
	}

	if ($command eq "pickup") {
		if (m{\G(\w+): uid=}gc) {
			my $qid = "$inst/$1";
			$transaction{$qid} = $_;
			$seqno{$qid} = ++$i;
		}
		next;
	}

	# bounce(8) logs transaction start after cleanup(8) already logged
	# the message-id, so the cleanup log entry may be first
	#
	if ($command eq "cleanup") {
		next unless (m{\G(\w+): }gc);
		my $qid = "$inst/$1";
		$transaction{$qid} .= $_;
		$seqno{$qid} = ++$i if (! exists $seqno{$qid});
		next;
	}

	if ($command eq "qmgr") {
		next unless (m{\G(\w+): }gc);
		my $qid = "$inst/$1";
		if (defined($transaction{$qid})) {
			$transaction{$qid} .= $_;
			if (m{\Gremoved$}gc) {
				print delete $transaction{$qid}, "\n";
			}
		}
		next;
	}

	# Save pre-delivery messages for smtp(8) and lmtp(8)
	#
	if ($command eq "smtp" || $command eq "lmtp") {
		$smtp{$pid} .= $_;

		if (m{\G(\w+): to=}gc) {
			my $qid = "$inst/$1";
			if (defined($transaction{$qid})) {
				$transaction{$qid} .= $smtp{$pid};
			}
			delete $smtp{$pid};
		}
		next;
	}

	if ($command eq "bounce") {
		if (m{\G(\w+): .*? notification: (\w+)$}gc) {
			my $qid = "$inst/$1";
			my $newid = "$inst/$2";
			if (defined($transaction{$qid})) {
				$transaction{$qid} .= $_;
			}
			$transaction{$newid} =
				$_ . $transaction{$newid};
			$seqno{$newid} = ++$i if (! exists $seqno{$newid});
		}
		next;
	}

	if ($isagent{$command}) {
		if (m{\G(\w+): to=}gc) {
			my $qid = "$inst/$1";
			if (defined($transaction{$qid})) {
				$transaction{$qid} .= $_;
			}
		}
		next;
	}
}

# Dump logs of incomplete transactions.
foreach my $qid (sort {$seqno{$a} <=> $seqno{$b}} keys %transaction) {
    print $transaction{$qid}, "\n";
}
