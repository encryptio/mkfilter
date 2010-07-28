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

#ifndef __WANTCURVE_H__
#define __WANTCURVE_H__

#include <stdio.h>
#include <stdbool.h>

typedef struct wantpoint {
    float freq;
    float power;
} wantpoint;

typedef struct wantcurve {
    wantpoint *pts;
    int ct;
    int sr;
    bool has_sr;
} wantcurve;

wantcurve *read_wantcurve_from_path(char *path);
wantcurve *read_wantcurve_from_file(FILE *fh);
wantcurve *read_wantcurve_from_string(char *str);

#endif

