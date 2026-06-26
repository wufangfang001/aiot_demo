#!/bin/bash

CC=${CC:-gcc}
PLATFORM="$1"
if [[ -z "$PLATFORM" || ("$PLATFORM" != "x86" && "$PLATFORM" != "arm") ]]; then
    echo "Usage: $0 [x86|arm]"
    exit 1
fi

RPATH_DIR="./lib/$PLATFORM"

$CC -Werror -O2 -o audio-playback                    \
    src/amf_ringbuf.c                                \
    src/audio_playback_wrapper_portaudio_impl.c      \
    sample-code/audio-playback/audio_playback_demo.c \
    -Iinc                                            \
    -Llib/$PLATFORM                                  \
    -lportaudio -lasound -lm -lpthread               \
    -Wl,-rpath=${RPATH_DIR}
