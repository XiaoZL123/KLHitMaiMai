#ifndef CAR_CONTROL_H
#define CAR_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/*
 * car_control.c：使用OPS位姿进行位置P、航向PD闭环，或旁路为纯开环速度控制。
 * CarControl_Init：默认闭环并等待启动零点；CarControl_Move：累加固定坐标位移目标；
 * CarControl_SetMode/GetMode：切换/查询模式；CarControl_SetVelocity：更新开环速度；
 * CarControl_HoldHeading：兼容停车入口，不修改闭环目标；CarControl_Stop：显式停用；
 * CarControl_Process：每个5 ms周期跟踪位姿并更新底盘输出。
 */
#define CAR_CONTROL_POSITION_KP 1.0F
#define CAR_CONTROL_POSITION_DEADBAND_MM 5.0F
#define CAR_CONTROL_HEADING_KP 3.0F
#define CAR_CONTROL_HEADING_KD 0.4F
#define CAR_CONTROL_HEADING_DEADBAND_DEG 1.0F
#define CAR_CONTROL_MAX_LINEAR_SPEED_MPS 0.5F
#define CAR_CONTROL_MAX_ANGULAR_SPEED_RAD_S 2.0F

/* 默认OPS +Y朝前、+X朝右；以下安装参数及航向正方向需要实车核对。 */
#ifndef CAR_CONTROL_OPS_INSTALL_YAW_DEG
#define CAR_CONTROL_OPS_INSTALL_YAW_DEG 0.0F /* OPS前方相对车头的逆时针偏角。 */
#endif
#ifndef CAR_CONTROL_OPS_OFFSET_FORWARD_MM
#define CAR_CONTROL_OPS_OFFSET_FORWARD_MM 0.0F /* OPS中心相对底盘中心的前向距离。 */
#endif
#ifndef CAR_CONTROL_OPS_OFFSET_LEFT_MM
#define CAR_CONTROL_OPS_OFFSET_LEFT_MM 0.0F /* OPS中心相对底盘中心的左向距离。 */
#endif
#ifndef CAR_CONTROL_OPS_HEADING_SIGN
#define CAR_CONTROL_OPS_HEADING_SIGN 1.0F /* 原始航向/角速度逆时针为正；相反时设为-1。 */
#endif

typedef enum {
  CAR_CONTROL_MODE_OPEN_LOOP = 0,
  CAR_CONTROL_MODE_CLOSED_LOOP = 1
} CarControl_Mode;

/* 默认启用闭环，首个有效OPS位姿作为零点；不发送运动命令。 */
HAL_StatusTypeDef CarControl_Init(void);
/* 在启动固定坐标累加目标：X前、Y左（mm），角度逆时针（deg，可整圈）；仅闭环且零点就绪有效。 */
HAL_StatusTypeDef CarControl_Move(float dx_mm, float dy_mm, float dangle_deg);
/* 切换模式时先停车、清开环速度，保留零点/目标并启用控制；同模式重复调用无副作用。 */
HAL_StatusTypeDef CarControl_SetMode(CarControl_Mode mode);
/* 查询当前模式；初始化后的默认值为CLOSED_LOOP。 */
CarControl_Mode CarControl_GetMode(void);
/* 仅开环更新车体速度：前、左、逆时针，单位m/s、m/s、rad/s；有限值会统一限幅。 */
HAL_StatusTypeDef CarControl_SetVelocity(float vx, float vy, float wz);
/* 兼容入口：清开环速度并按需停车，保留当前模式/闭环目标，不解除显式停用。 */
HAL_StatusTypeDef CarControl_HoldHeading(void);
/* 立即停车并禁用周期输出，保留目标；有效Move/SetVelocity或实际模式切换可恢复。 */
HAL_StatusTypeDef CarControl_Stop(void);
/* 主循环每5 ms调用一次；闭环位姿无效时停车保留目标，开环不依赖OPS，发送失败禁用输出。 */
HAL_StatusTypeDef CarControl_Process(void);

#ifdef __cplusplus
}
#endif

#endif
