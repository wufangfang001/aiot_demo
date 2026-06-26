#!/bin/bash

CC=${CC:-gcc}
PLATFORM="$1"
if [[ -z "$PLATFORM" || ("$PLATFORM" != "x86" && "$PLATFORM" != "arm") ]]; then
    echo "Usage: $0 [x86|arm]"
    exit 1
fi

RPATH_DIR="./lib/$PLATFORM"

$CC -Werror -O2 -o audio-call                          \
    src/amf_ringbuf.c                                  \
    src/audio_playback_wrapper_portaudio_impl.c        \
    src/audio_capture_wrapper_portaudio_impl.c         \
    sample-code/rtsa/agora_audio_call.c                \
    sample-code/rtsa/pacer.c                           \
    -Iinc                                              \
    -Llib/$PLATFORM                                    \
    -lagora-rtc-sdk -lportaudio -lfvad -lasound -lm -lpthread \
    -Wl,-rpath=${RPATH_DIR}