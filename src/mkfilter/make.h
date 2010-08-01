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

#ifndef __MAKE_H__
#define __MAKE_H__

#include "audiobuf.h"
#include "wantcurve.h"
#include "tools.h"

audiobuf *make_lowpass(int sr, float freq, int len, enum window window);
audiobuf *make_highpass(int sr, float freq, int len, enum window window);
audiobuf *make_bandstop(int sr, float freqlow, float freqhi, int len, enum window window);
audiobuf *make_bandpass(int sr, float freqlow, float freqhi, int len, enum window window);
audiobuf *make_bandpass2(int sr, float freqlow, float freqhi, int len, enum window window);
audiobuf *make_bandstop2(int sr, float freqlow, float freqhi, int len, enum window window);
audiobuf *make_bandstopdeep(int sr, float freq, double depth, int len, enum window window);
audiobuf *make_custom(int sr, wantcurve *curve, int len, enum window window);

#endif

