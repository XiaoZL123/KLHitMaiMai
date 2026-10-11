#include "car_control.h"
#include "car_run.h"
#include "ops.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

static OPS_Data_t pose;
static uint8_t heading_fresh;
static uint8_t pose_fresh;
static float sent_vx;
static float sent_vy;
static float sent_wz;
static unsigned velocity_count;
static unsigned stop_count;
static HAL_StatusTypeDef next_velocity_status;
static HAL_StatusTypeDef next_stop_status;

/* 提供可控的OPS反馈，直接测试真实控制源码。 */
const OPS_Data_t *OPS_GetData(void)
{
  return &pose;
}

/* 模拟OPS航向有效性。 */
uint8_t OPS_IsDataFresh(void)
{
  return heading_fresh;
}

/* 模拟OPS完整位姿有效性。 */
uint8_t OPS_IsPoseFresh(void)
{
  return pose_fresh;
}

/* 记录真实控制器送往底盘的速度，并注入单次发送错误。 */
HAL_StatusTypeDef CarRun_SetVelocity(float vx, float vy, float wz)
{
  HAL_StatusTypeDef status = next_velocity_status;

  assert(isfinite(vx) && isfinite(vy) && isfinite(wz));
  assert(hypotf(vx, vy) <= CAR_CONTROL_MAX_LINEAR_SPEED_MPS + 0.00001F);
  assert(fabsf(wz) <= CAR_CONTROL_MAX_ANGULAR_SPEED_RAD_S + 0.00001F);
  sent_vx = vx;
  sent_vy = vy;
  sent_wz = wz;
  velocity_count++;
  next_velocity_status = HAL_OK;
  return status;
}

/* 记录停车调用，并注入单次停车错误。 */
HAL_StatusTypeDef CarRun_Stop(void)
{
  HAL_StatusTypeDef status = next_stop_status;

  sent_vx = 0.0F;
  sent_vy = 0.0F;
  sent_wz = 0.0F;
  stop_count++;
  next_stop_status = HAL_OK;
  return status;
}

/* 比较期望速度，允许单精度计算误差。 */
static void ExpectVelocity(float vx, float vy, float wz)
{
  assert(fabsf(sent_vx - vx) < 0.00002F);
  assert(fabsf(sent_vy - vy) < 0.00002F);
  assert(fabsf(sent_wz - wz) < 0.00002F);
}

/* 设置有效的OPS位置和航向，角速度默认为零。 */
static void SetPose(float x_mm, float y_mm, float heading_deg)
{
  pose = (OPS_Data_t){.x = x_mm, .y = y_mm, .heading = heading_deg};
  heading_fresh = 1U;
  pose_fresh = 1U;
}

/* 复位硬件桩和真实控制器，但不触发CAN输出。 */
static void Reset(void)
{
  pose = (OPS_Data_t){0};
  heading_fresh = 0U;
  pose_fresh = 0U;
  sent_vx = sent_vy = sent_wz = 0.0F;
  velocity_count = stop_count = 0U;
  next_velocity_status = next_stop_status = HAL_OK;
  assert(CarControl_Init() == HAL_OK);
  assert(CarControl_GetMode() == CAR_CONTROL_MODE_CLOSED_LOOP);
}

#ifndef CONTROL_INSTALL_TEST
/* 验证未初始化及未建立完整零点时不会执行位移。 */
static void TestStartup(void)
{
  assert(CarControl_Process() == HAL_ERROR);
  assert(CarControl_Move(1.0F, 0.0F, 0.0F) == HAL_ERROR);
  assert(CarControl_SetVelocity(0.1F, 0.0F, 0.0F) == HAL_ERROR);
  assert(CarControl_SetMode(CAR_CONTROL_MODE_OPEN_LOOP) == HAL_ERROR);
  assert(CarControl_Stop() == HAL_ERROR);
  assert(CarControl_HoldHeading() == HAL_ERROR);
  Reset();
  assert(CarControl_Process() == HAL_OK);
  SetPose(1234.0F, -5678.0F, 45.0F);
  pose_fresh = 0U;
  assert(CarControl_Process() == HAL_OK);
  assert(CarControl_Move(1.0F, 0.0F, 0.0F) == HAL_ERROR);
  pose_fresh = 1U;
  assert(CarControl_Process() == HAL_OK);
  assert(velocity_count == 0U && stop_count == 0U);
  assert(CarControl_SetVelocity(0.1F, 0.0F, 0.0F) == HAL_ERROR);
  assert(CarControl_Move(0.0F, 0.0F, 0.0F) == HAL_OK);
}

/* 验证途中累加、目标保持及到位停车只发送一次。 */
static void TestAccumulation(void)
{
  Reset();
  SetPose(100.0F, 200.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  assert(CarControl_Move(500.0F, 0.0F, 0.0F) == HAL_OK);
  SetPose(100.0F, 500.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.2F, 0.0F, 0.0F);
  assert(CarControl_Move(500.0F, 0.0F, 0.0F) == HAL_OK);
  SetPose(100.0F, 900.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.3F, 0.0F, 0.0F);
  SetPose(100.0F, 1200.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  assert(stop_count == 1U);
  assert(CarControl_Process() == HAL_OK);
  assert(stop_count == 1U);
  SetPose(100.0F, 1190.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.01F, 0.0F, 0.0F);
}

/* 验证车体转向后位移仍沿启动固定坐标。 */
static void TestFixedCoordinates(void)
{
  Reset();
  SetPose(0.0F, 0.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  assert(CarControl_Move(500.0F, 0.0F, 90.0F) == HAL_OK);
  SetPose(0.0F, 0.0F, 90.0F);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.0F, -0.5F, 0.0F);
  SetPose(0.0F, 500.0F, 90.0F);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.0F, 0.0F, 0.0F);
  assert(CarControl_Move(0.0F, 500.0F, 0.0F) == HAL_OK);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.5F, 0.0F, 0.0F);
  SetPose(-500.0F, 500.0F, 90.0F);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.0F, 0.0F, 0.0F);

  Reset();
  SetPose(1000.0F, 2000.0F, 90.0F);
  assert(CarControl_Process() == HAL_OK);
  assert(CarControl_Move(500.0F, 0.0F, 0.0F) == HAL_OK);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.5F, 0.0F, 0.0F);
  SetPose(500.0F, 2000.0F, 90.0F);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.0F, 0.0F, 0.0F);
}

/* 验证跨越正负180度时保留完整正负转动圈数。 */
static void TestFullRotations(void)
{
  const float turn360[] = {90.0F, 179.0F, -90.0F, -1.0F, 0.0F};
  const float turn270[] = {90.0F, 180.0F, -90.0F};
  unsigned i;

  Reset();
  SetPose(0.0F, 0.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  assert(CarControl_Move(0.0F, 0.0F, 360.0F) == HAL_OK);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.0F, 0.0F, 2.0F);
  for (i = 0U; i < sizeof(turn360) / sizeof(turn360[0]); i++) {
    SetPose(0.0F, 0.0F, turn360[i]);
    assert(CarControl_Process() == HAL_OK);
    assert(sent_wz >= 0.0F);
  }
  ExpectVelocity(0.0F, 0.0F, 0.0F);
  assert(stop_count == 1U);
  for (int direction = -1; direction <= 1; direction += 2) {
    Reset();
    SetPose(0.0F, 0.0F, 0.0F);
    assert(CarControl_Process() == HAL_OK);
    assert(CarControl_Move(0.0F, 0.0F, 270.0F * direction) == HAL_OK);
    assert(CarControl_Process() == HAL_OK);
    assert(sent_wz * direction > 0.0F);
    for (i = 0U; i < sizeof(turn270) / sizeof(turn270[0]); i++) {
      SetPose(0.0F, 0.0F, turn270[i] * direction);
      assert(CarControl_Process() == HAL_OK);
      assert(sent_wz * direction >= 0.0F);
    }
    ExpectVelocity(0.0F, 0.0F, 0.0F);
  }
}

/* 验证径向死区、航向阻尼及平移/旋转限幅。 */
static void TestLimitsAndDeadbands(void)
{
  Reset();
  SetPose(0.0F, 0.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  assert(CarControl_Move(3.0F, 4.0F, 0.5F) == HAL_OK);
  assert(CarControl_Process() == HAL_OK);
  assert(velocity_count == 0U);
  pose.z = 10.0F;
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.0F, 0.0F, -0.06981317F);
  pose.z = 0.0F;
  assert(CarControl_Move(997.0F, 996.0F, 719.5F) == HAL_OK);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.35355339F, 0.35355339F, 2.0F);
}

/* 验证无效参数和累计溢出不会部分改变目标。 */
static void TestInvalidCommands(void)
{
  Reset();
  SetPose(0.0F, 0.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  assert(CarControl_Move(500.0F, 0.0F, 0.0F) == HAL_OK);
  assert(CarControl_Move(NAN, 100.0F, 90.0F) == HAL_ERROR);
  assert(CarControl_Move(0.0F, INFINITY, 0.0F) == HAL_ERROR);
  assert(CarControl_SetMode((CarControl_Mode)7) == HAL_ERROR);
  SetPose(0.0F, 500.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  assert(velocity_count == 0U);

  Reset();
  SetPose(0.0F, 0.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  assert(CarControl_Move(FLT_MAX, 0.0F, 0.0F) == HAL_OK);
  assert(CarControl_Move(FLT_MAX, 100.0F, 90.0F) == HAL_ERROR);
  assert(CarControl_Move(-FLT_MAX, 0.0F, 0.0F) == HAL_OK);
  assert(CarControl_Process() == HAL_OK);
  assert(velocity_count == 0U);
}

/* 验证OPS超时/NaN时停车，恢复后继续原目标和零点。 */
static void TestPoseFailure(void)
{
  Reset();
  SetPose(100.0F, 200.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  assert(CarControl_Move(500.0F, 0.0F, 0.0F) == HAL_OK);
  assert(CarControl_Process() == HAL_OK);
  heading_fresh = pose_fresh = 0U;
  assert(CarControl_Process() == HAL_OK);
  assert(stop_count == 1U);
  assert(CarControl_Process() == HAL_OK);
  assert(stop_count == 1U);
  assert(CarControl_Move(500.0F, 0.0F, 0.0F) == HAL_OK);
  SetPose(100.0F, 800.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.4F, 0.0F, 0.0F);
  pose.x = NAN;
  assert(CarControl_Process() == HAL_OK);
  assert(stop_count == 2U);
  SetPose(100.0F, 1200.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  assert(velocity_count == 2U);
}

/* 验证纯开环旁路、模式幂等和切回原位置目标。 */
static void TestModes(void)
{
  Reset();
  SetPose(0.0F, 0.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  assert(CarControl_Move(200.0F, 0.0F, 0.0F) == HAL_OK);
  assert(CarControl_Process() == HAL_OK);
  assert(CarControl_SetMode(CAR_CONTROL_MODE_OPEN_LOOP) == HAL_OK);
  assert(stop_count == 1U);
  assert(CarControl_Move(1.0F, 0.0F, 0.0F) == HAL_ERROR);
  assert(CarControl_SetVelocity(0.1F, 0.2F, 0.3F) == HAL_OK);
  assert(CarControl_SetMode(CAR_CONTROL_MODE_OPEN_LOOP) == HAL_OK);
  pose.x = NAN;
  pose.heading = NAN;
  heading_fresh = pose_fresh = 0U;
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.1F, 0.2F, 0.3F);
  assert(CarControl_SetVelocity(NAN, 0.0F, 0.0F) == HAL_ERROR);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.1F, 0.2F, 0.3F);
  assert(CarControl_SetVelocity(FLT_MAX, FLT_MAX, FLT_MAX) == HAL_OK);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.35355339F, 0.35355339F, 2.0F);
  SetPose(0.0F, 500.0F, 0.0F);
  assert(CarControl_SetMode(CAR_CONTROL_MODE_CLOSED_LOOP) == HAL_OK);
  assert(stop_count == 2U);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(-0.3F, 0.0F, 0.0F);
  unsigned before = stop_count;
  assert(CarControl_SetMode(CAR_CONTROL_MODE_CLOSED_LOOP) == HAL_OK);
  assert(stop_count == before);
}

/* 验证位置失效时开环仍累计航向整圈，停止时仍维护原零点。 */
static void TestTrackingWhileInactive(void)
{
  const float turn360[] = {90.0F, 179.0F, -90.0F, 0.0F};

  Reset();
  SetPose(0.0F, 0.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  assert(CarControl_SetMode(CAR_CONTROL_MODE_OPEN_LOOP) == HAL_OK);
  assert(CarControl_SetVelocity(0.0F, 0.0F, 0.1F) == HAL_OK);
  for (unsigned i = 0U; i < sizeof(turn360) / sizeof(turn360[0]); i++) {
    SetPose(0.0F, 0.0F, turn360[i]);
    pose_fresh = 0U;
    pose.x = NAN;
    assert(CarControl_Process() == HAL_OK);
    ExpectVelocity(0.0F, 0.0F, 0.1F);
  }
  SetPose(0.0F, 0.0F, 0.0F);
  assert(CarControl_SetMode(CAR_CONTROL_MODE_CLOSED_LOOP) == HAL_OK);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.0F, 0.0F, -2.0F);

  Reset();
  assert(CarControl_Stop() == HAL_OK);
  SetPose(100.0F, 200.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  SetPose(100.0F, 500.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  assert(CarControl_Move(500.0F, 0.0F, 0.0F) == HAL_OK);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.2F, 0.0F, 0.0F);

  Reset();
  SetPose(0.0F, 0.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  assert(CarControl_Move(0.0F, 0.0F, 360.0F) == HAL_OK);
  assert(CarControl_Process() == HAL_OK);
  assert(CarControl_Stop() == HAL_OK);
  unsigned before = velocity_count;
  for (unsigned i = 0U; i < sizeof(turn360) / sizeof(turn360[0]); i++) {
    SetPose(0.0F, 0.0F, turn360[i]);
    assert(CarControl_Process() == HAL_OK);
    assert(velocity_count == before);
  }
  assert(CarControl_Move(0.0F, 0.0F, 0.0F) == HAL_OK);
  assert(CarControl_Process() == HAL_OK);
  assert(velocity_count == before);
  ExpectVelocity(0.0F, 0.0F, 0.0F);
}

/* 验证显式停用不能被周期、重复模式或兼容入口自动解除。 */
static void TestExplicitStop(void)
{
  Reset();
  SetPose(0.0F, 0.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  assert(CarControl_Move(200.0F, 0.0F, 0.0F) == HAL_OK);
  assert(CarControl_Process() == HAL_OK);
  assert(CarControl_Stop() == HAL_OK);
  unsigned before = velocity_count;
  assert(CarControl_SetMode(CAR_CONTROL_MODE_CLOSED_LOOP) == HAL_OK);
  assert(CarControl_HoldHeading() == HAL_OK);
  assert(CarControl_Process() == HAL_OK);
  assert(velocity_count == before);
  assert(CarControl_Move(NAN, 0.0F, 0.0F) == HAL_ERROR);
  assert(CarControl_Process() == HAL_OK);
  assert(velocity_count == before);
  assert(CarControl_Move(0.0F, 0.0F, 0.0F) == HAL_OK);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.2F, 0.0F, 0.0F);
  assert(CarControl_SetMode(CAR_CONTROL_MODE_OPEN_LOOP) == HAL_OK);
  assert(CarControl_SetVelocity(0.1F, 0.0F, 0.0F) == HAL_OK);
  assert(CarControl_Process() == HAL_OK);
  assert(CarControl_HoldHeading() == HAL_OK);
  before = velocity_count;
  assert(CarControl_Process() == HAL_OK);
  assert(velocity_count == before);
  assert(CarControl_SetVelocity(0.2F, 0.0F, 0.0F) == HAL_OK);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.2F, 0.0F, 0.0F);
}

/* 验证CAN错误尝试停车，禁用周期输出且保留目标。 */
static void TestCanErrors(void)
{
  Reset();
  SetPose(0.0F, 0.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  assert(CarControl_Move(200.0F, 0.0F, 0.0F) == HAL_OK);
  next_velocity_status = HAL_TIMEOUT;
  assert(CarControl_Process() == HAL_TIMEOUT);
  assert(stop_count == 1U);
  unsigned before = velocity_count;
  assert(CarControl_Process() == HAL_OK);
  assert(velocity_count == before);
  assert(CarControl_Move(0.0F, 0.0F, 0.0F) == HAL_OK);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.2F, 0.0F, 0.0F);
  next_stop_status = HAL_BUSY;
  heading_fresh = pose_fresh = 0U;
  assert(CarControl_Process() == HAL_BUSY);
  assert(stop_count == 3U);
  before = velocity_count;
  SetPose(0.0F, 0.0F, 0.0F);
  assert(CarControl_Process() == HAL_OK);
  assert(velocity_count == before);
  assert(CarControl_SetMode(CAR_CONTROL_MODE_OPEN_LOOP) == HAL_OK);
  assert(CarControl_SetMode(CAR_CONTROL_MODE_CLOSED_LOOP) == HAL_OK);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.2F, 0.0F, 0.0F);
}

/* 运行真实默认配置下的控制行为验证。 */
int main(void)
{
  TestStartup();
  TestAccumulation();
  TestFixedCoordinates();
  TestFullRotations();
  TestLimitsAndDeadbands();
  TestInvalidCommands();
  TestPoseFailure();
  TestModes();
  TestTrackingWhileInactive();
  TestExplicitStop();
  TestCanErrors();
  puts("PASS: 11 control scenarios using real car_control.c and public headers");
  return 0;
}
#else
/* 验证非零安装偏角、中心偏移和相反航向符号的整体转换。 */
int main(void)
{
  Reset();
  SetPose(-50.0F, 100.0F, -90.0F);
  assert(CarControl_Process() == HAL_OK);
  assert(velocity_count == 0U);
  SetPose(-100.0F, -50.0F, -180.0F);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.0F, 0.0F, -2.0F);
  assert(CarControl_Move(500.0F, 0.0F, 90.0F) == HAL_OK);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.0F, -0.5F, 0.0F);
  SetPose(-100.0F, 450.0F, -180.0F);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.0F, 0.0F, 0.0F);
  pose.z = -10.0F;
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.0F, 0.0F, -0.06981317F);

  /* 启动时车体已朝世界左方；转90度只改变传感器偏移，不改变底盘中心。 */
  Reset();
  SetPose(-400.0F, 150.0F, -180.0F);
  assert(CarControl_Process() == HAL_OK);
  assert(CarControl_Move(500.0F, 0.0F, 90.0F) == HAL_OK);
  SetPose(-250.0F, 100.0F, 90.0F);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.0F, -0.5F, 0.0F);
  SetPose(-750.0F, 100.0F, 90.0F);
  assert(CarControl_Process() == HAL_OK);
  ExpectVelocity(0.0F, 0.0F, 0.0F);
  puts("PASS: installation yaw, sensor offset and reversed heading/gyro signs");
  return 0;
}
#endif
