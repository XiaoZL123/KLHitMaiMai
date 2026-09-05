#ifndef X42S_MOTOR_H
#define X42S_MOTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fdcan.h"
#include <stdbool.h>
#include <stdint.h>

#define X42S_CHECKSUM_FIXED 0x6BU
#define X42S_BROADCAST_ADDR 0x00U
#define X42S_STATUS_ENABLED  0x01U
#define X42S_STATUS_REACHED  0x02U
#define X42S_STATUS_STALL    0x0CU
#define X42S_STATUS_LEFT_LIMIT  0x10U
#define X42S_STATUS_RIGHT_LIMIT 0x20U
#define X42S_STATUS_POWER_LOSS  0x80U

typedef enum
{
  X42S_DIR_CW = 0,
  X42S_DIR_CCW = 1
} X42S_Direction;

typedef enum
{
  X42S_MOVE_RELATIVE_LAST = 0,
  X42S_MOVE_ABSOLUTE_ZERO = 1,
  X42S_MOVE_RELATIVE_NOW = 2
} X42S_MoveMode;

typedef enum
{
  X42S_REPLY_NONE = 0,
  X42S_REPLY_OK = 0x02,
  X42S_REPLY_REACHED = 0x9F,
  X42S_REPLY_PARAM_ERROR = 0xE2,
  X42S_REPLY_FORMAT_ERROR = 0xEE
} X42S_ReplyCode;

typedef struct
{
  bool valid;
  uint8_t address;
  uint8_t function;
  uint8_t reply;
  int16_t speed_tenth_rpm;
  int32_t position_tenth_degree;
  uint16_t bus_voltage_mv;
  uint16_t phase_current_ma;
  uint8_t motor_status;
  uint32_t rx_count;
} X42S_Feedback;

HAL_StatusTypeDef X42S_Init(FDCAN_HandleTypeDef *hfdcan);
void X42S_Process(void);
HAL_StatusTypeDef X42S_SendRaw(uint8_t address, const uint8_t *payload,
                              uint8_t length);
HAL_StatusTypeDef X42S_Enable(uint8_t address, bool enable, bool sync);
HAL_StatusTypeDef X42S_ClearProtection(uint8_t address);
HAL_StatusTypeDef X42S_SetCurrentPositionZero(uint8_t address);
HAL_StatusTypeDef X42S_SetSpeed(uint8_t address, float target_speed_rpm,
                               uint16_t acceleration_rpm_s, bool sync);
HAL_StatusTypeDef X42S_SetPosition(uint8_t address, X42S_Direction direction,
                                  uint16_t acceleration_rpm_s,
                                  uint16_t deceleration_rpm_s,
                                  uint16_t speed_tenth_rpm,
                                  uint32_t position_tenth_degree,
                                  X42S_MoveMode mode,
                                  uint16_t max_current_ma, bool sync);
HAL_StatusTypeDef X42S_Stop(uint8_t address, bool sync);
HAL_StatusTypeDef X42S_SyncStart(void);
HAL_StatusTypeDef X42S_ReadSpeed(uint8_t address);
HAL_StatusTypeDef X42S_ReadPosition(uint8_t address);
HAL_StatusTypeDef X42S_ReadStatus(uint8_t address);
HAL_StatusTypeDef X42S_ReadBusVoltage(uint8_t address);
HAL_StatusTypeDef X42S_ReadPhaseCurrent(uint8_t address);
const X42S_Feedback *X42S_GetFeedback(uint8_t address);

#ifdef __cplusplus
}
#endif

#endif
