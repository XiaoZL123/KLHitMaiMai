#include "car_run.h"

#include <math.h>
#include <string.h>

#define CAR_RUN_PI 3.14159265358979323846F
#define CAR_RUN_WHEEL_COUNT 4U

typedef enum
{
  CAR_RUN_WHEEL_LEFT_FRONT = 0,
  CAR_RUN_WHEEL_RIGHT_FRONT,
  CAR_RUN_WHEEL_LEFT_REAR,
  CAR_RUN_WHEEL_RIGHT_REAR
} CarRun_Wheel;

static FDCAN_HandleTypeDef *car_run_can;
static CarRun_Config car_run_config;
static bool car_run_initialized;

static const uint8_t car_run_addresses[CAR_RUN_WHEEL_COUNT] = {1U, 2U, 4U, 3U};

/* 检查底盘配置和速度输入是否有效。 */
static bool CarRun_IsConfigValid(const CarRun_Config *config)
{
  uint32_t index;

  if ((config == NULL) || !isfinite(config->wheel_diameter_m) ||
      !isfinite(config->wheel_track_m) || !isfinite(config->wheel_base_m) ||
      !isfinite(config->max_wheel_speed_rpm) ||
      (config->wheel_diameter_m <= 0.0F) ||
      (config->wheel_track_m <= 0.0F) || (config->wheel_base_m <= 0.0F) ||
      (config->max_wheel_speed_rpm <= 0.0F) ||
      (config->max_wheel_speed_rpm > 3000.0F))
  {
    return false;
  }

  for (index = 0U; index < CAR_RUN_WHEEL_COUNT; index++)
  {
    if ((config->wheel_sign[index] != 1) &&
        (config->wheel_sign[index] != -1))
    {
      return false;
    }
  }

  return true;
}

/* 检查三个车体速度是否为有限浮点数。 */
static bool CarRun_IsVelocityValid(float vx, float vy, float wz)
{
  return isfinite(vx) && isfinite(vy) && isfinite(wz);
}

/* 将四轮速度按最大轮速统一缩放，保持运动方向比例。 */
static void CarRun_LimitWheelSpeeds(float *wheel_rpm)
{
  float maximum = 0.0F;
  float scale;
  uint32_t index;

  for (index = 0U; index < CAR_RUN_WHEEL_COUNT; index++)
  {
    if (fabsf(wheel_rpm[index]) > maximum)
    {
      maximum = fabsf(wheel_rpm[index]);
    }
  }

  if (maximum <= car_run_config.max_wheel_speed_rpm)
  {
    return;
  }

  scale = car_run_config.max_wheel_speed_rpm / maximum;
  for (index = 0U; index < CAR_RUN_WHEEL_COUNT; index++)
  {
    wheel_rpm[index] *= scale;
  }
}

/* 初始化底盘控制模块并保存配置。 */
HAL_StatusTypeDef CarRun_Init(FDCAN_HandleTypeDef *hfdcan,
                              const CarRun_Config *config)
{
  if ((hfdcan == NULL) || !CarRun_IsConfigValid(config))
  {
    car_run_initialized = false;
    return HAL_ERROR;
  }

  car_run_can = hfdcan;
  memcpy(&car_run_config, config, sizeof(car_run_config));
  car_run_initialized = true;
  return HAL_OK;
}

/* 根据车体速度解算四轮转速并同步发送速度指令。 */
HAL_StatusTypeDef CarRun_SetVelocity(float vx, float vy, float wz)
{
  float wheel_linear[CAR_RUN_WHEEL_COUNT];
  float wheel_rpm[CAR_RUN_WHEEL_COUNT];
  float half_track;
  float half_base;
  float rotation_term;
  uint32_t index;
  HAL_StatusTypeDef status;

  if (!car_run_initialized || (car_run_can == NULL) ||
      !CarRun_IsVelocityValid(vx, vy, wz))
  {
    return HAL_ERROR;
  }

  if ((vx == 0.0F) && (vy == 0.0F) && (wz == 0.0F))
  {
    return CarRun_Stop();
  }

  half_track = car_run_config.wheel_track_m * 0.5F;
  half_base = car_run_config.wheel_base_m * 0.5F;
  rotation_term = (half_track + half_base) * wz;

  wheel_linear[CAR_RUN_WHEEL_LEFT_FRONT] = vx - vy - rotation_term;
  wheel_linear[CAR_RUN_WHEEL_RIGHT_FRONT] = vx + vy + rotation_term;
  wheel_linear[CAR_RUN_WHEEL_LEFT_REAR] = vx + vy - rotation_term;
  wheel_linear[CAR_RUN_WHEEL_RIGHT_REAR] = vx - vy + rotation_term;

  for (index = 0U; index < CAR_RUN_WHEEL_COUNT; index++)
  {
    wheel_rpm[index] = wheel_linear[index] *
                       (60.0F / (CAR_RUN_PI *
                                 car_run_config.wheel_diameter_m));
  }

  CarRun_LimitWheelSpeeds(wheel_rpm);

  for (index = 0U; index < CAR_RUN_WHEEL_COUNT; index++)
  {
    wheel_rpm[index] *= (float)car_run_config.wheel_sign[index];
    status = X42S_SetSpeed(car_run_addresses[index], wheel_rpm[index],
                           car_run_config.acceleration_rpm_s, true);
    if (status != HAL_OK)
    {
      return status;
    }
  }

  return X42S_SyncStart();
}

/* 向四个电机发送同步停止指令。 */
HAL_StatusTypeDef CarRun_Stop(void)
{
  uint32_t index;
  HAL_StatusTypeDef status;

  if (!car_run_initialized || (car_run_can == NULL))
  {
    return HAL_ERROR;
  }

  for (index = 0U; index < CAR_RUN_WHEEL_COUNT; index++)
  {
    status = X42S_Stop(car_run_addresses[index], true);
    if (status != HAL_OK)
    {
      return status;
    }
  }

  return X42S_SyncStart();
}
