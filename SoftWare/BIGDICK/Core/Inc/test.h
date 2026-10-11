#ifndef TEST_H
#define TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "car_control.h"

/*
 * test.c：解析蓝牙速度/位移/模式指令，处理 PD4/PD5 按键及 GM6020 小电压测试。
 * C,<前进>,<左移>,<旋转>\r\n：仅开环接受，三个整数范围为±1000；
 * 分别映射到vx、vy、wz，超过300 ms没有合法C帧时停车。
 * P,<dx_mm>,<dy_mm>,<dangle_deg>\r\n：仅闭环接受，支持有符号十进制小数；
 * 按启动固定坐标累加目标，X向启动前方、Y向启动左侧、角度正值逆时针。
 * 每个P帧都是一次累加，只发送一次；位置目标不受300 ms速度看门狗影响。
 * M,0\r\n切开环，M,1\r\n切闭环；实际切换清除旧速度看门狗，同模式无副作用。
 * PD4 按下使地址5电机正转1圈，PD5按下使地址5电机反转1圈。
 * Test_Init：初始化缓存、看门狗与调试状态；Test_Process：主循环非阻塞处理。
 */

#define TEST_MAX_LINEAR_SPEED_MPS  CAR_CONTROL_MAX_LINEAR_SPEED_MPS
#define TEST_MAX_ANGULAR_SPEED_RAD_S CAR_CONTROL_MAX_ANGULAR_SPEED_RAD_S
#define TEST_CONTROL_TIMEOUT_MS    300U
#define TEST_BUTTON_MOTOR_ADDRESS  5U
#define TEST_BUTTON_POSITION_TENTH_DEGREE 3600U
#define TEST_BUTTON_SPEED_TENTH_RPM 3000U
#define TEST_BUTTON_ACCELERATION_RPM_S 500U
#define TEST_BUTTON_DECELERATION_RPM_S 500U
#define TEST_BUTTON_MAX_CURRENT_MA 1000U
#define TEST_BUTTON_DEBOUNCE_MS 20U

/* GM6020 上电小电压测试参数。测试完成后会自动发送零电压。 */
#define TEST_GM6020_VOLTAGE_TEST_ENABLE 1U
#define TEST_GM6020_MOTOR_ADDRESS       4U
#define TEST_GM6020_VOLTAGE_COMMAND     1000
#define TEST_GM6020_REFRESH_PERIOD_MS   10U
#define TEST_GM6020_RUN_TIME_MS         500U

/* 初始化手机蓝牙底盘测试控制模块。 */
void Test_Init(void);
/* 非阻塞处理蓝牙、按键和 GM6020 小电压测试。 */
void Test_Process(void);

#ifdef __cplusplus
}
#endif

#endif
