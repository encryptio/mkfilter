/*
 * Copyright (c) 2010 Jack Christopher Kastorff
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name Chris Kastorff may not be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 */

#include "make.h"
#include "file.h"
#include "analyze.h"
#include "wantcurve.h"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include <err.h>
#include <string.h>

void usage(char *name) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "    %s {-o outfile | --analyze} -t type [-f freq[,freq]]\n", name);
    fprintf(stderr, "       [-c frequencycurve] [-C file] [-d depth] [-w window]\n");
    fprintf(stderr, "       [-r samplerate] [-l len] [-R convolutions]\n");
    fprintf(stderr, "       [--analyze-factor factor]\n");
    fprintf(stderr, "    %s --analyze [--analyze-factor factor] input.wav\n", name);
    fprintf(stderr, "    %s -h\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Filter types:\n");
    fprintf(stderr, "    lowpass, highpass (one frequency)\n");
    fprintf(stderr, "    bandpass, bandstop (one or two frequencies)\n");
    fprintf(stderr, "    bandstopdeep (one frequency, uses depth)\n");
    fprintf(stderr, "    custom (uses frequency curve)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Windows:\n");
    fprintf(stderr, "    blackman (default), hamming, hanning, barlett, rectangular\n");
    fprintf(stderr, "\n");
}

static struct option long_options[] = {
    { "output", 1, NULL, 'o' },
    { "analyze", 0, NULL, 'a' },
    { "analyse", 0, NULL, 'a' },
    { "help", 0, NULL, 'h' },
    { "type", 1, NULL, 't' },
    { "frequency", 1, NULL, 'f' },
    { "frequencies", 1, NULL, 'f' },
    { "frequency-curve", 1, NULL, 'c' },
    { "frequency-curve-file", 1, NULL, 'C' },
    { "depth", 1, NULL, 'd' },
    { "window", 1, NULL, 'w' },
    { "sample-rate", 1, NULL, 'r' },
    { "length", 1, NULL, 'l' },
    { "convolutions", 1, NULL, 'R' },
    { "analyzefactor", 1, NULL, 'A' },
    { "analysefactor", 1, NULL, 'A' },
    { "analyze-factor", 1, NULL, 'A' },
    { "analyse-factor", 1, NULL, 'A' },
    { NULL, 0, NULL, 0 }
};

enum filtertype {
    nofiltertype,
    lowpass,
    highpass,
    bandpass,
    bandpass2,
    bandstop,
    bandstop2,
    bandstopdeep,
    custom
};

bool handle_type(char *name, enum filtertype *type) {
    if ( strcmp(name, "lowpass") == 0 || strcmp(name, "lp") == 0 ) {
        *type = lowpass;
    } else if ( strcmp(name, "highpass") == 0 || strcmp(name, "hp") == 0 ) {
        *type = highpass;
    } else if ( strcmp(name, "bandpass") == 0 || strcmp(name, "bp") == 0 ) {
        *type = bandpass;
    } else if ( strcmp(name, "bandpass2") == 0 || strcmp(name, "bp2") == 0 ) {
        *type = bandpass2;
    } else if ( strcmp(name, "bandstop") == 0 || strcmp(name, "bs") == 0 || strcmp(name, "notch") == 0 ) {
        *type = bandstop;
    } else if ( strcmp(name, "bandstop2") == 0 || strcmp(name, "bs2") == 0 || strcmp(name, "notch2") == 0 ) {
        *type = bandstop2;
    } else if ( strcmp(name, "bandstopdeep") == 0 || strcmp(name, "deepnotch") == 0 || strcmp(name, "dn") == 0 ) {
        *type = bandstopdeep;
    } else if ( strcmp(name, "custom") == 0 || strcmp(name, "fit") == 0 ) {
        *type = custom;
    } else {
        return false;
    }
    return true; // didn't hit the last else clause, something worked
}

bool handle_window(char *name, enum window *window) {
    if ( strcmp(name, "blackman") == 0 ) {
        *window = window_blackman;
    } else if ( strcmp(name, "hamming") == 0 ) {
        *window = window_hamming;
    } else if ( strcmp(name, "barlett") == 0 ) {
        *window = window_barlett;
    } else if ( strcmp(name, "cosine") == 0 || strcmp(name, "hanning") == 0 ) {
        *window = window_hanning;
    } else if ( strcmp(name, "rectangular") == 0 || strcmp(name, "none") == 0 ) {
        *window = window_rectangular;
    } else {
        return false;
    }
    return true;
}

int main(int argc, char **argv) {
    char *progname = argv[0];

    if ( argc == 1 )
        errx(1, "Requires arguments (run with -h for help)");

    // argument variables
    char *outfile = NULL;

    bool analyze = false;
    int analyzefactor = 1;

    enum filtertype type = nofiltertype;
    enum window window = window_blackman;

    float freq1 = 0;
    float freq2 = 0;
    int freqs_set = 0;

    double depth = 0.01;

    int samplerate = 44100;
    bool samplerate_set = false;
    int length = 1000;
    int convolutions = 0;

    wantcurve *curve = NULL;

    while ( true ) {
        int c = getopt_long(argc, argv, "o:at:f:c:C:d:w:r:l:R:A:h", long_options, NULL);
        if ( c == -1 )
            break;

        switch (c) {
            case 'o':
                outfile = strdup(optarg);
                break;

            case 'a':
                analyze = true;
                break;

            case 'A':
                analyzefactor = strtol(optarg, &optarg, 10);
                if ( *optarg || analyzefactor < 0 )
                    errx(1, "Bad analyze factor specifier");
                break;


            case 't':
                if ( !handle_type(optarg, &type) )
                    errx(1, "Unknown filter type %s", optarg);
                break;

            case 'f':
                freq1 = strtof(optarg, &optarg);
                freqs_set = 1;
                if ( *optarg == ',' ) {
                    optarg++;
                    freq2 = strtof(optarg, &optarg);
                    freqs_set = 2;
                }
                if ( *optarg )
                    errx(1, "Bad frequency specifier");
                break;

            case 'c':
                curve = read_wantcurve_from_string(optarg);
                if ( !samplerate_set && curve->has_sr )
                    samplerate = curve->sr;
                break;

            case 'C':
                curve = read_wantcurve_from_path(optarg);
                if ( !samplerate_set && curve->has_sr )
                    samplerate = curve->sr;
                break;

            case 'd':
                depth = strtod(optarg, &optarg);
                if ( *optarg )
                    errx(1, "Bad depth specifier");
                break;

            case 'w':
                if ( !handle_window(optarg, &window) )
                    errx(1, "Unknown window type %s", optarg);
                break;

            case 'r':
                samplerate = strtol(optarg, &optarg, 10);
                samplerate_set = true;
                if ( *optarg )
                    errx(1, "Bad sample rate specifier");
                break;

            case 'l':
                length = strtol(optarg, &optarg, 10);
                if ( *optarg )
                    errx(1, "Bad length specifier");
                break;

            case 'R':
                convolutions = strtol(optarg, &optarg, 10);
                if ( *optarg )
                    errx(1, "Bad convolution count specifier");
                break;

            case 'h':
                usage(progname);
                exit(1);
                break;

            default:
                errx(1, "Not reached");
        }
    }

    bool extmode = false;
    char *extfile = NULL;

    if ( optind != argc ) {
        // we have extra arguments

        extmode = true;
        extfile = argv[optind++];

        if ( optind != argc )
            errx(1, "Too many arguments");
    }

    audiobuf *buf;

    // set the audiobuf to the proper thing
    if ( extmode ) {
        buf = read_file(extfile);
    } else {
        if ( !analyze && !outfile )
            errx(1, "Must give either an output file or use --analyze");
        if ( type == nofiltertype )
            errx(1, "Must give a filter type");
        if ( type == custom && !curve )
            errx(1, "Need a wantcurve for the custom fit filter");
        if ( type != custom && !freqs_set )
            errx(1, "Need a frequency");

        if ( freqs_set == 1 )
            freq2 = freq1;

        switch ( type ) {
            case lowpass:
                buf = make_lowpass(samplerate, freq1, length, window);
                break;

            case highpass:
                buf = make_highpass(samplerate, freq1, length, window);
                break;

            case bandpass:
                buf = make_bandpass(samplerate, freq1, freq2, length, window);
                break;

            case bandpass2:
                buf = make_bandpass2(samplerate, freq1, freq2, length, window);
                break;

            case bandstop:
                buf = make_bandstop(samplerate, freq1, freq2, length, window);
                break;

            case bandstop2:
                buf = make_bandstop2(samplerate, freq1, freq2, length, window);
                break;

            case bandstopdeep:
                buf = make_bandstopdeep(samplerate, freq1, depth, length, window);
                break;

            case custom:
                buf = make_custom(samplerate, curve, length, window);
                break;

            default:
                errx(1, "Not reached");
        }

        if ( convolutions ) {
            audiobuf *orig = duplicate_buf(buf);
            for (int i = 0; i < convolutions; i++) {
                audiobuf *new = convolve(orig, buf);
                free_buf(buf);
                buf = new;
            }
            free_buf(orig);
        }
    }

    // TODO: make this chatty
    normalize_peak_if_clipped(buf);
    
    if ( analyze )
        analyze_filter(buf, stdout, analyzefactor);

    if ( outfile )
        write_file(buf, outfile);
}

