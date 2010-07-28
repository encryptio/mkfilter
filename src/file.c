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

#include "file.h"

#include <sndfile.h>

#include <stdlib.h>
#include <err.h>
#include <string.h>

audiobuf *read_file(char *path) {
    audiobuf *buf;
    if ( (buf = malloc(sizeof(audiobuf))) == NULL )
        err(1, "Couldn't malloc space for audiobuf struct");

    SF_INFO info;
    memset(&info, 0, sizeof(SF_INFO));

    SNDFILE *sf;
    if ( (sf = sf_open(path, SFM_READ, &info)) == NULL )
        errx(1, "Couldn't open input file %s for reading", path);

    if ( info.channels > 1 )
        errx(1, "Bad input file %s: has too many channels (%d, need 1)", path, info.channels);

    buf->sr = info.samplerate;
    buf->len = info.frames;
    buf->fd = NULL;
    buf->type = audiobuf_td;

    if ( (buf->td = malloc(sizeof(float)*buf->len)) == NULL )
        err(1, "Couldn't malloc %d bytes for input buffer for %s", sizeof(float)*buf->len, path);

    sf_readf_float(sf, buf->td, buf->len);
    
    sf_close(sf);

    expand_buf(buf, 0);

    return buf;
}

void write_file(audiobuf *buf, char *path) {
    convert_buf(buf, audiobuf_td);

    SF_INFO info;
    memset(&info, 0, sizeof(SF_INFO));

    info.samplerate = buf->sr;
    info.channels = 1;
    info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_24 | SF_ENDIAN_FILE;

    SNDFILE *sf;
    if ( (sf = sf_open(path, SFM_WRITE, &info)) == NULL )
        errx(1, "Couldn't open output file %s for writing", path);

    sf_writef_float(sf, buf->td, buf->len);

    if ( sf_close(sf) )
        errx(1, "Couldn't close output file for %s: %s", path, sf_strerror(sf));
}

