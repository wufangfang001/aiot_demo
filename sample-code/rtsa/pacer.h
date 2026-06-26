/*************************************************************
 *
 * This is a part of the Agora Media Framework Library.
 * Copyright (C) 2021 Agora IO
 * All rights reserved.
 *
 *************************************************************/
#ifndef PACER_H_
#define PACER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
  uint32_t interval_ms;
  int64_t predict_next_time_ms;
} pacer_t;

void *pacer_create(uint32_t interval_ms);
void pacer_destroy(void *pacer);
void wait_for_next_pace(void *pacer);

#ifdef __cplusplus
}
#endif
#endif /* PACER_H_ */