#ifndef FDCAN_H
#define FDCAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define FDCAN1_USE_INTERNAL_LOOPBACK 0U

extern FDCAN_HandleTypeDef hfdcan1;

void FDCAN1_Init(void);

#ifdef __cplusplus
}
#endif

#endif
