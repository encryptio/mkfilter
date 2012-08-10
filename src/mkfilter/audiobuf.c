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

#include "audiobuf.h"
#include "../kissfft/kiss_fftr.h"

#include <err.h>
#include <string.h>

void convert_buf(audiobuf *buf, enum audiobuf_type target) {
    if ( buf->type == target )
        return;

    if ( target == audiobuf_fd ) {
        expand_buf(buf, 0);

        if ( buf->fd == NULL )
            if ( (buf->fd = malloc(sizeof(float)*(buf->len/2+1)*2)) == NULL )
                err(1, "Couldn't allocate %zu bytes for frequency domain samples", sizeof(float)*(buf->len/2+1)*2);

        kiss_fftr_cfg cfg = kiss_fftr_alloc(buf->len, 0, NULL, NULL);
        kiss_fftr(cfg, buf->td, (kiss_fft_cpx*) buf->fd);
        kiss_fftr_free(cfg);

        buf->type = audiobuf_fd;
    } else if ( target == audiobuf_td ) {
        if ( buf->td == NULL )
            if ( (buf->td = malloc(sizeof(float)*buf->len)) == NULL )
                err(1, "Couldn't allocate %zu bytes for time domain samples", sizeof(float)*buf->len);

        kiss_fftr_cfg cfg = kiss_fftr_alloc(buf->len, 1, NULL, NULL);
        kiss_fftri(cfg, (kiss_fft_cpx*) buf->fd, buf->td);
        kiss_fftr_free(cfg);

        buf->type = audiobuf_td;
    } else {
        errx(1, "not reached");
    }
}

void expand_buf(audiobuf *buf, int minsize) {
    convert_buf(buf, audiobuf_td);

    int oldsize = buf->len;
    int newsize = kiss_fftr_next_fast_size_real(oldsize > minsize ? oldsize : minsize);

    if ( newsize > oldsize ) {
        if ( (buf->td = realloc(buf->td, sizeof(float)*newsize)) == NULL )
            err(1, "Couldn't realloc %zu bytes for expanded time domain samples", sizeof(float)*newsize);

        for (int i = oldsize; i < newsize; i++)
            buf->td[i] = 0;

        buf->len = newsize;
    }
}

audiobuf *duplicate_buf(audiobuf *buf) {
    convert_buf(buf, audiobuf_td); // TODO: allow duplication with frequency domain samples

    audiobuf *new;
    if ( (new = malloc(sizeof(audiobuf))) == NULL )
        err(1, "Couldn't allocate memory for audiobuf struct");

    memcpy(new, buf, sizeof(audiobuf));
    
    if ( (new->td = malloc(sizeof(float)*new->len)) == NULL )
        err(1, "Couldn't allocate %zu bytes for time domain samples", sizeof(float)*new->len);

    memcpy(new->td, buf->td, new->len*sizeof(float));
    new->fd = NULL;

    return new;
}

void add_buf(audiobuf *dst, audiobuf *summand) {
    convert_buf(dst, audiobuf_td);
    convert_buf(summand, audiobuf_td);

    for (int i = 0; i < dst->len; i++)
        dst->td[i] += summand->td[i];
}

void free_buf(audiobuf *buf) {
    if ( buf->td != NULL )
        free(buf->td);
    if ( buf->fd != NULL )
        free(buf->fd);
    free(buf);
}

