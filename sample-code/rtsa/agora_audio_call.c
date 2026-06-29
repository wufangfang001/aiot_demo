/*************************************************************
 *
 * This is a part of the Agora Media Framework Library.
 * Copyright (C) 2021 Agora IO
 * All rights reserved.
 *
 *************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>

#include "agora_rtc_api.h"
#include "pacer.h"
#include "audio_capture_wrapper.h"
#include "audio_playback_wrapper.h"
#include "amf_ringbuf.h"
#include "fvad.h"

// The Agora APP ID for testing. Please replace it with your OWN APP ID
#define AGORA_APP_ID_FOR_TEST         "0a324a*******cc28"
#define DEFAULT_CHANNEL_NAME          "aiot_demo"
#define DEFAULT_TOKEN                 NULL
#define DEFAULT_USER_ID               1 // sdk will automatically assign a random user id
#define DEFAULT_SDK_LOG_PATH          "io.agora.rtc_sdk"
#define DEFAULT_AREA_CODE             AREA_CODE_GLOB
#define DEFAULT_AUDIO_CODEC_TYPE      AUDIO_CODEC_TYPE_OPUS

#define CONFIG_SEND_PCM_DATA
#define CONFIG_PCM_SAMPLE_RATE         (48000)
#define CONFIG_PCM_CHANNEL_NUM         (1)
#define CONFIG_AUDIO_FRAME_DURATION_MS (20)
#define CONFIG_PCM_FRAME_LEN           (CONFIG_PCM_SAMPLE_RATE * CONFIG_AUDIO_FRAME_DURATION_MS * CONFIG_PCM_CHANNEL_NUM * 2 / 1000) // 2 bytes per sample

/* Enable VAD test functionality in audio send path */
#define CONFIG_ENABLE_FVAD_TEST         0
/* Enable saving PCM to mic.pcm / speak.pcm */
#define CONFIG_ENABLE_PCM_FILE_SAVE     0

#define DEFAULT_BANDWIDTH_ESTIMATE_MIN_BITRATE   (100000)
#define DEFAULT_BANDWIDTH_ESTIMATE_MAX_BITRATE   (1000000)
#define DEFAULT_BANDWIDTH_ESTIMATE_START_BITRATE (500000)

#ifdef CONFIG_LICENSE
static const char *certificate_for_test = "license";
#endif

typedef struct {
  connection_id_t         conn_id;
  connection_info_t       conn_inf;
  audio_playback_handle_t playback_handle;
  audio_wrapper_handle_t  capture_handle;
  ringbuf_handle_t        capture_ringbuf_handle;
  ringbuf_handle_t        playback_ringbuf_handle;
  bool                    b_exit;
  bool                    b_join_channel_success;
  Fvad*                   vad;
  int                     vad_state;
  FILE*                   mic_file;
  FILE*                   speak_file;
} app_t;

static app_t g_app = {0};

static void __process_capture_data(char *data, unsigned int len, void *user_data)
{
  amf_ringbuf_write(g_app.capture_ringbuf_handle, data, len);
}

static void __play_audio_data_push(const void* data, int len)
{
  amf_ringbuf_write(g_app.playback_ringbuf_handle, (const char *)data, len);
}

static int __play_audio_data_pull(void* data, int len)
{
  if (0 > amf_ringbuf_read((char *)data, len, g_app.playback_ringbuf_handle)) {
    printf("warning: not have enough play data.\n");
    return -1;
  }
  //fwrite(data, 1, len, g_app.speak_file);
  return 0;
}

static void __signal_handler(int sig)
{
  switch (sig) {
  case SIGINT:
    g_app.b_exit = true;
    break;
  default:
    printf("no handler, sig=%d\n", sig);
  }
}

static int __send_audio_frame()
{
  audio_frame_info_t info = { 0 };
  info.data_type = AUDIO_DATA_TYPE_PCM;
  char capture_audio[CONFIG_PCM_FRAME_LEN] = {0};

  if (0 > amf_ringbuf_read(capture_audio, sizeof(capture_audio), g_app.capture_ringbuf_handle)) {
    printf("warning: not have enough send data.\n");
    return -1;
  }
#ifdef CONFIG_ENABLE_FVAD_TEST
  //fwrite(capture_audio, 1, sizeof(capture_audio), g_app.mic_file);

  int vad_state = fvad_process(g_app.vad, (int16_t *)capture_audio, sizeof(capture_audio) / 2);
  if (vad_state != g_app.vad_state) {
    g_app.vad_state = vad_state;
    printf("VAD state switch to %s ...\n", vad_state == 1 ? "active" : "silence");
  }
  if (vad_state != 1) {
    return 0;
  }
#endif
  int rval = agora_rtc_send_audio_data(g_app.conn_id, capture_audio, sizeof(capture_audio), &info);
  if (rval < 0) {
    printf("Failed to send audio data, reason: %s\n", agora_rtc_err_2_str(rval));
    return -1;
  }

  return 0;
}

static void *audio_send_thread(void *args)
{
  int audio_send_interval_ms = CONFIG_AUDIO_FRAME_DURATION_MS;
  uint8_t playback[CONFIG_PCM_FRAME_LEN];
  void *pacer = pacer_create(audio_send_interval_ms);
  audio_playback_wrapper_start(g_app.playback_handle);
  audio_capture_wrapper_start(g_app.capture_handle);

  audio_capture_wrapper_set_volume(g_app.capture_handle, 50);
  audio_playback_wrapper_set_volume(g_app.playback_handle, 50);

  int capture_volume = audio_capture_wrapper_get_volume(g_app.capture_handle);
  int playback_volume = audio_playback_wrapper_get_volume(g_app.playback_handle);

  printf("[volume]. capture=%d, playback=%d\n", capture_volume, playback_volume);

  while (g_app.b_join_channel_success && !g_app.b_exit) {
    __send_audio_frame();
    if (0 == __play_audio_data_pull(playback, sizeof(playback))) {
      audio_playback_wrapper_feed(g_app.playback_handle, playback, sizeof(playback));
    }
    /* sleep and wait until time is up for next send */
    wait_for_next_pace(pacer);
  }

  audio_capture_wrapper_stop(g_app.capture_handle);
  audio_playback_wrapper_stop(g_app.playback_handle);
  pacer_destroy(pacer);
  return NULL;
}

static void __on_join_channel_success(connection_id_t conn_id, uint32_t uid, int elapsed)
{
  g_app.b_join_channel_success = true;
  agora_rtc_get_connection_info(conn_id, &g_app.conn_inf);

  printf("[conn-%u] Join the channel %s successfully, uid %u elapsed %d ms\n",
         conn_id, g_app.conn_inf.channel_name, uid, elapsed);
}

static void __on_connection_lost(connection_id_t conn_id)
{
  g_app.b_join_channel_success = false;
  printf("[conn-%u] Lost connection from the channel\n", conn_id);
}

static void __on_rejoin_channel_success(connection_id_t conn_id, uint32_t uid, int elapsed_ms)
{
  g_app.b_join_channel_success = true;
  printf("[conn-%u] Rejoin the channel successfully, uid %u elapsed %d ms\n", conn_id, uid, elapsed_ms);
}

static void __on_user_joined(connection_id_t conn_id, uint32_t uid, int elapsed_ms)
{
  printf("[conn-%u] Remote user \"%u\" has joined the channel, elapsed %d ms\n", uid, conn_id, elapsed_ms);
}

static void __on_user_offline(connection_id_t conn_id, uint32_t uid, int reason)
{
  printf("[conn-%u] Remote user \"%u\" has left the channel, reason %d\n", conn_id, uid, reason);
}

static void __on_user_mute_audio(connection_id_t conn_id, uint32_t uid, bool muted)
{
  printf("[conn-%u] audio: uid=%u muted=%d\n", conn_id, uid, muted);
}

static void __on_user_mute_video(connection_id_t conn_id, uint32_t uid, bool muted)
{
  printf("[conn-%u] video: uid=%u muted=%d\n", conn_id, uid, muted);
}

static void __on_error(connection_id_t conn_id, int code, const char *msg)
{
  if (code == ERR_VIDEO_SEND_OVER_BANDWIDTH_LIMIT) {
    printf("Not enough uplink bandwdith. Error msg \"%s\"\n", msg);
    return;
  }

  if (code == ERR_INVALID_APP_ID) {
    printf("Invalid App ID. Please double check. Error msg \"%s\"\n", msg);
  } else if (code == ERR_INVALID_CHANNEL_NAME) {
    printf("Invalid channel name. Please double check. Error msg \"%s\"\n", msg);
  } else if (code == ERR_INVALID_TOKEN || code == ERR_TOKEN_EXPIRED) {
    printf("Invalid token. Please double check. Error msg \"%s\"\n", msg);
  } else if (code == ERR_DYNAMIC_TOKEN_BUT_USE_STATIC_KEY) {
    printf("Dynamic token is enabled but is not provided. Error msg \"%s\"\n", msg);
  } else {
    printf("Error %d is captured. Error msg \"%s\"\n", code, msg);
  }

  g_app.b_exit = true;
}

static uint64_t os_get_time_now_us ()
{
	struct timeval tv;
	if (gettimeofday (&tv, NULL) < 0)
		return 0;

	return (uint64_t)(((uint64_t)tv.tv_sec * (uint64_t)1000000) + tv.tv_usec);
}

static void __on_audio_data(connection_id_t conn_id, const uint32_t uid, uint16_t sent_ts, const void *data, size_t len,
                            const audio_frame_info_t *info_ptr)
{
#if 0
  static int64_t last_time_ms = 0;
  int64_t cur_time_ms = 0;
  int64_t diff_time_ms = 0;

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  cur_time_ms = (uint64_t)ts.tv_sec * (uint64_t)1000 + ts.tv_nsec / 1000000;
  diff_time_ms = cur_time_ms - last_time_ms;
  if (diff_time_ms > CONFIG_AUDIO_FRAME_DURATION_MS + 10) {
    time_t now_sec = os_get_time_now_us() / 1000 / 1000;
    struct tm time_tm;
    char time_str[20] = { 0 };
    strftime(time_str, sizeof(time_str), "%F %T", localtime_r(&now_sec, &time_tm));
    printf("[%s] diff_time_ms = %ld\n", time_str, diff_time_ms);
  }
  last_time_ms = cur_time_ms;
#endif
  __play_audio_data_push(data, (int)len);
}

static void __on_mixed_audio_data(connection_id_t conn_id, const void *data, size_t len,
                                  const audio_frame_info_t *info_ptr)
{

}

#ifndef CONFIG_AUDIO_ONLY
static void __on_video_data(connection_id_t conn_id, const uint32_t uid, uint16_t sent_ts, const void *data, size_t len,
                            const video_frame_info_t *info_ptr)
{

}

static void __on_target_bitrate_changed(connection_id_t conn_id, uint32_t target_bps)
{
  printf("[conn-%u] Bandwidth change detected. Please adjust encoder bitrate to %u kbps\n", conn_id, target_bps / 1000);
}

static void __on_key_frame_gen_req(connection_id_t conn_id, uint32_t uid, video_stream_type_e stream_type)
{
  printf("[conn-%u] Frame loss detected. Please notify the encoder to generate key frame immediately\n", conn_id);
}
#endif

static void app_init_event_handler(agora_rtc_event_handler_t *event_handler)
{
  event_handler->on_join_channel_success   = __on_join_channel_success;
  event_handler->on_connection_lost        = __on_connection_lost;
  event_handler->on_rejoin_channel_success = __on_rejoin_channel_success;
  event_handler->on_user_joined            = __on_user_joined;
  event_handler->on_user_offline           = __on_user_offline;
  event_handler->on_user_mute_audio        = __on_user_mute_audio;
  event_handler->on_user_mute_video        = __on_user_mute_video;
#ifndef CONFIG_AUDIO_ONLY
  event_handler->on_target_bitrate_changed = __on_target_bitrate_changed;
  event_handler->on_key_frame_gen_req      = __on_key_frame_gen_req;
  event_handler->on_video_data             = __on_video_data;
#endif
  event_handler->on_error                  = __on_error;

#ifdef CONFIG_ENABLE_AUDIO_MIXING
  event_handler->on_mixed_audio_data       = __on_mixed_audio_data;
#else
  event_handler->on_audio_data             = __on_audio_data;
#endif
}

static void __vad_instance_create(void)
{
    g_app.vad = fvad_new();
    fvad_set_mode(g_app.vad, 0);
    fvad_set_sample_rate(g_app.vad, 16000);
}

static void __vad_instance_destroy(void)
{
    fvad_free(g_app.vad);
    g_app.vad = NULL;
}

static int __open_save_file(void)
{
#ifdef CONFIG_ENABLE_PCM_FILE_SAVE
  if ((g_app.mic_file = fopen("mic.pcm", "w")) == NULL) {
    printf("Failed to create file mic.pcm\n");
    return -1;
  }
  if ((g_app.speak_file = fopen("speak.pcm", "w")) == NULL) {
    printf("Failed to create file speak.pcm\n");
    fclose(g_app.mic_file);
    return -1;
  }
#endif
  return 0;
}

static void __close_save_file(void)
{
#ifdef CONFIG_ENABLE_PCM_FILE_SAVE
  if (g_app.mic_file) {
    fclose(g_app.mic_file);
  }
  if (g_app.speak_file) {
    fclose(g_app.speak_file);
  }
#endif
}

int main(int argc, char **argv)
{
  int rval;

  /* step1. 设置signal处理 */
  signal(SIGINT, __signal_handler);
  printf("Welcome to RTSA SDK v%s\n", agora_rtc_get_version());

  /* step2. 创建音频采集、播放ringbuf */
  g_app.capture_ringbuf_handle = amf_ringbuf_create(32 * 1024);
  g_app.playback_ringbuf_handle = amf_ringbuf_create(32 * 1024);

  /* step3. 创建音频播放 */
  g_app.playback_handle = audio_playback_wrapper_create(CONFIG_PCM_SAMPLE_RATE, 16, 1);
  if (NULL == g_app.playback_handle) {
    printf("Failed to create audio playback\n");
    goto L_ERROR_AUDIO_PLAYBACK_CREATE_FAILED;
  }

  /* step4. 创建音频采集 */
  g_app.capture_handle = audio_capture_wrapper_create(__process_capture_data, CONFIG_PCM_SAMPLE_RATE, 16, 1, NULL);
  if (NULL == g_app.capture_handle) {
    printf("Failed to create audio capture\n");
    goto L_ERROR_AUDIO_CAPTURE_CREATE_FAILED;
  }

  /* step5. license校验 */
#ifdef CONFIG_LICENSE
  rval = agora_rtc_license_verify(certificate_for_test, strlen(certificate_for_test), NULL, 0);
  if (rval < 0) {
    printf("Failed to verify license, reason: %s\n", agora_rtc_err_2_str(rval));
    return -1;
  }
  printf("Verify license successfully\n");
#endif

  /* step6. API: init agora rtc sdk */
  agora_rtc_event_handler_t event_handler = { 0 };
  app_init_event_handler(&event_handler);
  rtc_service_option_t service_opt = { 0 };
  service_opt.area_code = DEFAULT_AREA_CODE;
  service_opt.log_cfg.log_level = RTC_LOG_WARNING;
  service_opt.log_cfg.log_path = DEFAULT_SDK_LOG_PATH;
  rval = agora_rtc_init(AGORA_APP_ID_FOR_TEST, &event_handler, &service_opt);
  if (rval < 0) {
    printf("Failed to initialize Agora sdk, reason: %s\n", agora_rtc_err_2_str(rval));
    goto L_ERROR_RTC_INIT_FAILED;
  }

#ifdef CONFIG_ENABLE_FVAD_TEST
  __vad_instance_create();
#endif
  __open_save_file();

#ifndef CONFIG_AUDIO_ONLY
  agora_rtc_set_bwe_param(CONNECTION_ID_ALL, DEFAULT_BANDWIDTH_ESTIMATE_MIN_BITRATE,
                          DEFAULT_BANDWIDTH_ESTIMATE_MAX_BITRATE, DEFAULT_BANDWIDTH_ESTIMATE_START_BITRATE);
#endif

  /* step7. API: Create connection */
  rval = agora_rtc_create_connection(&g_app.conn_id);
  if (rval < 0) {
    printf("Failed to create connection, reason: %s\n", agora_rtc_err_2_str(rval));
    goto L_ERROR_CREATE_CONN_FAILED;
  }

  /* step8. API: join channel */
  rtc_channel_options_t channel_options = { 0 };
  channel_options.auto_subscribe_audio = true;
  channel_options.auto_subscribe_video = true;
#ifdef CONFIG_SEND_PCM_DATA
  /* If we want to send PCM data instead of encoded audio like AAC or Opus, here please enable
   * audio codec, as well as configure the PCM sample rate and number of channels
   */
  channel_options.audio_codec_opt.audio_codec_type       = DEFAULT_AUDIO_CODEC_TYPE;
  channel_options.audio_codec_opt.pcm_sample_rate        = CONFIG_PCM_SAMPLE_RATE;
  channel_options.audio_codec_opt.pcm_channel_num        = CONFIG_PCM_CHANNEL_NUM;
  channel_options.enable_audio_decode                    = true;
  channel_options.enable_audio_jitter_buffer             = true;
  channel_options.enable_audio_ai_qos                    = true;
#endif
  rval = agora_rtc_join_channel(g_app.conn_id, DEFAULT_CHANNEL_NAME, DEFAULT_USER_ID, DEFAULT_TOKEN, &channel_options);
  if (rval < 0) {
    printf("Failed to join channel \"%s\", reason: %s\n", DEFAULT_CHANNEL_NAME, agora_rtc_err_2_str(rval));
    goto L_JOIN_CHANNEL_FAILED;
  }

  /* step9. wait until we join channel successfully */
  while (!g_app.b_join_channel_success) {
    usleep(100 * 1000);
  }

  /* step10. create tasks sending video and audio frames */
  pthread_t audio_thread_id;
  rval = pthread_create(&audio_thread_id, NULL, audio_send_thread, 0);
  if (rval < 0) {
    printf("Unable to create audio push thread\n");
    goto L_CREATE_PTHREAD_FAILED;
  }

  /* step11. wait until pthread exit */
  pthread_join(audio_thread_id, NULL);

L_CREATE_PTHREAD_FAILED:
  /* step12. API: leave channel */
  agora_rtc_leave_channel(g_app.conn_id);

L_JOIN_CHANNEL_FAILED:
  /* step13. API: destroy connection */
  agora_rtc_destroy_connection(g_app.conn_id);

L_ERROR_CREATE_CONN_FAILED:
  /* step14. API: fini rtc sdk */
  agora_rtc_fini();
#ifdef CONFIG_ENABLE_FVAD_TEST
  __vad_instance_destroy();
#endif
  __close_save_file();

L_ERROR_RTC_INIT_FAILED:
  /* step15. 注销音频采集，释放资源 */
  audio_capture_wrapper_destroy(g_app.capture_handle);

L_ERROR_AUDIO_CAPTURE_CREATE_FAILED:
  /* step16. 注销音频播放，释放资源 */
  audio_playback_wrapper_destroy(g_app.playback_handle);

L_ERROR_AUDIO_PLAYBACK_CREATE_FAILED:
  /* step17. 释放ringbuf资源 */
  amf_ringbuf_destroy(g_app.capture_ringbuf_handle);
  amf_ringbuf_destroy(g_app.playback_ringbuf_handle);
  memset(&g_app, 0, sizeof(g_app));
  return 0;
}