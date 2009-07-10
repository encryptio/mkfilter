#!/bin/sh
# Copyright (c) 2009 Jack Christopher Kastorff
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions, and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * The name Chris Kastorff may not be used to endorse or promote
#       products derived from this software without specific prior written
#       permission.

INPUT="$1"
OUTPUT="$2"
LENGTH="$3"
MAXSCALE="$4"

if [[ -z "$INPUT" || -z "$OUTPUT" || -z "$LENGTH" || -z "$MAXSCALE" ]]; then
    echo
    echo 'Usage:'
    echo '    mkunfilter input output filterlength maxscale'
    echo
    echo 'mkunfilter creates an approximation to a spectral inversion of any filter given'
    echo 'as input. The maxscale argument gives the maximum amplitude boost (as a multi-'
    echo 'plicative factor) that any given frequency will be multiplied by. The final'
    echo 'filter will be scaled such that no frequency is boosted, only attenuated.'
    echo
    echo 'mkunfilter ignores phase.'
    echo
    exit 1
fi

mkfilter --analyze --analyzefactor=0 "$INPUT" \
    | awk -v 'OFMT=%.15f' -v "ms=$MAXSCALE" \
        '/^[0-9]/ {
            if ($2<1/ms) $2=1;
                else $2=1/($2*ms);
            print $1, $2;
        }' \
    | mkfilter -t custom -C - -l "$LENGTH" -o "$OUTPUT"

