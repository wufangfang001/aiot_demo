/*************************************************************
 *
 * This is a part of the Agora Media Framework Library.
 * Copyright (C) 2021 Agora IO
 * All rights reserved.
 *
 *************************************************************/
#include "audio_playback_wrapper.h"
#include "portaudio.h"
#include "amf_ringbuf.h"
#include "amf_log.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>

#define TAG   "[playback]"

typedef struct {
  PaStream*        pa_stream;
  ringbuf_handle_t playback_ringbuf;
  bool             pa_stopping;
  uint32_t         volume_percent;
  uint32_t         bit_width;
} audio_playback_t;

static void __volume_adjust(uint8_t *pcm, uint32_t len,
                            uint32_t bit_width,
                            int32_t volume_percent)
{
  /* do nothing when volume percent 100, which as default value */
  if (volume_percent == 100) {
    return;
  }

  /* silent process */
  if (volume_percent == 0) {
    memset(pcm, 0, len);
    return;
  }

  /* process 16bit */
  if (bit_width == 16) {
    int32_t tmp;
    int16_t *p = (int16_t *)pcm;
    for (int i = 0; i < len; i += 2) {
      tmp = (int32_t)(*p) * volume_percent / 100;
      *p++ = (int16_t)tmp;
    }
    return;
  }

  /* process 8bit */
  if (bit_width == 8) {
    int16_t tmp;
    int8_t *p = (int8_t *)pcm;
    for (int i = 0; i < len; i++) {
      tmp = (int16_t)(*p) * volume_percent / 100;
      pcm[i] = (uint8_t)tmp;
    }
    return;
  }

  /* 24bit is not used, donnt process now */
}

static int __portaudio_playback(const void* inputBuffer,
                                void* outputBuffer,
                                unsigned long numSamples,
                                const PaStreamCallbackTimeInfo* timeInfo,
                                PaStreamCallbackFlags statusFlags,
                                void* userData)
{
  audio_playback_t *audio_playback = (audio_playback_t *)userData;
  char pcm[numSamples << 1];
  int err = amf_ringbuf_read(pcm, numSamples << 1, audio_playback->playback_ringbuf);
  if (err < 0) {
    memset(outputBuffer, 0, numSamples << 1);
    LOGW(TAG, "audio playback underrun. feed silence audio");
    return audio_playback->pa_stopping ? paAbort : paContinue;
  }

  __volume_adjust(pcm, numSamples << 1,
                  audio_playback->bit_width,
                  (int32_t)audio_playback->volume_percent);

  memcpy(outputBuffer, pcm, numSamples << 1);
  return audio_playback->pa_stopping ? paAbort : paContinue;
}

static PaSampleFormat __get_sample_format_by_bitwidth(uint32_t bitwidth)
{
  assert(bitwidth == 8 || bitwidth == 16 || bitwidth == 24);

  if (8 == bitwidth) {
    return paInt8;
  }

  if (16 == bitwidth) {
    return paInt16;
  }

  if (24 == bitwidth) {
    return paInt24;
  }

  return paInt16;
}

audio_playback_handle_t audio_playback_wrapper_create(uint32_t sample_rate,
                                                      uint32_t bit_width,
                                                      uint32_t channel_num)
{
  audio_playback_t *audio_playback = malloc(sizeof(audio_playback_t));
  assert(audio_playback);
  memset(audio_playback, 0, sizeof(audio_playback_t));
  int cache_2_second_len = channel_num * sample_rate * bit_width / 8 * 2;
  LOGT(TAG, "playback buffer size=%d", cache_2_second_len);

  audio_playback->playback_ringbuf = amf_ringbuf_create(cache_2_second_len);
  assert(audio_playback->playback_ringbuf);

  audio_playback->bit_width      = bit_width;
  audio_playback->volume_percent = 100;

  PaError err = Pa_Initialize();
  if (err != paNoError) {
    LOGE(TAG, "Failed to initialize PortAudio. err=%s", Pa_GetErrorText(err));
    free(audio_playback);
    return NULL;
  }
  assert(err == paNoError);

  PaStreamParameters outputParameters;
  memset(&outputParameters, 0, sizeof(outputParameters));
  outputParameters.device                    = Pa_GetDefaultOutputDevice();
  outputParameters.channelCount              = channel_num;
  outputParameters.sampleFormat              = __get_sample_format_by_bitwidth(bit_width);
  outputParameters.suggestedLatency          = Pa_GetDeviceInfo(outputParameters.device)->defaultLowInputLatency;
  outputParameters.hostApiSpecificStreamInfo = NULL;

  err = Pa_OpenStream(&audio_playback->pa_stream,
                      NULL,
                      &outputParameters,
                      sample_rate,
                      paFramesPerBufferUnspecified,
                      paNoFlag,
                      __portaudio_playback,
                      audio_playback);
  if (err != paNoError) {
    LOGE(TAG, "Failed to open PortAudio default stream. err=%s", Pa_GetErrorText(err));
    free(audio_playback);
    return NULL;
  }

  return (audio_playback_handle_t)audio_playback;
}

int audio_playback_wrapper_start(audio_playback_handle_t handle)
{
  audio_playback_t *audio_playback = (audio_playback_t *)handle;
  if (NULL == audio_playback) {
    LOGW(TAG, "handle invalid. nullptr");
    return -1;
  }

  PaError err = Pa_StartStream(audio_playback->pa_stream);
  if (err != paNoError) {
    LOGE(TAG, "Failed to start PortAudio stream. err=%s", Pa_GetErrorText(err));
    return -1;
  }

  return 0;
}

int audio_playback_wrapper_stop(audio_playback_handle_t handle)
{
  audio_playback_t *audio_playback = (audio_playback_t *)handle;
  if (NULL == audio_playback) {
    LOGW(TAG, "handle invalid. nullptr");
    return -1;
  }

  audio_playback->pa_stopping = true;
  PaError err = Pa_StopStream(audio_playback->pa_stream);
  if (err != paNoError) {
    LOGE(TAG, "Failed to stop PortAudio stream. err=%s", Pa_GetErrorText(err));
    return -1;
  }

  return 0;
}

int audio_playback_wrapper_set_volume(audio_playback_handle_t handle, uint32_t volume_percent)
{
  audio_playback_t *audio_playback = (audio_playback_t *)handle;
  if (NULL == audio_playback) {
    LOGW(TAG, "handle invalid. nullptr");
    return -1;
  }

  audio_playback->volume_percent = volume_percent;
  LOGT(TAG, "set audio playback volume percent to %u%%", volume_percent);
  return 0;
}

int audio_playback_wrapper_get_volume(audio_playback_handle_t handle)
{
  audio_playback_t *audio_playback = (audio_playback_t *)handle;
  if (NULL == audio_playback) {
    LOGW(TAG, "handle invalid. nullptr");
    return -1;
  }

  return (int)audio_playback->volume_percent;
}

void audio_playback_wrapper_feed(audio_playback_handle_t handle, uint8_t *data, uint32_t len)
{
  audio_playback_t *audio_playback = (audio_playback_t *)handle;
  if (NULL == audio_playback) {
    LOGW(TAG, "handle invalid. nullptr");
    return;
  }

  int err = amf_ringbuf_write(audio_playback->playback_ringbuf, (char *)data, len);
  if (err < 0) {
    LOGW(TAG, "audio playback overrun. please check your audio feed rate");
  }
}

void audio_playback_wrapper_destroy(audio_playback_handle_t handle)
{
  audio_playback_t *audio_playback = (audio_playback_t *)handle;
  if (NULL == audio_playback) {
    LOGW(TAG, "handle invalid. nullptr");
    return;
  }

  Pa_CloseStream(audio_playback->pa_stream);
  Pa_Terminate();
  amf_ringbuf_destroy(audio_playback->playback_ringbuf);
  free(audio_playback);
}