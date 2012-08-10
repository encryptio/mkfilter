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

#include "../kissfft/kiss_fftr.h"

#include <math.h>
#include <err.h>
#include <assert.h>
#include <stdlib.h>

#define PI 3.1415926535897932384626433832795028841971693993

void apply_window(audiobuf *buf, enum window type) {
    convert_buf(buf, audiobuf_td);

    float l = buf->len;

    switch ( type ) {
        case window_blackman:
            for (int i = 0; i < buf->len; i++)
                buf->td[i] *= 0.42 - 0.5 * cosf(2*PI * (float)i / l) + 0.08 * cosf(4*PI * (float)i / l);
            break;

        case window_hamming:
            for (int i = 0; i < buf->len; i++)
                buf->td[i] *= 0.54 - 0.46*cosf(2*PI * (float)i / l);
            break;

        case window_barlett:
            for (int i = 0; i < buf->len; i++)
                buf->td[i] *= 1.0 - fabsf(1.0 - (float)i / l * 2.0);
            break;

        case window_hanning:
            for (int i = 0; i < buf->len; i++)
                buf->td[i] *= 0.5 - 0.5*cosf(2*PI * (float)i / l);
            break;

        case window_rectangular:
            break;

        default:
            err(1, "not reached");
    }
}

audiobuf *make_sinc(int sr, float freq, int size) {
    if ( size % 2 == 0 ) size++; // must have odd size, otherwise symmetry causes nonlinear phase in other operations

    float fc = freq/sr;
    int center = size/2;

    audiobuf *buf;
    if ( (buf = malloc(sizeof(audiobuf))) == NULL )
        err(1, "Couldn't allocate space for audiobuf struct");
    if ( (buf->td = malloc(sizeof(float)*size)) == NULL )
        err(1, "Couldn't allocate %zu bytes for time domain samples", sizeof(float)*size);
    buf->fd = NULL;
    buf->len = size;
    buf->type = audiobuf_td;
    buf->sr = sr;

    for (int i = 0; i < size; i++) {
        if ( i == center ) {
            buf->td[i] = 2*PI * fc;
        } else {
            buf->td[i] = sinf(2*PI * fc * (i-center)) / (i-center);
        }
    }

    return buf;
}

void spectral_inversion_td(audiobuf *buf) {
    convert_buf(buf, audiobuf_td);
    for (int i = 0; i < buf->len; i++)
        buf->td[i] = -buf->td[i];
    buf->td[buf->len/2] += 1;
}

void normalize_dc(audiobuf *buf) {
    convert_buf(buf, audiobuf_td);
    float total = 0;
    for (int i = 0; i < buf->len; i++)
        total += buf->td[i];
    for (int i = 0; i < buf->len; i++)
        buf->td[i] /= total;
}

void normalize_peak(audiobuf *buf) {
    convert_buf(buf, audiobuf_td);
    float max = 0;
    for (int i = 0; i < buf->len; i++)
        if ( max < fabsf(buf->td[i]) )
            max = fabsf(buf->td[i]);
    for (int i = 0; i < buf->len; i++)
        buf->td[i] /= max;
}

void normalize_peak_if_clipped(audiobuf *buf) {
    convert_buf(buf, audiobuf_td);
    float max = 0;
    for (int i = 0; i < buf->len; i++)
        if ( max < fabsf(buf->td[i]) )
            max = fabsf(buf->td[i]);
    if ( max > 1 ) {
        fprintf(stderr, "mkfilter: WARNING: Scaling filter by %.10f to avoid clipping.\n", 1/max);
        for (int i = 0; i < buf->len; i++)
            buf->td[i] /= max;
    }
}

audiobuf *convolve(audiobuf *a, audiobuf *b) {
    convert_buf(a, audiobuf_td);
    convert_buf(b, audiobuf_td);

    assert(a->sr == b->sr);

    int fftsize = 1 << (int)(ceil(log2(a->len + b->len)));

    float *samp;
    if ( (samp = malloc(sizeof(float)*fftsize)) == NULL )
        err(1, "Couldn't allocate %zu bytes for fft in convolution", sizeof(float)*fftsize);

    kiss_fft_cpx *afft;
    kiss_fft_cpx *bfft;

    if ( (afft = malloc(sizeof(kiss_fft_cpx)*(fftsize+1))) == NULL )
        err(1, "Couldn't allocate %zu bytes for ffta in convolution", sizeof(kiss_fft_cpx)*(fftsize/2+1));
    if ( (bfft = malloc(sizeof(kiss_fft_cpx)*(fftsize+1))) == NULL )
        err(1, "Couldn't allocate %zu bytes for fftb in convolution", sizeof(kiss_fft_cpx)*(fftsize/2+1));

    kiss_fftr_cfg cfg = kiss_fftr_alloc(fftsize, 0, NULL, NULL);

    for (int i = 0; i < fftsize; i++)
        samp[i] = i < a->len ? a->td[i] : 0;
    kiss_fftr(cfg, samp, afft);

    for (int i = 0; i < fftsize; i++)
        samp[i] = i < b->len ? b->td[i] : 0;
    kiss_fftr(cfg, samp, bfft);

    kiss_fftr_free(cfg);

    for (int i = 0; i < fftsize/2+1; i++) {
        float re = afft[i].r*bfft[i].r - afft[i].i*bfft[i].i;
        float im = afft[i].r*bfft[i].i + afft[i].i*bfft[i].r;
        afft[i].r = re;
        afft[i].i = im;
    }

    free(bfft);

    cfg = kiss_fftr_alloc(fftsize, 1, NULL, NULL);
    kiss_fftri(cfg, afft, samp);
    kiss_fftr_free(cfg);

    for (int i = 0; i < fftsize; i++)
        samp[i] /= fftsize;

    free(afft);

    audiobuf *buf;
    if ( (buf = malloc(sizeof(audiobuf))) == NULL )
        err(1, "Couldn't allocate space for audiobuf struct");

    buf->td = samp;
    buf->fd = NULL;
    buf->sr = a->sr;
    buf->len = a->len + b->len;
    buf->type = audiobuf_td;

    return buf;
}

double frequency_power(audiobuf *buf, float freq) {
    convert_buf(buf, audiobuf_td);
    double re = 0;
    double im = 0;
    for (int i = 0; i < buf->len; i++) {
        re += buf->td[i] * cos(PI*2*i*freq/buf->sr);
        im += buf->td[i] * sin(PI*2*i*freq/buf->sr);
    }
    return sqrt(re*re + im*im);
}

