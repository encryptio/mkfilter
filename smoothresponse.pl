#!/usr/bin/perl
use warnings;
use strict;

use Getopt::Std;

sub HELP_MESSAGE {
    print <<'EOF'

Usage:
    smoothresponse -w octaves [file ...]
    smoothresponse -h

smoothresponse smooths the given response (in mkfilter --analyze format)
in the per-frequency amplitude domain. It removes the phase information, as it
is corrupted by this smoothing.

Its purpose is to create smooth custom filters and to visualize filters in a
more pleasing way.

It samples the same frequencies as are in the input, so its intended behavior
is only reached when the input is dense.

EOF
}

my %opts;
$Getopt::Std::STANDARD_HELP_VERSION = 1;
getopts('hw:', \%opts);

HELP_MESSAGE(), exit if exists $opts{'h'};
my $octaves = $opts{'w'}
    or die "Need a smoothing width. Run with -h for help.\n";

my @data = ();

while ( <> ) {
    if ( /^\s*#/ ) {
        print;
    } else {
        my ($freq, $amp) = split /[=\s]+/;
        next unless $freq and $amp;
        next unless $freq =~ /^[\d\.e]+$/i and $amp =~ /^[\d\.e]+$/i;
        push @data, [$freq, $amp];
    }
}

my @outdata = ();

my $sum = $data[0][1];
my $from = 0;
my $to = 0;
for my $f ( map $_->[0], @data ) {
    my $topfreq = $f * (1+$octaves);
    my $bottomfreq = $f / (1+$octaves);

    $sum += $data[$to++  ][1] while $#data > $to+1   and $data[$to  ][0] < $topfreq;
    $sum -= $data[$from++][1] while $#data > $from+1 and $data[$from][0] < $bottomfreq;

    print "$f ".($sum/($to-$from+1))."\n";
}

