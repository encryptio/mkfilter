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
#include "kissfft/kiss_fft.h"

#include <math.h>
#include <err.h>

audiobuf *make_lowpass(int sr, float freq, int len, enum window window) {
    audiobuf *buf = make_sinc(sr, freq, len);
    apply_window(buf, window);
    normalize_dc(buf);
    return buf;
}

audiobuf *make_highpass(int sr, float freq, int len, enum window window) {
    audiobuf *buf = make_sinc(sr, freq, len);
    apply_window(buf, window);
    normalize_dc(buf);
    spectral_inversion_td(buf);
    return buf;
}

audiobuf *make_bandstop(int sr, float freqlow, float freqhi, int len, enum window window) {
    if ( freqlow == freqhi )
        return make_bandstop2(sr, freqlow, freqhi, len, window);

    audiobuf *hp = make_highpass(sr, freqhi,  len, window);
    audiobuf *lp = make_lowpass( sr, freqlow, len, window);

    add_buf(lp, hp);
    free_buf(hp);

    normalize_dc(lp);

    return lp;
}

audiobuf *make_bandpass(int sr, float freqlow, float freqhi, int len, enum window window) {
    audiobuf *buf = make_bandstop(sr, freqlow, freqhi, len, window);
    spectral_inversion_td(buf);
    return buf;
}

audiobuf *make_bandpass2(int sr, float freqlow, float freqhi, int len, enum window window) {
    int l2 = len/2;
    audiobuf *hp = make_highpass(sr, freqlow, l2, window);
    audiobuf *lp = make_lowpass( sr, freqhi,  l2, window);
    audiobuf *new = convolve(hp, lp);
    free_buf(hp);
    free_buf(lp);
    return new;
}

audiobuf *make_bandstop2(int sr, float freqlow, float freqhi, int len, enum window window) {
    audiobuf *buf = make_bandpass2(sr, freqlow, freqhi, len, window);
    buf->len--; // center correctly for the inversion
    spectral_inversion_td(buf);
    return buf;
}

audiobuf *make_bandstopdeep(int sr, float freq, double depth, int len, enum window window) {
    double width = sr/1000;
    double step = width/3;
    audiobuf *buf = make_bandstop(sr, freq-width, freq+width, len, window);
    double power = frequency_power(buf, freq);

    int lastdir = 0;
    int dir = 0;
    while ( fabs(log(power) - log(depth)) > 0.01 && step > 0.00000001 ) {
        if ( power < depth ) {
            width -= step;
            dir = -1;
        } else {
            width += step;
            dir = 1;
        }
        if ( lastdir != 0 && dir != lastdir ) step *= 0.5;

        free_buf(buf);

        float low = freq-width;
        float hi = freq+width;

        if ( low < 0 || hi > sr/2 ) {
            width = (freq < sr/2-freq ? freq : sr/2-freq) - sr/100000.0;
            return make_bandstop(sr, freq-width, freq+width, len, window);
        }

        buf = make_bandstop(sr, freq-width, freq+width, len, window);
        power = frequency_power(buf, freq);

        lastdir = dir;
    }

    return buf;
}

audiobuf *make_custom(int sr, wantcurve *curve, int len, enum window window) {
    if ( len % 2 == 0 ) len++; // odd size for proper window usage

    int fftsize = 1 << (int)(ceil(log2(len))+1);

    kiss_fft_cpx *in;
    if ( (in = malloc(sizeof(kiss_fft_cpx)*fftsize)) == NULL )
        err(1, "Couldn't malloc space for fft buffer");

    kiss_fft_cpx *out;
    if ( (out = malloc(sizeof(kiss_fft_cpx)*fftsize)) == NULL )
        err(1, "Couldn't malloc space for fft buffer");

    // make the input for the fft that has the proper response
    for (int i = 0; i < len; i++)
        in[i].i = 0;

    int pti = 0;
    for (int i = 0; i < fftsize; i++) {
        float f = (float)i/fftsize*sr;
        if ( f > sr/2 ) f = sr-f;

        if ( f < curve->pts[0].freq ) {
            // before the first point
            in[i].r = curve->pts[0].power;
        } else if ( curve->pts[curve->ct-1].freq < f ) {
            // after last point
            in[i].r = curve->pts[curve->ct-1].power;
        } else {
            // in-between points

            while ( curve->pts[pti+1].freq < f ) pti++;
            //           pts[pti  ] is the point immediately to the left  of f
            // likewise, pts[pti+1] is the point immediately to the right of f

            wantpoint *low = &(curve->pts[pti  ]);
            wantpoint *hi  = &(curve->pts[pti+1]);

            float p0 = (f-low->freq)/(hi->freq-low->freq);
            in[i].r = p0*hi->power + (1-p0)*low->power;
        }
    }

    kiss_fft_cfg cfg = kiss_fft_alloc(fftsize, 1, NULL, NULL);
    kiss_fft(cfg, in, out);
    kiss_fft_free(cfg);

    free(in);

    float *audio;
    if ( (audio = malloc(sizeof(float)*len)) == NULL )
        err(1, "Couldn't malloc space for audio");

    int center = len/2;
    for (int i = 0; i < center; i++)
        audio[i] = out[fftsize+i-center].r / fftsize;
    for (int i = 0; i < center; i++)
        audio[i+center] = out[i].r / fftsize;

    free(out);

    audiobuf *buf;
    if ( (buf = malloc(sizeof(audiobuf))) == NULL )
        err(1, "Couldn't malloc space for audiobuf structure");

    buf->td = audio;
    buf->len = len;
    buf->fd = NULL;
    buf->sr = sr;
    buf->type = audiobuf_td;

    apply_window(buf, window);

    return buf;
}

