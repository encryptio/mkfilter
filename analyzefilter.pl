#!/usr/bin/perl
#
# Copyright (c) 2009 Chris Kastorff
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions, and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * The name Chris Kastorff may not be used to endorse or promote
#       products derived from this software without specific prior written
#       permission.
#

use strict;
use warnings;

use Getopt::Std;

sub HELP_MESSAGE {
    print <<'EOF'

Usage:
    analyzefilter [-s WIDTHxHEIGHT] [-f FREQ:[FREQ]] [-p POWER:POWER]
        [-F analyzefactor] [-m mode] [-L] -o output.png [--] input.wav ...
    analyzefilter -h

analyzefilter graphs an FIR filter's frequency response with gnuplot and
mkfilter.

The -f and -p options allow you to change the window of the graph (note that
with -p, the power levels are decimal, not decibel. That is, -p 0.1:10 (decimal)
should be used, not -p -20:20 (decibel.))

The -s option allows you to set the pixel size of the output graph.

The -F option passes the given analyzefactor to mkfilter, see its documentation
for details.

The -m option changes the graph mode. The two allowable choices are "magnitude"
(the default), and "phase".

The -L option removes a linear phase from the graph by looking at the first
two phases. It is likely to screw up non-linear phase filter analysis.

EOF
}

my $size = '800x400';
my $analyzefactor = 1;
my $powerrange = '0.000001:100';
my $freqrange = '10:';
my $mode = 'magnitude';
my $linearkill = 0;
my $outfile;

my %opts;
$Getopt::Std::STANDARD_HELP_VERSION = 1;
getopts('hLvo:s:F:f:p:m:', \%opts);

$size = $opts{'s'} if exists $opts{'s'};
$outfile = $opts{'o'} if exists $opts{'o'};
$analyzefactor = $opts{'F'} if exists $opts{'F'};
$powerrange = $opts{'p'} if exists $opts{'p'};

$freqrange = $opts{'f'} if exists $opts{'f'};
$mode = $opts{'m'} if exists $opts{'m'};
$linearkill = 1 if exists $opts{'L'};

my @inputs = @ARGV;

if ( exists $opts{'h'} and $opts{'h'} ) {
    HELP_MESSAGE; exit 0;
}

die "Need an output file. Run with -h for help.\n" if not defined $outfile;
die "Need input file(s)\n" unless @inputs;
die "Mode must be either 'magnitude' or 'phase'.\n" unless $mode =~ /^(magnitude|phase)$/;

my @data = map {
        my @lines = grep { $_ =~ /^[\d\.]/ } (`mkfilter --analyze --analyzefactor=\Q$analyzefactor\E \Q$_`);
        die "Couldn't analyze $_, mkfilter exited with status $?" if $?;
        if ( $linearkill ) {
            my $b =  ($lines[0] =~ /\s([\d\.eE]+)\s*$/)[0];
            my $m = (($lines[1] =~ /\s([\d\.eE]+)\s*$/)[0]-$b);
            die unless defined $b and defined $m;
            my @newlines = ();
            my $at = 0;
            for my $l ( @lines ) {
                my @f = split /\s+/, $l, 3;
                $f[2] -= $b + $m*$at;
                $at++;
                $l = join("\t", @f)."\n";
            }
        }
        join '', @lines;
    } @inputs;

$size =~ s/x/,/;

open my $gnuplot, "|-", "gnuplot" or die "Couldn't open a pipe to gnuplot: $!";
print $gnuplot <<EOF;
set terminal png notransparent small size $size x040410 x9999cc x444477 xff0000 x00ff00 x0000ff
set output '$outfile'

EOF
if ( $mode eq "magnitude" ) {
    print $gnuplot <<EOF;
set log y
set yrange [$powerrange]
set ytics mirror (  "40dB" 100, \\
                    "20dB"  10, \\
                     "0dB"   1, \\
                   "-20dB"   0.1, \\
                   "-40dB"   0.01, \\
                   "-60dB"   0.001, \\
                   "-80dB"   0.0001, \\
                  "-100dB"   0.00001, \\
                  "-120dB"   0.000001)
EOF
}
print $gnuplot <<EOF;

set log x
set grid

set xrange [$freqrange]

set autoscale xfixmax

set xtics ( \\
              "10Hz"    10, \\
              "20Hz"    20, \\
                  ""    30, \\
                  ""    40, \\
                  ""    50, \\
              "60Hz"    60, \\
                  ""    70, \\
                  ""    80, \\
                  ""    90, \\
             "100Hz"   100, \\
                  ""   200, \\
                  ""   300, \\
                  ""   400, \\
             "500Hz"   500, \\
                  ""   600, \\
                  ""   700, \\
                  ""   800, \\
                  ""   900, \\
            "1000Hz"  1000, \\
                  ""  2000, \\
                  ""  3000, \\
                  ""  4000, \\
            "5000Hz"  5000, \\
                  ""  6000, \\
                  ""  7000, \\
                  ""  8000, \\
                  ""  9000, \\
           "10000Hz" 10000, \\
                  "" 15000, \\
           "20000Hz" 20000)
EOF

my $plotcmd = "plot " . join(", ", map "'-' using ".($mode eq "magnitude" ? "1:2" : "1:3")." with lines title \"$_\"", @inputs);
print $gnuplot "$plotcmd\n";

for my $d ( @data ) {
    print $gnuplot "$d\ne\n";
}

close $gnuplot;

