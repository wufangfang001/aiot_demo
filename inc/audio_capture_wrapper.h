/*************************************************************
 *
 * This is a part of the Agora Media Framework Library.
 * Copyright (C) 2021 Agora IO
 * All rights reserved.
 *
 *************************************************************/
#ifndef AUDIO_CAPTURE_WRAPPER_H_
#define AUDIO_CAPTURE_WRAPPER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef void* audio_wrapper_handle_t;
typedef void (*cb_audio_data_t)(char *data, uint32_t len, void *user_data);

/**
 * Create audio wrapper handle.
 *
 * @param[in]  cb_data   audio source data callback.
 * @param[in]  user_data   audio source data user_data.
 *
 * @return  NULL: fail, otherwise: audioin handle.
 */
audio_wrapper_handle_t audio_capture_wrapper_create(cb_audio_data_t cb_data,
                                                    uint32_t sample_rate,
                                                    uint32_t bit_width,
                                                    uint32_t channel_num,
                                                    void *user_data);

/**
 * Destroy audio wrapper handle.
 *
 * @param[in]  hndl audio wrapper handle.
 *
 * @return  none.
 */
void audio_capture_wrapper_destroy(audio_wrapper_handle_t hndl);

/**
 * Start audio wrapper to capture audio.
 *
 * @param[in] hndl audio wrapper handle.
 *
 * @return  0: success, otherwise: fail.
 */
int audio_capture_wrapper_start(audio_wrapper_handle_t hndl);

/**
 * Stop audio wrapper to capture audio.
 *
 * @param[in] hndl audio wrapper handle.
 *
 * @return  0: success, otherwise: fail.
 */
int audio_capture_wrapper_stop(audio_wrapper_handle_t hndl);

/**
 * Set audio capture volume
 * 
 * @param hndl audio wrapper handle.
 * @param volume_percent audio volume percent[0, 100], 0 for silence, default 100.
 * @return 0: success, otherwise: fail.
 */
int audio_capture_wrapper_set_volume(audio_wrapper_handle_t hndl, uint32_t volume_percent);

/**
 * Get audio capture volume
 * 
 * @param hndl audio wrapper handle.
 * @return -1: fail, otherwise: volume [0, 100]
 */
int audio_capture_wrapper_get_volume(audio_wrapper_handle_t hndl);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif  /* AUDIO_CAPTURE_WRAPPER_H_ */