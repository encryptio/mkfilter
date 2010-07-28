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

#include "analyze.h"

#include <math.h>

#define PI 3.1415926535897932384626433832795028841971693993

void analyze_filter(audiobuf *buf, FILE *fh, int analyzefactor) {
    int wantsize = 1 << (int)(ceil(log2(buf->len))+analyzefactor);
    if ( wantsize < 1<<14 )
        wantsize = 1<<14;

    convert_buf(buf, audiobuf_td);
    expand_buf(buf, wantsize);
    convert_buf(buf, audiobuf_fd);

    fprintf(fh, "# SAMPLERATE=%d\n", buf->sr);
    fprintf(fh, "# frequency magnitude phase\n\n");

    float lastphase = 0;
    float runningphase = 0;
    for (int i = 0; i < buf->len/2; i++) {
        float freq = buf->sr*(float)i/(buf->len);
        float real = buf->fd[i*2];
        float imag = buf->fd[i*2+1];

        // to polar
        float mag = sqrtf(real*real + imag*imag);
        float arg = atan2f(real, imag);

        // unwrap phase
        float phasediff = arg - lastphase;
        while ( phasediff >  PI ) phasediff -= 2*PI;
        while ( phasediff < -PI ) phasediff += 2*PI;
        runningphase += phasediff;
        lastphase = arg;

        fprintf(fh, "%.14f\t%.14f\t%.14f\n", freq, mag, runningphase);
    }
}

