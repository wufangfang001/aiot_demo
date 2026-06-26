/*************************************************************
 *
 * This is a part of the Agora Media Framework Library.
 * Copyright (C) 2021 Agora IO
 * All rights reserved.
 *
 *************************************************************/
#ifndef AMF_LOG_H_
#define AMF_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#define LOGS(TAG, fmt, ...)          fprintf(stdout, "" fmt "\n", ##__VA_ARGS__)
#define LOGT(TAG, fmt, ...)          fprintf(stdout, "\033[0m\033[42;33mI\033[0m/ %s" fmt " at %s:%u\n", TAG, ##__VA_ARGS__, __PRETTY_FUNCTION__, __LINE__)
#define LOGI(TAG, fmt, ...)          fprintf(stdout, "\033[0m\033[42;33mI\033[0m/ %s" fmt " at %s:%u\n", TAG, ##__VA_ARGS__, __PRETTY_FUNCTION__, __LINE__)
#define LOGD(TAG, fmt, ...)          //fprintf(stdout, "\033[0m\033[47;33mD\033[0m/ %s" fmt " at %s:%u\n", TAG, ##__VA_ARGS__, __PRETTY_FUNCTION__, __LINE__)
#define LOGE(TAG, fmt, ...)          fprintf(stdout, "\033[0m\033[41;33mE\033[0m/ %s" fmt " at %s:%u\n", TAG, ##__VA_ARGS__, __PRETTY_FUNCTION__, __LINE__)
#define LOGW(TAG, fmt, ...)          fprintf(stdout, "\033[0m\033[41;33mW\033[0m/ %s" fmt " at %s:%u\n", TAG, ##__VA_ARGS__, __PRETTY_FUNCTION__, __LINE__)

#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif  /* AMF_LOG_H_ */