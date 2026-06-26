/*************************************************************
 *
 * This is a part of the Agora Media Framework Library.
 * Copyright (C) 2021 Agora IO
 * All rights reserved.
 *
 *************************************************************/
#ifndef AUDIO_PLAYBACK_WRAPPER_H_
#define AUDIO_PLAYBACK_WRAPPER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef void* audio_playback_handle_t;

/**
 * @brief create audio playback handle
 * 
 * @return audio_playback_handle_t 
 */
audio_playback_handle_t audio_playback_wrapper_create(uint32_t sample_rate,
                                                      uint32_t bit_width,
                                                      uint32_t channel_num);

/**
 * @brief audio playback start
 * 
 * @param handle audio playback handle
 * 
 * @return  0: success, otherwise: fail.
 */
int audio_playback_wrapper_start(audio_playback_handle_t handle);

/**
 * @brief audio playback stop
 * 
 * @param handle audio playback handle
 * 
 * @return  0: success, otherwise: fail.
 */
int audio_playback_wrapper_stop(audio_playback_handle_t handle);

/**
 * @brief audio playback volume
 * 
 * @param handle audio playback handle
 * @param volume_percent audio volume percent[0, 100], 0 for silence, default 100.
 * @return 0: success, otherwise: fail.
 */
int audio_playback_wrapper_set_volume(audio_playback_handle_t handle, uint32_t volume_percent);

/**
 * @brief get audio playback volume
 * 
 * @param handle audio playback handle
 * @return -1: fail, otherwise: volume [0, 100]
 */
int audio_playback_wrapper_get_volume(audio_playback_handle_t handle);

/**
 * @brief audio playing
 * 
 * @param handle audio playback handle
 * @param data pcm data 16K 16bit 1channel
 * @param len pcm data length
 */
void audio_playback_wrapper_feed(audio_playback_handle_t handle, uint8_t *data, uint32_t len);

/**
 * @brief destroy audio playback resource
 * 
 * @param handle 
 */
void audio_playback_wrapper_destroy(audio_playback_handle_t handle);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif  /* AUDIO_PLAYBACK_WRAPPER_H_ */