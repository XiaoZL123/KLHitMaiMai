#ifndef OPS_H
#define OPS_H

#include "main.h"

/*
 * ops.c：解析USART2 DMA接收的OPS帧，提供数据、处理周期和有效期查询。
 * OPS_Init：初始化解析器和TIM6；OPS_TimerTick/OPS_IsProcessPending：置位/查询处理周期；
 * OPS_Process：消费接收数据；
 * OPS_GetData/OPS_GetRawFrame：读取解析数据与原始帧；
 * OPS_IsDataFresh/OPS_IsPoseFresh：检查航向/完整位姿的有限性和50 ms有效期。
 */

typedef struct
{
  float heading; /* 航向角，deg。 */
  float pitch;   /* 俯仰角，deg。 */
  float roll;    /* 横滚角，deg。 */
  float x;       /* OPS绝对X坐标，mm。 */
  float y;       /* OPS绝对Y坐标，mm。 */
  float z;       /* Z轴角速度，deg/s。 */
} OPS_Data_t;

/* 初始化OPS解析器状态。 */
void OPS_Init(void);
/* 判断是否到达下一次OPS处理周期。 */
uint8_t OPS_IsProcessPending(void);
/* 置位OPS处理周期标志。 */
void OPS_TimerTick(void);
/* 处理DMA数据；返回1表示本次消费了一个TIM6处理周期。 */
uint8_t OPS_Process(void);
/* 获取最近一次解析成功的OPS数据。 */
const OPS_Data_t *OPS_GetData(void);
/* 判断最近一帧有效航向/角速度数据是否在50 ms内。 */
uint8_t OPS_IsDataFresh(void);
/* 最新完整帧的X/Y/航向/角速度均有限且在50 ms内时返回1。 */
uint8_t OPS_IsPoseFresh(void);
/* 获取最近接收完整帧的24字节原始数据。 */
const uint8_t *OPS_GetRawFrame(void);

#endif
