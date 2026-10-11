#ifndef CAR_RUN_H
#define CAR_RUN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "x42s_motor.h"
#include <stdint.h>

/* car_run.c：完成麦克纳姆底盘运动解算，并通过 X42S 同步控制四个电机。 */

#define CAR_RUN_DEFAULT_WHEEL_DIAMETER_M 0.1F
#define CAR_RUN_DEFAULT_WHEEL_TRACK_M    0.2F
#define CAR_RUN_DEFAULT_WHEEL_BASE_M     0.2F
#define CAR_RUN_DEFAULT_MAX_WHEEL_RPM    3000.0F
#define CAR_RUN_DEFAULT_ACCELERATION_RPM_S 1000U

typedef struct
{
  float wheel_diameter_m;       /* 麦轮直径，单位 m。 */
  float wheel_track_m;          /* 左右轮中心距，单位 m。 */
  float wheel_base_m;           /* 前后轮中心距，单位 m。 */
  float max_wheel_speed_rpm;    /* 单轮最大目标转速，单位 RPM。 */
  uint16_t acceleration_rpm_s;  /* 速度指令加速度，单位 RPM/s。 */
  int8_t wheel_sign[4];         /* 左前、右前、左后、右后的方向修正。 */
} CarRun_Config;

/* 初始化底盘控制模块并保存 CAN 句柄与运动参数。 */
HAL_StatusTypeDef CarRun_Init(FDCAN_HandleTypeDef *hfdcan,
                              const CarRun_Config *config);

/* 设置车体速度：vx 前进、vy 左移、wz 逆时针，单位分别为 m/s、m/s、rad/s。 */
HAL_StatusTypeDef CarRun_SetVelocity(float vx, float vy, float wz);

/* 同步停止底盘四个电机。 */
HAL_StatusTypeDef CarRun_Stop(void);

#ifdef __cplusplus
}
#endif

#endif
