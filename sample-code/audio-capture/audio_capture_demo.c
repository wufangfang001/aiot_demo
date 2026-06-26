/*************************************************************
 *
 * This is a part of the Agora Media Framework Library.
 * Copyright (C) 2021 Agora IO
 * All rights reserved.
 *
 *************************************************************/
#include "audio_capture_wrapper.h"
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#define CAPTURE_SAMPLE_RATE   (16000)
#define CAPTURE_BIT_WIDTH     (16)
#define CAPTURE_CHANNEL_NUM   (1)

static void process_capture_data(char *data,
                                 uint32_t len,
                                 void *user_data)
{
  FILE *fd = (FILE *)user_data;
  printf("audio source callback. pcm length is %d\n", len);
  fwrite(data, len, 1, fd);
}

int main()
{
  static FILE *fd = NULL;
  audio_wrapper_handle_t audio_handle;

  /* step1. 打开文件，用于存储采集PCM音频 */
  fd = fopen("save_audio.pcm", "wb");
  assert(fd);

  /* step2. 创建音频采集 */
  audio_handle = audio_capture_wrapper_create(process_capture_data,
                                              CAPTURE_SAMPLE_RATE,
                                              CAPTURE_BIT_WIDTH,
                                              CAPTURE_CHANNEL_NUM,
                                              fd);
  assert(audio_handle);

  audio_capture_wrapper_set_volume(audio_handle, 50);

  /* step3. 开始音频采集 */
  audio_capture_wrapper_start(audio_handle);

  /* step4. 音频采集5秒后结束 */  
  usleep(1000 * 1000 * 5);

  /* step5. 结束音频采集 */
  audio_capture_wrapper_stop(audio_handle);

  /* step6. 释放音频采集占用的资源 */
  audio_capture_wrapper_destroy(audio_handle);

  /* step7. 关闭文件句柄 */
  fclose(fd);

  return 0;
}