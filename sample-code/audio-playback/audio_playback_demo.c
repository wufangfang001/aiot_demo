/*************************************************************
 *
 * This is a part of the Agora Media Framework Library.
 * Copyright (C) 2021 Agora IO
 * All rights reserved.
 *
 *************************************************************/
#include "audio_playback_wrapper.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#define CAPTURE_SAMPLE_RATE   (16000)
#define CAPTURE_BIT_WIDTH     (16)
#define CAPTURE_CHANNEL_NUM   (1)

int main()
{
  audio_playback_handle_t play_handle;
  uint8_t audio_data[640];
  FILE *fd;

  /* step1. 创建audio playback */
  play_handle = audio_playback_wrapper_create(CAPTURE_SAMPLE_RATE,
                                              CAPTURE_BIT_WIDTH,
                                              CAPTURE_CHANNEL_NUM);
  assert(play_handle);

  /* step2. 打开测试播放源pcm文件(16K,16bit,1channel) */
  if (NULL == (fd = fopen("audio.pcm", "rb"))) {
    printf("open audio file failed.\n");
    return 0;
  }

  audio_playback_wrapper_set_volume(play_handle, 50);

  /* step3. 开始audio playback */
  audio_playback_wrapper_start(play_handle);

  /* step4. 向audio playback注入测试PCM音频数据 */
  while (fread(audio_data, sizeof(audio_data), 1, fd) != 0) {
    audio_playback_wrapper_feed(play_handle, audio_data, sizeof(audio_data));
    usleep(18 * 1000);
  }

  /* step5. 结束audio playback */
  audio_playback_wrapper_stop(play_handle);

  /* step6. 释放audio playback占用的资源 */
  audio_playback_wrapper_destroy(play_handle);

  /* step7. 关闭测试音频文件句柄 */
  fclose(fd);

  return 0;
}
