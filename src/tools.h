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

#ifndef __TOOLS_H__
#define __TOOLS_H__

#include "audiobuf.h"

enum window {
    window_blackman,
    window_hamming,
    window_barlett,
    window_hanning,
    window_rectangular
};

void apply_window(audiobuf *buf, enum window type);

audiobuf *make_sinc(int sr, float freq, int size);

// input MUST be normalized to dc=0
void spectral_inversion_td(audiobuf *buf);

void normalize_dc(audiobuf *buf);
void normalize_peak(audiobuf *buf);
void normalize_peak_if_clipped(audiobuf *buf);

audiobuf *convolve(audiobuf *a, audiobuf *b);
double frequency_power(audiobuf *buf, float freq);

#endif

