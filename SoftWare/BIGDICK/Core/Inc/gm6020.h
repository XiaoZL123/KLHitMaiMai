#ifndef GM6020_H
#define GM6020_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fdcan.h"
#include <stdint.h>
#include <stdbool.h>

#define GM6020_MIN_ID 1U
#define GM6020_MAX_ID 7U
#define GM6020_FEEDBACK_ID_BASE 0x204U
#define GM6020_VOLTAGE_ID_LOW 0x1FFU
#define GM6020_VOLTAGE_ID_HIGH 0x2FFU
#define GM6020_VOLTAGE_LIMIT 25000

/*
 * gm6020.c：管理 GM6020 标准 CAN 帧，解析反馈并发送电压控制指令。
 * FDCAN1_Process 是 FDCAN1 RX FIFO0 的统一消费者，标准帧通过
 * GM6020_HandleFrame 交给本模块，避免各驱动重复读取同一个 FIFO。
 */
typedef struct
{
  bool valid;
  uint8_t id;
  uint16_t angle_ecd;
  int16_t speed_rpm;
  int16_t torque_current;
  uint8_t temperature;
  uint32_t rx_count;
  uint32_t last_rx_tick;
} GM6020_Feedback;

/* 绑定 GM6020 使用的 FDCAN 句柄并清零反馈状态。 */
HAL_StatusTypeDef GM6020_Init(FDCAN_HandleTypeDef *hfdcan);
/* 解析已经从 FDCAN FIFO 取出的 GM6020 标准反馈帧。 */
void GM6020_HandleFrame(const FDCAN_RxHeaderTypeDef *header,
                        const uint8_t *data, uint8_t length);
/* 兼容旧调用方式，转交给 FDCAN1_Process 统一处理 FIFO。 */
void GM6020_Process(void);
/* 设置指定电机电压指令，范围为 -25000～25000。 */
HAL_StatusTypeDef GM6020_SetVoltage(uint8_t id, int16_t voltage);
/* 清除所有 GM6020 电压指令并发送停止帧。 */
HAL_StatusTypeDef GM6020_Stop(void);
/* 获取指定地址最近一次反馈数据。 */
const GM6020_Feedback *GM6020_GetFeedback(uint8_t id);

#ifdef __cplusplus
}
#endif

#endif
