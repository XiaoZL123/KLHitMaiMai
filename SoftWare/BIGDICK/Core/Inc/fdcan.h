#ifndef FDCAN_H
#define FDCAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define FDCAN1_USE_INTERNAL_LOOPBACK 0U

/* fdcan.c：初始化 FDCAN1、配置标准/扩展过滤器和 PD0/PD1，并使用 1 Mbit/s。 */
extern FDCAN_HandleTypeDef hfdcan1;

/* 初始化并启动 FDCAN1，配置 GM6020 标准帧与 X42S 扩展帧过滤器。 */
void FDCAN1_Init(void);
/* 唯一轮询 RX FIFO0，并按帧类型分发到 GM6020 或 X42S。 */
void FDCAN1_Process(void);

#ifdef __cplusplus
}
#endif

#endif
