/*************************************************************
 *
 * This is a part of the Agora Media Framework Library.
 * Copyright (C) 2021 Agora IO
 * All rights reserved.
 *
 *************************************************************/
#include "audio_capture_wrapper.h"
#include "portaudio.h"
#include "amf_log.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#define TAG     "[capture]"

typedef struct {
  PaStream*       pa_stream;
  cb_audio_data_t cb_data;
  void*           user_data;
  bool            pa_stopping;
  uint32_t        cur_pcm_buffer_len;
  uint8_t         *raw_pcm_20ms;
  uint32_t        pcm_20ms_len;
  uint32_t        volume_percent;
  uint32_t        bit_width;
} audio_wrapper_t;

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

#define __MIN(a, b)  ((a) > (b) ? (b) : (a))
static int __portaudio_callback(const void* inputBuffer,
                                void* outputBuffer,
                                unsigned long numSamples,
                                const PaStreamCallbackTimeInfo* timeInfo,
                                PaStreamCallbackFlags statusFlags,
                                void* userData)
{
  audio_wrapper_t *audio_wrapper = (audio_wrapper_t *)userData;
  int cur_remain_bytes = numSamples << 1;
  uint8_t *cur_src_read_ptr = (uint8_t *)inputBuffer;
  uint32_t feed_len;

  while (cur_remain_bytes > 0) {
    if (audio_wrapper->cur_pcm_buffer_len == audio_wrapper->pcm_20ms_len) {
      __volume_adjust(audio_wrapper->raw_pcm_20ms,
                      audio_wrapper->pcm_20ms_len,
                      audio_wrapper->bit_width,
                      (int32_t)audio_wrapper->volume_percent);
      audio_wrapper->cb_data(audio_wrapper->raw_pcm_20ms,
                             audio_wrapper->pcm_20ms_len,
                             audio_wrapper->user_data);
      audio_wrapper->cur_pcm_buffer_len = 0;
    }

    feed_len = __MIN(cur_remain_bytes, audio_wrapper->pcm_20ms_len - audio_wrapper->cur_pcm_buffer_len);
    memcpy(&audio_wrapper->raw_pcm_20ms[audio_wrapper->cur_pcm_buffer_len],
           cur_src_read_ptr,
           feed_len);

    audio_wrapper->cur_pcm_buffer_len += feed_len;
    cur_src_read_ptr += feed_len;
    cur_remain_bytes -= feed_len;
  }

  LOGD(TAG, "portaudio data. buf=%p, len=%lu", inputBuffer, numSamples << 1);
  return audio_wrapper->pa_stopping ? paAbort : paContinue;
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

static PaError __try_open_capture_device(audio_wrapper_t *audio_wrapper,
                                         PaDeviceIndex device,
                                         uint32_t sample_rate,
                                         uint32_t bit_width,
                                         uint32_t channel_num)
{
  if (device == paNoDevice) {
    return paDeviceUnavailable;
  }

  const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(device);
  if (deviceInfo == NULL || deviceInfo->maxInputChannels < (int)channel_num) {
    return paDeviceUnavailable;
  }

  PaStreamParameters inputParameters;
  memset(&inputParameters, 0, sizeof(inputParameters));
  inputParameters.device = device;
  inputParameters.channelCount = channel_num;
  inputParameters.sampleFormat = __get_sample_format_by_bitwidth(bit_width);
  inputParameters.suggestedLatency = deviceInfo->defaultLowInputLatency;
  inputParameters.hostApiSpecificStreamInfo = NULL;

  if (Pa_IsFormatSupported(&inputParameters, NULL, sample_rate) != paFormatIsSupported) {
    LOGD(TAG, "device %d '%s' does not support format %uHz %ubit %uch",
         device, deviceInfo->name, sample_rate, bit_width, channel_num);
    return paSampleFormatNotSupported;
  }

  return Pa_OpenStream(&audio_wrapper->pa_stream,
                       &inputParameters,
                       NULL,
                       sample_rate,
                       paFramesPerBufferUnspecified,
                       paNoFlag,
                       __portaudio_callback,
                       audio_wrapper);
}

static PaError __open_capture_stream(audio_wrapper_t *audio_wrapper,
                                     uint32_t sample_rate,
                                     uint32_t bit_width,
                                     uint32_t channel_num)
{
  int device_count = Pa_GetDeviceCount();
  if (device_count < 0) {
    LOGE(TAG, "Failed to query PortAudio devices. err=%d", device_count);
    return (PaError)device_count;
  }

  PaDeviceIndex defaultDevice = Pa_GetDefaultInputDevice();
  PaError err = __try_open_capture_device(audio_wrapper,
                                          defaultDevice,
                                          sample_rate,
                                          bit_width,
                                          channel_num);
  if (err == paNoError) {
    LOGT(TAG, "Opened PortAudio input using default device %d", defaultDevice);
    return paNoError;
  }

  for (PaDeviceIndex device = 0; device < device_count; ++device) {
    if (device == defaultDevice) {
      continue;
    }

    err = __try_open_capture_device(audio_wrapper,
                                    device,
                                    sample_rate,
                                    bit_width,
                                    channel_num);
    if (err == paNoError) {
      const PaDeviceInfo *info = Pa_GetDeviceInfo(device);
      LOGT(TAG, "Opened PortAudio input using fallback device %d '%s'",
           device, info ? info->name : "unknown");
      return paNoError;
    }
  }

  LOGE(TAG, "No compatible PortAudio input device found.");
  return paDeviceUnavailable;
}

audio_wrapper_handle_t audio_capture_wrapper_create(cb_audio_data_t cb_data,
                                                    uint32_t sample_rate,
                                                    uint32_t bit_width,
                                                    uint32_t channel_num,
                                                    void *user_data)
{
  audio_wrapper_t* audio_wrapper = (audio_wrapper_t *)malloc(sizeof(audio_wrapper_t));
  assert(audio_wrapper != NULL);
  memset(audio_wrapper, 0, sizeof(audio_wrapper_t));

  audio_wrapper->bit_width      = bit_width;
  audio_wrapper->cb_data        = cb_data;
  audio_wrapper->user_data      = user_data;
  audio_wrapper->volume_percent = 100;

  PaError err;
  err = Pa_Initialize();
  if (err != paNoError) {
    LOGE(TAG, "Failed to initialize PortAudio. err=%s", Pa_GetErrorText(err));
    free(audio_wrapper);
    return NULL;
  }

  audio_wrapper->pcm_20ms_len = channel_num * sample_rate * bit_width / 8 / 50;
  audio_wrapper->raw_pcm_20ms = (uint8_t *)malloc(audio_wrapper->pcm_20ms_len);
  assert(audio_wrapper->raw_pcm_20ms);

  LOGT(TAG, "sample_rate=%u, bit_width=%u, channel_num=%u, 20ms_len=%u",
       sample_rate, bit_width, channel_num, audio_wrapper->pcm_20ms_len);

  err = __open_capture_stream(audio_wrapper, sample_rate, bit_width, channel_num);
  if (err != paNoError) {
    LOGE(TAG, "Failed to open PortAudio stream. err=%s", Pa_GetErrorText(err));
    Pa_Terminate();
    free(audio_wrapper->raw_pcm_20ms);
    free(audio_wrapper);
    return NULL;
  }

  return audio_wrapper;
}

void audio_capture_wrapper_destroy(audio_wrapper_handle_t hndl)
{
  audio_wrapper_t *audio_wrapper = (audio_wrapper_t *)hndl;
  if (NULL == audio_wrapper) {
    LOGW(TAG, "handle invalid. nullptr");
    return;
  }

  Pa_CloseStream(audio_wrapper->pa_stream);
  Pa_Terminate();

  free(audio_wrapper->raw_pcm_20ms);
  free(audio_wrapper);
}

int audio_capture_wrapper_start(audio_wrapper_handle_t hndl)
{
  audio_wrapper_t *audio_wrapper = (audio_wrapper_t *)hndl;
  if (NULL == audio_wrapper) {
    LOGW(TAG, "handle invalid. nullptr");
    return -1;
  }

  PaError err = Pa_StartStream(audio_wrapper->pa_stream);
  if (err != paNoError) {
    LOGE(TAG, "Failed to start PortAudio stream. err=%s", Pa_GetErrorText(err));
    return -1;
  }

  return 0;
}

int audio_capture_wrapper_stop(audio_wrapper_handle_t hndl)
{
  audio_wrapper_t *audio_wrapper = (audio_wrapper_t *)hndl;
  if (NULL == audio_wrapper) {
    LOGW(TAG, "handle invalid. nullptr");
    return -1;
  }

  audio_wrapper->pa_stopping = true;
  PaError err = Pa_StopStream(audio_wrapper->pa_stream);
  if (err != paNoError) {
    LOGE(TAG, "Failed to stop PortAudio stream. err=%s", Pa_GetErrorText(err));
    return -1;
  }

  return 0;
}

int audio_capture_wrapper_set_volume(audio_wrapper_handle_t hndl, uint32_t volume_percent)
{
  audio_wrapper_t *audio_wrapper = (audio_wrapper_t *)hndl;
  if (NULL == audio_wrapper) {
    LOGW(TAG, "handle invalid. nullptr");
    return -1;
  }

  if (volume_percent > 100) {
    LOGW(TAG, "audio volume percent must be [0, 100]. percent %u is invalid", volume_percent);
    return -1;
  }

  audio_wrapper->volume_percent = volume_percent;
  LOGT(TAG, "set audio volume percent to %u%%", volume_percent);
  return 0;
}

int audio_capture_wrapper_get_volume(audio_wrapper_handle_t hndl)
{
  audio_wrapper_t *audio_wrapper = (audio_wrapper_t *)hndl;
  if (NULL == audio_wrapper) {
    LOGW(TAG, "handle invalid. nullptr");
    return -1;
  }

  return (int)audio_wrapper->volume_percent;
}
