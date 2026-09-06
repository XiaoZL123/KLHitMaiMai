#ifndef OPS_H
#define OPS_H

#include "main.h"

typedef struct
{
  float heading;
  float pitch;
  float roll;
  float x;
  float y;
  float z;
} OPS_Data_t;

/* 初始化OPS解析器状态。 */
void OPS_Init(void);
/* 判断是否到达下一次OPS处理周期。 */
uint8_t OPS_IsProcessPending(void);
/* 置位OPS处理周期标志。 */
void OPS_TimerTick(void);
/* 处理DMA接收缓冲区中的新数据。 */
void OPS_Process(void);
/* 获取最近一次解析成功的OPS数据。 */
const OPS_Data_t *OPS_GetData(void);
/* 获取最近接收完整帧的24字节原始数据。 */
const uint8_t *OPS_GetRawFrame(void);

#endif
