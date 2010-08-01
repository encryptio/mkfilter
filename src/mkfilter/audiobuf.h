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

#ifndef __AUDIOBUF_H__
#define __AUDIOBUF_H__

#include <inttypes.h>

enum audiobuf_type {
    audiobuf_td,
    audiobuf_fd
};

typedef struct audiobuf {
    float *td;
    float *fd;
    int len; // length in time samples. when type=fd, *fd has length len/2+1 complex points
    enum audiobuf_type type;
    int sr;
} audiobuf;

void convert_buf(audiobuf *buf, enum audiobuf_type target);
void expand_buf(audiobuf *buf, int minsize);
audiobuf *duplicate_buf(audiobuf *buf);
void add_buf(audiobuf *dst, audiobuf *summand);
void free_buf(audiobuf *buf);

#endif

