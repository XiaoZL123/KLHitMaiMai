#include "car_control.h"

#include "car_run.h"
#include "ops.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#define CAR_CONTROL_DEG_TO_RAD 0.017453292519943295F
#define CAR_CONTROL_MM_TO_M 0.001F

typedef struct {
  float vx;
  float vy;
  float wz;
  float origin_x_mm;
  float origin_y_mm;
  float origin_heading_rad;
  float previous_heading_deg;
  float x_mm;
  float y_mm;
  float angle_deg;
  float target_x_mm;
  float target_y_mm;
  float target_angle_deg;
  CarControl_Mode mode;
  bool pose_ready;
  bool control_active;
  bool drive_output_active;
} CarControl_State;

static CarControl_State car_control_state;
static bool car_control_initialized;

/* 对相邻航向求最短角差，仅用于展开反馈，不缩短目标转动圈数。 */
static float CarControl_HeadingDelta(float current_deg, float previous_deg)
{
  return remainderf(current_deg - previous_deg, 360.0F);
}

/* 跟踪启动固定坐标位姿；位置无效时仍尽量跟踪有效航向。 */
static bool CarControl_UpdatePose(void)
{
  const OPS_Data_t *data = OPS_GetData();
  float heading_deg;
  float heading_rad;
  float center_x_mm;
  float center_y_mm;
  float dx_mm;
  float dy_mm;
  bool pose_fresh;

  if ((data == NULL) || (OPS_IsDataFresh() == 0U) ||
      !isfinite(data->heading) || !isfinite(data->z)) {
    return false;
  }

  heading_deg = remainderf(CAR_CONTROL_OPS_HEADING_SIGN * data->heading, 360.0F);
  if (car_control_state.pose_ready) {
    car_control_state.angle_deg += CarControl_HeadingDelta(
        heading_deg, car_control_state.previous_heading_deg);
    car_control_state.previous_heading_deg = heading_deg;
  }

  pose_fresh = (OPS_IsPoseFresh() != 0U) &&
               isfinite(data->x) && isfinite(data->y);
  if (!pose_fresh) {
    return false;
  }

  heading_rad = (heading_deg - CAR_CONTROL_OPS_INSTALL_YAW_DEG) *
                CAR_CONTROL_DEG_TO_RAD;
  center_x_mm = data->y - cosf(heading_rad) * CAR_CONTROL_OPS_OFFSET_FORWARD_MM +
                sinf(heading_rad) * CAR_CONTROL_OPS_OFFSET_LEFT_MM;
  center_y_mm = -data->x - sinf(heading_rad) * CAR_CONTROL_OPS_OFFSET_FORWARD_MM -
                cosf(heading_rad) * CAR_CONTROL_OPS_OFFSET_LEFT_MM;
  if (!isfinite(center_x_mm) || !isfinite(center_y_mm)) {
    return false;
  }

  if (!car_control_state.pose_ready) {
    car_control_state.origin_x_mm = center_x_mm;
    car_control_state.origin_y_mm = center_y_mm;
    car_control_state.origin_heading_rad = heading_rad;
    car_control_state.previous_heading_deg = heading_deg;
    car_control_state.pose_ready = true;
  }

  dx_mm = center_x_mm - car_control_state.origin_x_mm;
  dy_mm = center_y_mm - car_control_state.origin_y_mm;
  car_control_state.x_mm = cosf(car_control_state.origin_heading_rad) * dx_mm +
                           sinf(car_control_state.origin_heading_rad) * dy_mm;
  car_control_state.y_mm = -sinf(car_control_state.origin_heading_rad) * dx_mm +
                           cosf(car_control_state.origin_heading_rad) * dy_mm;
  return isfinite(car_control_state.x_mm) && isfinite(car_control_state.y_mm) &&
         isfinite(car_control_state.angle_deg);
}

/* 保持平移方向并限制合速度，同时限制角速度。 */
static void CarControl_LimitVelocity(float *vx, float *vy, float *wz)
{
  float largest = fmaxf(fabsf(*vx), fabsf(*vy));

  if (largest > 0.0F) {
    float norm_x = *vx / largest;
    float norm_y = *vy / largest;
    float allowed = CAR_CONTROL_MAX_LINEAR_SPEED_MPS / hypotf(norm_x, norm_y);

    if (largest > allowed) {
      *vx = norm_x * allowed;
      *vy = norm_y * allowed;
    }
  }
  *wz = fmaxf(-CAR_CONTROL_MAX_ANGULAR_SPEED_RAD_S,
              fminf(*wz, CAR_CONTROL_MAX_ANGULAR_SPEED_RAD_S));
}

/* 发送错误后尝试停车并停用控制，保留位置目标供显式恢复。 */
static HAL_StatusTypeDef CarControl_DisableOnError(HAL_StatusTypeDef status)
{
  car_control_state.vx = 0.0F;
  car_control_state.vy = 0.0F;
  car_control_state.wz = 0.0F;
  car_control_state.control_active = false;
  (void)CarRun_Stop();
  car_control_state.drive_output_active = false;
  return status;
}

/* 仅在已有运动输出时发送一次停止帧。 */
static HAL_StatusTypeDef CarControl_StopOutput(void)
{
  HAL_StatusTypeDef status;

  if (!car_control_state.drive_output_active) {
    return HAL_OK;
  }
  status = CarRun_Stop();
  if (status != HAL_OK) {
    return CarControl_DisableOnError(status);
  }
  car_control_state.drive_output_active = false;
  return HAL_OK;
}

/* 根据固定坐标位置误差和展开航向计算当前车体闭环速度。 */
static void CarControl_CalculateVelocity(const OPS_Data_t *data,
                                         float *vx, float *vy, float *wz)
{
  float ex_m = car_control_state.target_x_mm * CAR_CONTROL_MM_TO_M -
               car_control_state.x_mm * CAR_CONTROL_MM_TO_M;
  float ey_m = car_control_state.target_y_mm * CAR_CONTROL_MM_TO_M -
               car_control_state.y_mm * CAR_CONTROL_MM_TO_M;
  float angle_error_rad = car_control_state.target_angle_deg * CAR_CONTROL_DEG_TO_RAD -
                          car_control_state.angle_deg * CAR_CONTROL_DEG_TO_RAD;
  float angle_rad = remainderf(car_control_state.angle_deg, 360.0F) *
                    CAR_CONTROL_DEG_TO_RAD;
  float vx_fixed = 0.0F;
  float vy_fixed = 0.0F;

  if (hypotf(ex_m, ey_m) > CAR_CONTROL_POSITION_DEADBAND_MM * CAR_CONTROL_MM_TO_M) {
    vx_fixed = CAR_CONTROL_POSITION_KP * ex_m;
    vy_fixed = CAR_CONTROL_POSITION_KP * ey_m;
  }
  if (fabsf(angle_error_rad) <= CAR_CONTROL_HEADING_DEADBAND_DEG * CAR_CONTROL_DEG_TO_RAD) {
    angle_error_rad = 0.0F;
  }

  *wz = CAR_CONTROL_HEADING_KP * angle_error_rad -
        CAR_CONTROL_HEADING_KD * CAR_CONTROL_OPS_HEADING_SIGN * data->z *
        CAR_CONTROL_DEG_TO_RAD;
  CarControl_LimitVelocity(&vx_fixed, &vy_fixed, wz);
  *vx = cosf(angle_rad) * vx_fixed + sinf(angle_rad) * vy_fixed;
  *vy = -sinf(angle_rad) * vx_fixed + cosf(angle_rad) * vy_fixed;
}

/* 默认启用位置闭环并等待首个有效位姿，不发送底盘运动命令。 */
HAL_StatusTypeDef CarControl_Init(void)
{
  car_control_state = (CarControl_State){0};
  car_control_state.mode = CAR_CONTROL_MODE_CLOSED_LOOP;
  car_control_state.control_active = true;
  car_control_initialized = true;
  return HAL_OK;
}

/* 在启动固定坐标内将位移和转角累加到已有目标。 */
HAL_StatusTypeDef CarControl_Move(float dx_mm, float dy_mm, float dangle_deg)
{
  float target_x_mm;
  float target_y_mm;
  float target_angle_deg;

  if (!car_control_initialized || !car_control_state.pose_ready ||
      (car_control_state.mode != CAR_CONTROL_MODE_CLOSED_LOOP) ||
      !isfinite(dx_mm) || !isfinite(dy_mm) || !isfinite(dangle_deg)) {
    return HAL_ERROR;
  }
  target_x_mm = car_control_state.target_x_mm + dx_mm;
  target_y_mm = car_control_state.target_y_mm + dy_mm;
  target_angle_deg = car_control_state.target_angle_deg + dangle_deg;
  if (!isfinite(target_x_mm) || !isfinite(target_y_mm) || !isfinite(target_angle_deg)) {
    return HAL_ERROR;
  }

  car_control_state.target_x_mm = target_x_mm;
  car_control_state.target_y_mm = target_y_mm;
  car_control_state.target_angle_deg = target_angle_deg;
  car_control_state.control_active = true;
  return HAL_OK;
}

/* 切换模式前停止旧输出，保留启动零点和位移目标。 */
HAL_StatusTypeDef CarControl_SetMode(CarControl_Mode mode)
{
  HAL_StatusTypeDef status;

  if (!car_control_initialized ||
      ((mode != CAR_CONTROL_MODE_OPEN_LOOP) && (mode != CAR_CONTROL_MODE_CLOSED_LOOP))) {
    return HAL_ERROR;
  }
  if (mode == car_control_state.mode) {
    return HAL_OK;
  }
  status = CarControl_StopOutput();
  if (status != HAL_OK) {
    return status;
  }

  car_control_state.vx = 0.0F;
  car_control_state.vy = 0.0F;
  car_control_state.wz = 0.0F;
  car_control_state.mode = mode;
  car_control_state.control_active = true;
  return HAL_OK;
}

/* 返回当前开环或闭环模式。 */
CarControl_Mode CarControl_GetMode(void)
{
  return car_control_state.mode;
}

/* 仅在开环模式保存限幅后的手动车体速度。 */
HAL_StatusTypeDef CarControl_SetVelocity(float vx, float vy, float wz)
{
  if (!car_control_initialized || (car_control_state.mode != CAR_CONTROL_MODE_OPEN_LOOP) ||
      !isfinite(vx) || !isfinite(vy) || !isfinite(wz)) {
    return HAL_ERROR;
  }
  CarControl_LimitVelocity(&vx, &vy, &wz);
  car_control_state.vx = vx;
  car_control_state.vy = vy;
  car_control_state.wz = wz;
  car_control_state.control_active = true;
  return HAL_OK;
}

/* 兼容旧停车入口：清开环速度，不修改模式或位置目标。 */
HAL_StatusTypeDef CarControl_HoldHeading(void)
{
  if (!car_control_initialized) {
    return HAL_ERROR;
  }
  car_control_state.vx = 0.0F;
  car_control_state.vy = 0.0F;
  car_control_state.wz = 0.0F;
  return CarControl_StopOutput();
}

/* 保留目标但立即停车并停用周期输出。 */
HAL_StatusTypeDef CarControl_Stop(void)
{
  HAL_StatusTypeDef status;

  if (!car_control_initialized) {
    return HAL_ERROR;
  }
  car_control_state.vx = 0.0F;
  car_control_state.vy = 0.0F;
  car_control_state.wz = 0.0F;
  car_control_state.control_active = false;
  status = CarRun_Stop();
  if (status != HAL_OK) {
    return CarControl_DisableOnError(status);
  }
  car_control_state.drive_output_active = false;
  return HAL_OK;
}

/* 主循环每5 ms跟踪位姿，再按模式发送速度或一次停车命令。 */
HAL_StatusTypeDef CarControl_Process(void)
{
  bool pose_fresh;
  float vx;
  float vy;
  float wz;
  HAL_StatusTypeDef status;

  if (!car_control_initialized) {
    return HAL_ERROR;
  }
  pose_fresh = CarControl_UpdatePose();
  if (!car_control_state.control_active) {
    return HAL_OK;
  }
  if (car_control_state.mode == CAR_CONTROL_MODE_CLOSED_LOOP) {
    if (!pose_fresh) {
      return CarControl_StopOutput();
    }
    CarControl_CalculateVelocity(OPS_GetData(), &vx, &vy, &wz);
  } else {
    vx = car_control_state.vx;
    vy = car_control_state.vy;
    wz = car_control_state.wz;
  }
  if (!isfinite(vx) || !isfinite(vy) || !isfinite(wz)) {
    return CarControl_DisableOnError(HAL_ERROR);
  }
  if ((vx == 0.0F) && (vy == 0.0F) && (wz == 0.0F)) {
    return CarControl_StopOutput();
  }

  status = CarRun_SetVelocity(vx, vy, wz);
  if (status != HAL_OK) {
    return CarControl_DisableOnError(status);
  }
  car_control_state.drive_output_active = true;
  return HAL_OK;
}
