#!/usr/bin/perl
#############################################################################
# mkfilter.pl - Create FIR filters
# The latest version of this can be found at:
# http://encryptio.com/code/mkfilter
#############################################################################
# Copyright (c) 2009-2010 Chris Kastorff
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
#############################################################################
use warnings;
use strict;

use POSIX qw/ floor ceil /;
use Math::Complex ':DEFAULT', ':pi';
use List::Util qw/ sum min max /;
use Math::FFT;

################################################################################
### Oddity.

my $USEWINDOW;

################################################################################
### File reading

sub read_file {
    eval { require Audio::SndFile };
    if ( $@ ) {
        print STDERR "Audio::SndFile is required to read files.\n";
        exit 1;
    }

    my ($fn) = @_;

    my $lf = Audio::SndFile->open("<", $fn) or die;
    if ( $lf->channels != 1 ) {
        print STDERR "Input file $fn has multiple channels.\n";
        exit 1;
    }

    return ($lf->samplerate, [$lf->unpackf_double($lf->frames)]);
}

################################################################################
### File writing

sub write_file {
    eval { require Audio::SndFile };
    if ( $@ ) {
        goto &write_file_pureperl;
    } else {
        goto &write_file_sndfile;
    }
}

sub guessformat {
    my ($filename) = @_;

    my ($format) = ($filename =~ /\.([^\.]+)$/);
    $format = 'wav' unless defined $format;
    $format = lc $format;

    return $format;
}

sub write_file_sndfile {
    my ($fn, $data, $sr) = @_;

    my $format = guessformat($fn);

    my $sf = Audio::SndFile->open(">", $fn,
        type => $format,
        endianness => 'file',
        samplerate => $sr,
        subtype => 'pcm_24', # TODO: allow other options here
        channels => 1,
    );

    $sf->pack_double(@$data);
}

# fallback, so users without Audio::SndFile can do something
sub write_file_pureperl {
    my ($fn, $data, $sr) = @_;

    my $format = guessformat($fn);
    if ( $format ne "au" ) {
        print STDERR "Only .au is supported without Audio::SndFile.\n";
        exit 1;
    }

    open my $sf, ">", $fn or die "Couldn't open $fn for reading: $!";

    print $sf ".snd" . pack "NNNNN",
        24,               # data starts at byte 24
        scalar(@$data)*2, # length (in bytes)
        3,                # 16-bit linear PCM
        $sr,              # sample rate
        1;                # channel count

    my $buf = '';
    for my $d ( @$data ) {
        my $is = floor $d*2**15;

        # clip
        if ( $is > 2**15-1 ) {
            $is = 2**15-1;
        } elsif ( $is < -(2**15) ) {
            $is = -(2**15);
        }

        # wrap to twos complement so we can pack unsigned
        $is = 2**16+$is if $is < 0;

        $buf .= pack 'n', $is;

        if ( length $buf > 8192 ) {
            print $sf $buf;
            $buf = '';
        }
    }

    if ( length $buf ) {
        print $sf $buf;
        $buf = '';
    }

    close $sf;
}

################################################################################
### Filter Analysis

sub dofft_polar {
    my ($input) = @_;

    my $fftinput  = [map +($_,0), @$input];
    my $fftoutput = Math::FFT->new($fftinput)->cdft;

    return [
        map {
            my $t = Math::Complex->make($fftoutput->[$_*2], $fftoutput->[$_*2+1]);
            (abs($t), arg($t));
        } 0 .. (scalar(@$fftoutput)/4)
    ];
}

sub print_gnuplot {
    my ($data, $sr, $analyzefactor) = @_;

    my $tofft = [@$data];

    # pad to 8x over the next highest power of 2
    my $wantsize = 2 ** (ceil(log(scalar @$data)/log(2))+$analyzefactor);
    $wantsize = 2**14 unless $wantsize > 2**14; # at least 16384
    push @$tofft, 0 while @$tofft < $wantsize;

    my $fft = dofft_polar($tofft);

    print "# SAMPLERATE=$sr\n";
    print "# frequency magnitude phase\n\n";
    my $lastphase = 0;
    my $runningphase = 0;
    for my $i ( 0 .. $#$fft/2 ) {
        my ($f, $mag, $phase) = ($sr*$i/@$fft, $fft->[$i*2], $fft->[$i*2+1]);

        # unwrap phase
        my $phasediff = $phase - $lastphase;
        $phasediff -= pi2 while $phasediff > +pi();
        $phasediff += pi2 while $phasediff < -pi();
        $runningphase += $phasediff;
        $lastphase = $phase;

        print "$f\t$mag\t$runningphase\n";
    }
}

################################################################################
### Filter creation

### Mostly linear-phase stuff

sub window {
    my ($window, $data) = @_;
    my $m = scalar @$data;

    my $winfunc;

    if ( $window eq "blackman" ) {
        $winfunc = sub { 0.42 - 0.5 * cos(pi2 * $_[0]/$m) + 0.08 * cos(pi4 * $_[0]/$m) };
    } elsif ( $window eq "hamming" ) {
        $winfunc = sub { 0.54 - 0.46*cos(pi2 * $_[0]/$m) };
    } elsif ( $window eq "barlett" ) {
        $winfunc = sub { 1-abs(1-$_[0]/$m*2) };
    } elsif ( $window eq "cosine" or $window eq "hanning" ) {
        $winfunc = sub { 0.5 - 0.5*cos(pi2 * $_[0]/$m) };
    } elsif ( $window eq "rectangular" or $window eq "none" ) {
        $winfunc = sub { 1 };
    } else {
        die "unknown window function $window";
    }

    return [map $data->[$_] * $winfunc->($_), 0 .. $#$data];
}

sub make_sinc {
    my ($sr, $frequency, $size) = @_;
    $size++ if $size % 2 == 0; # must have odd size, otherwise symmetry causes nonlinear phase in other operations

    my $fc = $frequency/$sr;

    my $center = floor $size/2;

    my $out = [];
    for my $i ( 0 .. $size-1 ) {
        if ( $i == $center ) {
            $out->[$i] = pi2 * $fc;
        } else {
            $out->[$i] = sin(pi2 * $fc * ($i-$center)) / ($i-$center);
        }
    }

    return $out;
}

sub make_custom_zerophase {
    my ($sr, $wantpoints, $size) = @_;
    $size++ if $size % 2 == 0; # odd size for proper window usage

    my $fftsize = 2 ** ceil(log($size)/log(2)+1);

    my $want = make_wantcurve($sr, $fftsize, $wantpoints);

    my $coeff = [map +($_,0), @$want];
    my $fft = Math::FFT->new($coeff)->invcdft($coeff);

    my @data = map $fft->[$_*2], 0 .. floor($#$fft/2); # extract real part
    unshift @data, pop @data for 1 .. $size/2; # shift
    @data = @data[0..$size-1]; # truncate

    return window($USEWINDOW, \@data);
}

# input to this function MUST be linear phase with center at floor $#data/2,
# and it also MUST be normalized
sub spectral_inversion {
    my ($data) = @_;

    my $out = [map -$_, @$data];
    $out->[floor @$data/2] += 1;

    return $out;
}

# input to this function MUST be linear phase with center at floor $#data/2
sub spectral_reversal {
    my ($data) = @_;
    return [map $data->[$_]*($_ % 2 == 0 ? -1 : 1), 0 .. $#$data];
}

# DC -> 0dB
sub normalize {
    my ($data) = @_;
    my $sum = sum @$data;
    return [map $_/$sum, @$data];
}

# Peak in time domain -> 0dB
sub normalize_peak {
    my ($data) = @_;
    my $max = max @$data;
    return [map $_/$max, @$data];
}

sub add {
    if ( @_ > 2 ) { return add($_[0], add(@_[1..$#_])) }
    my ($l, $r) = @_;
    return [map $l->[$_]+$r->[$_], 0..$#$l];
}

### Sampled IIR stuff (incomplete)
sub runiir {
    my ($coeff, $length) = @_;
    
    my @in = (1, (0) x ($length-1));
    my @out = ();
    while ( @out < $length ) {
        my $next = $#out+1;
        my $value = 0;
        for my $i ( 0 .. $#{$coeff->{'a'}} ) {
            $value += $coeff->{'a'}->[$i] if $next-$i == 0;
        }
        for my $i ( 1 .. $#{$coeff->{'b'}} ) {
            $value += $coeff->{'b'}->[$i] * $out[$next-$i] if $next-$i >= 0;
        }
        push @out, $value;
    }

    return \@out;
}

################################################################################
### Helpers for want curves

sub read_curve_from {
    my ($fn) = @_;
    my $fh;
    if ( $fn eq "-" ) {
        $fh = \*STDIN;
    } else {
        open $fh, "<", $fn or die "Couldn't open $fn for reading: $!";
    }

    return read_curve_string(do { local $/; <$fh> });
}

sub read_curve_string {
    my ($str) = @_;

    my $samplerate;

    my $curve = [];
    for my $line ( grep { defined and length } split /[\n\r,]/, $str ) {
        if ( $line =~ /SAMPLERATE=(\d+)/ ) {
            $samplerate = $1;
        }
        next if $line =~ /^[#;]/;
        $line =~ s/^\s*//;
        $line =~ s/\s*$//;
        my ($freq, $power) = split /[=\s]+/, $line;
        next unless $freq  =~ /^\+?\d*(?:\.\d*)?(?:[eE][-+]?\d+)?$/;
        next unless $power =~ /^\+?\d*(?:\.\d*)?(?:[eE][-+]?\d+)?$/;
        push @$curve, [$freq, $power];
    }

    return ($curve, $samplerate);
}

sub make_wantcurve {
    my ($sr,$size,$pts) = @_;
    my @points = sort { $a->[0] <=> $b->[0] } @$pts;
    
    my $want = [];
    FREQ: for my $i ( 0 .. $size-1 ) {
        my $f = $i/$size*$sr;
        
        if ( $f > $sr/2 ) {
            $f = $sr-$f; # ah, imaginary numbers, how i love you so
        }

        # nearest-neighbor extension to undefined regions
        if ( $f <= $points[0][0] ) {
            push @$want, $points[0][1];
            next;
        } elsif ( $f >= $points[-1][0] ) {
            push @$want, $points[-1][1];
            next;
        }

        # linear interpolation between the two closest points
        my $lower;
        my $upper;
        {
            my ($min, $max) = (0, $#points);
            while ( $min < $max ) {
                my $mid = floor +($min+$max)/2;
                if ( $points[$mid][0] < $f ) {
                    # this point is okay, check above too
                    last if $min == $mid;
                    $min = $mid;
                } elsif ( $points[$mid][0] > $f ) {
                    # this point is not okay, check below only
                    last if $max == $mid-1;
                    $max = $mid-1;
                } else {
                    last;
                }
            }
            my $found = (grep { $points[$_][0] < $f } $min .. $max)[-1];
            $lower = $points[$found];
            $upper = $points[$found+1];
        }

        die unless defined $lower and defined $upper;

        my $p0 = ($f-$lower->[0])/($upper->[0]-$lower->[0]);
        my $val = $p0 * $upper->[1] + (1-$p0) * $lower->[1];

        push @$want, $val;
    }

    return $want;
}

################################################################################
### Helpers for anything

sub frequency_power {
    my ($sr, $data, $freq) = @_;

    my $real = sum map $data->[$_]*cos(pi2*$_*$freq/$sr), 0 .. $#$data;
    my $imag = sum map $data->[$_]*sin(pi2*$_*$freq/$sr), 0 .. $#$data;

    return sqrt($real**2 + $imag**2);
}

# FFT convolution
# you need to cut a sample off the end if you want to allow a spectral_inversion
# on two convolved linear phase filters
sub convolve {
    if ( @_ > 2 ) { return convolve($_[0], convolve(@_[1..$#_])) }
    my ($l, $r) = @_;
    
    my $sl = scalar @$l;
    my $sr = scalar @$r;

    my $fftsize = 2 ** ceil(log($sl+$sr)/log(2));

    my $lfftin = [map +($_,0), @$l];
    push @$lfftin, 0 while @$lfftin/2 < $fftsize;

    my $rfftin = [map +($_,0), @$r];
    push @$rfftin, 0 while @$rfftin/2 < $fftsize;

    my $lfft = Math::FFT->new($lfftin)->cdft;
    my $rfft = Math::FFT->new($rfftin)->cdft;

    my $offt = [];
    for my $i ( 0 .. $fftsize-1 ) {
        my ($a,$b,$c,$d) = ($lfft->[$i*2], $lfft->[$i*2+1], $rfft->[$i*2], $rfft->[$i*2+1]);
        $offt->[$i*2]   = $a*$c - $b*$d;
        $offt->[$i*2+1] = $a*$d + $c*$b;
    }

    my $outcx = Math::FFT->new($offt)->invcdft($offt);

    my $out = [];
    while ( @$outcx ) {
        push @$out, shift @$outcx; # keep real part
        shift @$outcx; # drop imaginary part
    }

    # truncate to only the neccessary size
    pop @$out while @$out > $sl + $sr;

    return $out;
}

################################################################################
### Meta-creation helpers (ready to use)

sub f_lowpass {
    my ($sr, $freq, $length) = @_;
    return normalize window $USEWINDOW, make_sinc $sr, $freq, $length;
}

sub f_highpass {
    my ($sr, $freq, $length) = @_;
    return spectral_inversion normalize window $USEWINDOW, make_sinc $sr, $freq, $length;
}

sub f_bandstop {
    my ($sr, $freqlow, $freqhigh, $length) = @_;
    return f_bandstop2($sr,$freqlow,$freqhigh,$length) if $freqlow == $freqhigh;
    my $hp = f_highpass($sr, $freqhigh, $length);
    my $lp = f_lowpass($sr, $freqlow, $length);
    return add $hp, $lp;
}

sub f_bandpass {
    my ($sr, $freqlow, $freqhigh, $length) = @_;
    return spectral_inversion f_bandstop $sr, $freqlow, $freqhigh, $length;
}

sub f_bandpass2 {
    my ($sr, $freqlow, $freqhigh, $length) = @_;
    my $l2 = floor $length/2;
    my $hp = f_highpass($sr, $freqlow, $l2);
    my $lp = f_lowpass($sr, $freqhigh, $l2);
    return convolve($hp,$lp);
}

sub f_bandstop2 {
    my ($sr, $freqlow, $freqhigh, $length) = @_;
    my $bp = f_bandpass2($sr, $freqlow, $freqhigh, $length);
    pop @$bp; # center it correctly for the inversion
    return spectral_inversion $bp;
}

sub f_bandstopdeep {
    my ($sr, $freq, $depth, $length) = @_;
    
    my $width = $sr/1000;
    my $step = $width/3;
    my $data = f_bandstop($sr, $freq-$width, $freq+$width, $length);
    my $power = frequency_power($sr, $data, $freq);

    my ($lastdir, $dir);
    while ( abs(log $power - log $depth) > 0.1 and $step > 0.000000001 ) {
        if ( $power < $depth ) {
            # too much power, decrease the width
            $width -= $step;
            $dir = '-';
        } else {
            $width += $step;
            $dir = '+';
        }
        $step *= 0.5 if defined $lastdir and $dir ne $lastdir;
        
        my $low = $freq-$width;
        my $high = $freq+$width;

        if ( $low < 0 or $high > $sr/2 ) {
            # woop! too big, time to stop, don't want to diverge
            $width = min($freq, $sr/2-$freq)-$sr/100000;
            $data = f_bandstop($sr, $freq-$width, $freq+$width, $length);
            return $data;
        }

        $data = f_bandstop($sr, $freq-$width, $freq+$width, $length);
        $power = frequency_power($sr, $data, $freq);

        $lastdir = $dir;
    }

    return $data;
}

sub f_custom {
    my ($sr, $wantpoints, $length) = @_;
    return make_custom_zerophase($sr, $wantpoints, $length);
}

################################################################################
### Main

sub help {
    print STDERR <<'EOF';
Usage:
    mkfilter {-o outfile | --analyze} -t type [-f freq[,freq]]
             [-c frequencycurve] [-C file] [-d depth] [-w window]
             [-r samplerate] [-l len] [-R selfconvolutions]
             [--analyzefactor=factor]
    mkfilter --analyze [--analyzefactor=factor] file.wav
    mkfilter -h

Filter types:
    lowpass, highpass (one frequency)
    bandpass, bandstop (one or two frequencies)
    bandstopdeep (one frequency, uses depth)
    custom (uses frequency curve)

Windows:
    blackman (default), hamming, hanning, barlett, rectangular

EOF
}

# argument parsing
my $outfile;
my $printanalysis = 0;
my $type;
my $freqa;
my $freqb;
my $length = 500;
my $depth = 0.01;
my $window = 'blackman';
my $samplerate = 44100;
my $convolutions = 0;
my $wantpoints;
my $analyzefactor = 1;

my $extmode = 0;
my $extfile;

while ( @ARGV ) {
    my $arg = shift @ARGV;
    if ( $arg eq "-o" ) {
        $outfile = shift @ARGV;
    } elsif ( $arg eq "--analyze" ) {
        $printanalysis = 1;
    } elsif ( $arg =~ /^--analyzefactor=(.+)$/ ) {
        $analyzefactor = $1;
    } elsif ( $arg eq "-t" ) {
        $type = shift @ARGV;
    } elsif ( $arg eq "-f" ) {
        ($freqa, $freqb) = split /,/, shift @ARGV;
    } elsif ( $arg eq "-r" ) {
        $samplerate = shift @ARGV;
    } elsif ( $arg eq "-l" ) {
        $length = shift @ARGV;
    } elsif ( $arg eq "-R" ) {
        $convolutions = shift @ARGV;
    } elsif ( $arg eq "-d" ) {
        $depth = shift @ARGV;
    } elsif ( $arg eq "-w" ) {
        $window = shift @ARGV;
    } elsif ( $arg eq "-c" ) {
        ($wantpoints, my $newsamplerate) = read_curve_string(shift @ARGV);
        $samplerate = $newsamplerate if defined $newsamplerate;
    } elsif ( $arg eq "-C" ) {
        ($wantpoints, my $newsamplerate) = read_curve_from(shift @ARGV);
        $samplerate = $newsamplerate if defined $newsamplerate;
    } elsif ( $arg eq "-h" or $arg eq "--help" ) {
        help; exit 0;
    } elsif ( -f $arg and not @ARGV and $printanalysis and not $outfile ) {
        $extmode = 1;
        $extfile = $arg;
    } else {
        die "Unknown argument $arg";
    }
}

my %typealiases = (
    lp => 'lowpass',
    hp => 'highpass',
    bp => 'bandpass',
    bp2 => 'bandpass2',
    bs => 'bandstop',
    bs2 => 'bandstop2',
    notch => 'bandstop',
    notch2 => 'bandstop2',
    dn => 'bandstopdeep',
    deepnotch => 'bandstopdeep',
    fit => 'custom',
);

$type = $typealiases{$type} if defined $type and exists $typealiases{$type};

$USEWINDOW = $window;

my $filter;
if ( not $extmode ) {
    # argument checking
    if ( not defined $outfile and not $printanalysis ) {
        print STDERR "Must give either an output file or use --analyze\n";
        print STDERR "(run with -h for help)\n";
        exit 1;
    }
    if ( not defined $type ) {
        print STDERR "Must give a filter type\n";
        exit 1;
    }
    if ( $type !~ /^(lowpass|highpass|bandpass2?|bandstop2?|bandstopdeep|custom)$/ ) {
        print STDERR "Filter type must be lowpass, highpass, bandpass, bandstop, or custom\n";
        exit 1;
    }
    if ( $type =~ /^(custom|fit)$/ ) {
        if ( not defined $wantpoints ) {
            print STDERR "Need a set of frequency points\n";
            exit 1;
        }
    } else {
        if ( not defined $freqa ) {
            print STDERR "Need a frequency\n";
            exit 1;
        }
    }
    if ( $type =~ /^(bp|bs|notch|bandpass|bandstop)2?$/ and not defined $freqb ) {
        $freqb = $freqa;
    }

    # and actual processing
    if ( $type eq 'lowpass' ) {
        $filter = f_lowpass($samplerate, $freqa, $length);
    } elsif ( $type eq 'highpass' ) {
        $filter = f_highpass($samplerate, $freqa, $length);
    } elsif ( $type eq 'bandpass' ) {
        $filter = f_bandpass($samplerate, $freqa, $freqb, $length);
    } elsif ( $type eq 'bandstop' ) {
        $filter = f_bandstop($samplerate, $freqa, $freqb, $length);
    } elsif ( $type eq 'bandpass2' ) {
        $filter = f_bandpass2($samplerate, $freqa, $freqb, $length);
    } elsif ( $type eq 'bandstop2' ) {
        $filter = f_bandstop2($samplerate, $freqa, $freqb, $length);
    } elsif ( $type eq 'bandstopdeep' ) {
        $filter = f_bandstopdeep($samplerate, $freqa, $depth, $length);
    } elsif ( $type eq 'custom' ) {
        $filter = f_custom($samplerate, $wantpoints, $length);
    } else { die }

    {
        my $outfilter = $filter;
        for my $i ( 1 .. $convolutions ) {
            $outfilter = convolve $outfilter, $filter;
        }
        $filter = $outfilter;
    }
} else {
    # extmode true
    ($samplerate, $filter) = read_file($extfile);
}


if ( $printanalysis ) {
    print_gnuplot $filter, $samplerate, $analyzefactor;
}

if ( defined $outfile ) {
    write_file $outfile, $filter, $samplerate;
}

