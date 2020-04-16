#! /usr/bin/env perl

use strict;
use warnings;

local $/ = "\n\n";

while (<>) {
    my $qid;
    my %tls;
    my $smtp;
    foreach my $line (split("\n")) {
	if ($line =~ m{ postfix(?:\S*?)/qmgr\[\d+\]: (\w+): from=<.*>, size=\d+, nrcpt=\d+ [(]queue active[)]$}) {
	    $qid //= $1;
	    next;
	}
	if ($line =~ m{ postfix(?:\S*?)/smtp\[(\d+)\]: (\S+) TLS connection established to (\S+): (.*)}) {
	    $tls{$1}->{lc($3)} = [$2, $4];
	    next;
	}
	if ($line =~ m{.*? postfix(?:\S*?)/smtp\[(\d+)\]: (\w+): (to=.*), relay=(\S+), (delay=\S+, delays=\S+, dsn=2\.\S+, status=sent .*)}) {
	    next unless $qid eq $2;
	    if (defined($tls{$1}->{lc($4)}) && ($tls{$1}->{lc($4)}->[2] //= $5) eq $5) {
		printf "qid=%s, relay=%s, %s -> %s %s\n", $qid, lc($4), $3, @{$tls{$1}->{lc($4)}}[0..1];
	    } else {
		delete $tls{$1};
		printf "qid=%s, relay=%s, %s -> cleartext\n", $qid, lc($4), $3;
	    }
	}
    }
}
