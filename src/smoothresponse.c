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

#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <err.h>
#include <string.h>
#include <ctype.h>

void usage(char *name) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "    %s -w octaves [-o outfile] [file]\n", name);
    fprintf(stderr, "    %s -h\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "%s smooths the given response (in mkfilter --analyze format)\n", name);
    fprintf(stderr, "in the per-frequency amplitude domain. It removes the phase information, as it\n");
    fprintf(stderr, "is corrupted by this smoothing.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "It samples the same frequencies as are in the input, so its intended behavior\n");
    fprintf(stderr, "is only reached when the input is dense.\n");
    fprintf(stderr, "\n");
}

static struct option long_options[] = {
    { "output", 1, NULL, 'o' },
    { "width", 1, NULL, 'w' },
    { "window", 1, NULL, 'w' },
    { "help", 0, NULL, 'h' },
    { NULL, 0, NULL, 0 }
};

typedef struct pt {
    double freq;
    double amp;
} pt;

int compare_pt(const void *a, const void *b) {
    double fa = ((pt*)a)->freq;
    double fb = ((pt*)b)->freq;
    if ( fa > fb ) return  1;
    if ( fa < fb ) return -1;
    return 0;
}

#define BUF_SIZE 1000
void process(FILE *in, FILE *out, double width) {
    int at = 0;
    int alloced = 100;
    pt *pts;
    if ( (pts = malloc(sizeof(pt)*alloced)) == NULL )
        err(1, "Couldn't allocate space for points array");

    // step 1: read the input data into *pts
    char line[BUF_SIZE];
    while ( fgets(line, BUF_SIZE, in) ) {
        char *p = line;

        while ( *p && isspace(*p) ) p++;

        if ( *p == ';' || *p == '#' ) {
            // keep comments - they have important information like the sample rate
            fprintf(out, "%s", line);
            continue;
        }

        char *oldp = p;
        pts[at].freq = strtod(p, &p);
        if ( oldp == p || !*p ) continue;

        while ( *p && isspace(*p) ) p++;

        oldp = p;
        pts[at].amp = strtod(p, &p);
        if ( oldp == p ) continue;

        // ignore the rest of the line, if any

        at++;

        if ( at >= alloced ) {
            alloced = alloced*2 + 50;
            if ( (pts = realloc(pts, sizeof(pt)*alloced)) == NULL )
                err(1, "Couldn't realloc space for points array");
        }
    }

    // step 2: sort by frequency
    qsort(pts, at, sizeof(pt), compare_pt);

    // step 3: scan, printing the results as we go
    double sum = pts[0].amp;
    int from = 0;
    int to = 0;
    for (int i = 0; i < at; i++) {
        double topf = pts[i].freq * (1+width);
        double botf = pts[i].freq / (1+width);

        while ( at-1 > to+1   && pts[to  ].freq < topf ) sum += pts[to++  ].amp;
        while ( at-1 > from+1 && pts[from].freq < botf ) sum -= pts[from++].amp;

        fprintf(out, "%.15f %.15f\n", pts[i].freq, sum/(to-from+1));
    }
}

int main(int argc, char **argv) {
    char *progname = argv[0];

    char *outfile = NULL;
    bool window_set = false;
    double window = 0;

    while ( true ) {
        int c = getopt_long(argc, argv, "o:w:h", long_options, NULL);
        if ( c == -1 )
            break;

        switch (c) {
            case 'o':
                outfile = strdup(optarg);
                break;

            case 'w':
                window_set = true;
                window = strtod(optarg, &optarg);
                if ( *optarg )
                    errx(1, "Bad window specifier");
                break;

            case 'h':
                usage(progname);
                exit(1);
                break;

            default:
                errx(1, "Not reached");
        }
    }

    if ( !window_set )
        errx(1, "Need a window (run with -h for help)");

    FILE *read_from;
    FILE *write_to;

    if ( optind+1 == argc ) {
        if ( (read_from = fopen(argv[optind], "r")) == NULL )
            err(1, "Couldn't open %s for reading", argv[optind]);

    } else if ( optind == argc ) {
        read_from = stdin;
    } else {
        errx(1, "Too many arguments");
    }

    if ( outfile == NULL || strcmp(outfile, "-") == 0 )
        write_to = stdout;
    else
        if ( (write_to = fopen(outfile, "w")) == NULL )
            err(1, "Couldn't open %s for writing", outfile);

    process(read_from, write_to, window);

    if ( fclose(read_from) )
        err(1, "Couldn't close reading filehandle");
    if ( fclose(write_to) )
        err(1, "Couldn't close writing filehandle");

    return 0;
}

