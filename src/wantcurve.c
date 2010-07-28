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

#include "wantcurve.h"

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

wantcurve *read_wantcurve_from_path(char *path) {
    FILE *fh;
    if ( strcmp(path, "-") == 0 ) {
        fh = stdin;
    } else {
        if ( (fh = fopen(path, "rb")) == NULL )
            err(1, "Couldn't open %s for reading", path);
    }
    wantcurve *ret = read_wantcurve_from_file(fh);
    fclose(fh);
    return ret;
}

wantcurve *read_wantcurve_from_file(FILE *fh) {
    char *str;
    int malloced = 1024;
    int at = 0;
    if ( (str = malloc(malloced)) == NULL )
        err(1, "Couldn't malloc space for wantcurve");

    int read = 0;
    while ( (read = fread(str+at, 1, malloced-at, fh)) > 0 ) {
        malloced = malloced*1.5 + 1024;
        if ( (str = realloc(str, malloced)) == NULL )
            err(1, "Couldn't realloc space for wantcurve");
        at += read;
    }

    str[at] = '\0';

    wantcurve *ret = read_wantcurve_from_string(str);

    free(str);

    return ret;
}

wantcurve *read_wantcurve_from_string(char *str) {
    char *s = str;
    char *end = str+strlen(str);

    int ptsmalloced = 100;
    wantcurve *ret;
    if ( (ret = malloc(sizeof(wantcurve))) == NULL )
        err(1, "Couldn't malloc space for wantcurve structure");
    if ( (ret->pts = malloc(sizeof(wantpoint)*ptsmalloced)) == NULL )
        err(1, "Couldn't malloc space for wantpoint list");
    ret->ct = 0;
    ret->has_sr = false;

    // grab the sample rate, if it was given
    while ( s < end ) {
        if ( end-s <= 11 )
            break;
        if ( memcmp(s, "SAMPLERATE=", 11) == 0 ) {
            s += 11;
            if ( *s >= '0' && *s <= '9' ) {
                ret->sr = strtol(s, &s, 10);
                ret->has_sr = true;
            }
        }
        s++;
    }

    s = str;
    // now we look for the points.
    while ( s < end ) {
        // skip comments
        if ( *s == '#' || *s == ';' ) {
            while ( *s && *s != '\n' && *s != '\r' && *s != ',' ) s++;
            s++; continue;
        }

        // skip spaces
        while ( *s && isspace(*s) ) s++;

        // get a number or skip this line
        if ( !((*s >= '0' && *s <= '9') || *s == '+' || *s == '-' || *s == '.') ) {
            while ( *s && *s != '\n' && *s != '\r' && *s != ',' ) s++;
            s++; continue;
        }
        float freq = strtof(s, &s);

        // skip the seperator
        while ( *s && (isspace(*s) || *s == '=') ) s++;
        
        // get another number or skip this line
        if ( !((*s >= '0' && *s <= '9') || *s == '+' || *s == '-' || *s == '.') ) {
            while ( *s && *s != '\n' && *s != '\r' && *s != ',' ) s++;
            s++; continue;
        }
        float power = strtof(s, &s);

        while ( *s && *s != '\n' && *s != '\r' && *s != ',' ) s++;
        s++;

        // now we insert the point into the wantcurve
        if ( ret->ct+1 > ptsmalloced ) {
            ptsmalloced = ptsmalloced * 2;
            if ( (ret->pts = realloc(ret->pts, sizeof(wantpoint)*ptsmalloced)) == NULL )
                err(1, "Couldn't allocate space for wantcurve points");
        }

        ret->pts[ret->ct].freq  = freq;
        ret->pts[ret->ct].power = power;
        ret->ct++;
    }

    // insertion sort by frequency
    for (int i = 1; i < ret->ct; i++) {
        int j = i;
        wantpoint this = ret->pts[i];
        while ( j > 0 && ret->pts[j-1].freq > this.freq ) {
            j--;
            ret->pts[j+1] = ret->pts[j];
        }
        ret->pts[j] = this;
    }

    if ( ret->ct == 0 )
        errx(1, "Wantcurve appears to be empty.");

    return ret;
}

