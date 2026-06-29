#!/bin/bash

CC=${CC:-gcc}
PLATFORM="${1:-}"
BUILD_TYPE="${2:-release}"
if [[ -z "$PLATFORM" ]]; then
    machine="$(uname -m)"
    case "$machine" in
        x86_64|i386|i686|i?86)
            PLATFORM="x86"
            ;;
        aarch64|arm64|armv7l|armv8l)
            PLATFORM="arm"
            ;;
        *)
            echo "Unable to detect platform from uname -m: $machine"
            echo "Usage: $0 [x86|arm] [release|debug]"
            exit 1
            ;;
    esac
fi
if [[ "$PLATFORM" != "x86" && "$PLATFORM" != "arm" ]]; then
    echo "Usage: $0 [x86|arm] [release|debug]"
    exit 1
fi

if [[ "$BUILD_TYPE" == "debug" || "${DEBUG:-0}" == "1" ]]; then
    CFLAGS="-g -O0"
else
    CFLAGS="-Werror -O2"
fi

RPATH_DIR="./lib/$PLATFORM"

$CC $CFLAGS -o audio-playback                    \
    src/amf_ringbuf.c                                \
    src/audio_playback_wrapper_portaudio_impl.c      \
    sample-code/audio-playback/audio_playback_demo.c \
    -Iinc                                            \
    -Llib/$PLATFORM                                  \
    -lportaudio -lasound -lm -lpthread               \
    -Wl,-rpath=${RPATH_DIR}
