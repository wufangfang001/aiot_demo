/*************************************************************
 *
 * This is a part of the Agora Media Framework Library.
 * Copyright (C) 2021 Agora IO
 * All rights reserved.
 *
 *************************************************************/
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pacer.h"

void *pacer_create(uint32_t interval_ms)
{
  pacer_t *pacer = (pacer_t *)malloc(sizeof(pacer_t));

  pacer->interval_ms = interval_ms;
  pacer->predict_next_time_ms = 0;

  return pacer;
}

void pacer_destroy(void *pacer)
{
  if (pacer) {
    free(pacer);
  }
}

void wait_for_next_pace(void *pacer)
{
  pacer_t *pc = pacer;
  int64_t sleep_ms = 0;
  int64_t cur_time_ms = 0;

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  cur_time_ms = (uint64_t)ts.tv_sec * (uint64_t)1000 + ts.tv_nsec / 1000000;

  if (pc->predict_next_time_ms == 0) {
    pc->predict_next_time_ms = cur_time_ms + pc->interval_ms;
  }

  sleep_ms = pc->predict_next_time_ms - cur_time_ms;

  if (sleep_ms > 0) {
    usleep(sleep_ms * 1000);
  }

  pc->predict_next_time_ms += pc->interval_ms;
}