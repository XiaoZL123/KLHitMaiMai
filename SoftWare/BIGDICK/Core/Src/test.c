#include "test.h"

#include "car_control.h"
#include "gm6020.h"
#include "x42s_motor.h"
#include "usart.h"

#include <stdbool.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>

#define TEST_FRAME_MAX_LENGTH 96U
#define TEST_INPUT_LIMIT      1000L

typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
  GPIO_PinState stable_state;
  GPIO_PinState last_sample_state;
  uint32_t last_change_tick;
} Test_ButtonState;

static char test_frame[TEST_FRAME_MAX_LENGTH + 1U];
static uint16_t test_frame_length;
static bool test_frame_overflow;
static uint32_t test_last_command_tick;
static bool test_command_received;
static bool test_stop_sent;
static Test_ButtonState test_button_cw;
static Test_ButtonState test_button_ccw;
#if TEST_GM6020_VOLTAGE_TEST_ENABLE
static bool test_gm6020_active;
static bool test_gm6020_started;
static uint32_t test_gm6020_start_tick;
static uint32_t test_gm6020_last_send_tick;
#endif

/* 解析一个范围为-1000到1000的有符号控制量。 */
static bool Test_ParseValue(const char **cursor, int32_t *value)
{
  char *end;
  const char *digit = *cursor;
  long parsed;

  if ((*digit == '+') || (*digit == '-'))
  {
    digit++;
  }
  if ((*digit < '0') || (*digit > '9'))
  {
    return false;
  }

  errno = 0;
  parsed = strtol(*cursor, &end, 10);
  if ((errno == ERANGE) || (end == *cursor) || (parsed < -TEST_INPUT_LIMIT) ||
      (parsed > TEST_INPUT_LIMIT))
  {
    return false;
  }

  *value = (int32_t)parsed;
  *cursor = end;
  return true;
}

/* 解析普通有符号十进制小数，拒绝空白、指数、非有限值和范围错误。 */
static bool Test_ParseDecimal(const char **cursor, float *value)
{
  const char *start = *cursor;
  const char *number = start;
  char *end;
  bool has_digit = false;
  float parsed;

  if ((*number == '+') || (*number == '-'))
  {
    number++;
  }
  while ((*number >= '0') && (*number <= '9'))
  {
    has_digit = true;
    number++;
  }
  if (*number == '.')
  {
    number++;
    while ((*number >= '0') && (*number <= '9'))
    {
      has_digit = true;
      number++;
    }
  }
  if (!has_digit || ((*number != ',') && (*number != '\0')))
  {
    return false;
  }

  errno = 0;
  parsed = strtof(start, &end);
  if ((end != number) || (errno == ERANGE) || !isfinite(parsed))
  {
    return false;
  }
  *value = parsed;
  *cursor = number;
  return true;
}

/* 解析C,前进,左移,旋转格式的蓝牙控制帧。 */
static bool Test_ParseCommand(const char *frame, int32_t *forward,
                              int32_t *left, int32_t *rotate)
{
  const char *cursor;

  if ((frame == NULL) || (frame[0] != 'C') || (frame[1] != ','))
  {
    return false;
  }

  cursor = &frame[2];
  if (!Test_ParseValue(&cursor, forward) || (*cursor++ != ',') ||
      !Test_ParseValue(&cursor, left) || (*cursor++ != ',') ||
      !Test_ParseValue(&cursor, rotate) || (*cursor != '\0'))
  {
    return false;
  }

  return true;
}

/* 完整解析位移帧后，按启动固定坐标累加闭环目标。 */
static void Test_ApplyPosition(const char *frame)
{
  const char *cursor = &frame[2];
  float dx;
  float dy;
  float angle;

  if (!Test_ParseDecimal(&cursor, &dx) || (*cursor++ != ',') ||
      !Test_ParseDecimal(&cursor, &dy) || (*cursor++ != ',') ||
      !Test_ParseDecimal(&cursor, &angle) || (*cursor != '\0'))
  {
    return;
  }
  (void)CarControl_Move(dx, dy, angle);
}

/* 分发速度、位移和模式帧；仅成功的开环速度帧刷新看门狗。 */
static void Test_ApplyCommand(const char *frame)
{
  int32_t forward;
  int32_t left;
  int32_t rotate;
  float vx;
  float vy;
  float wz;

  if ((frame == NULL) || (frame[0] == '\0') || (frame[1] != ','))
  {
    return;
  }
  if (frame[0] == 'M')
  {
    CarControl_Mode previous = CarControl_GetMode();
    CarControl_Mode mode;

    if (((frame[2] != '0') && (frame[2] != '1')) || (frame[3] != '\0'))
    {
      return;
    }
    mode = (frame[2] == '0') ? CAR_CONTROL_MODE_OPEN_LOOP :
                             CAR_CONTROL_MODE_CLOSED_LOOP;
    if ((CarControl_SetMode(mode) == HAL_OK) && (previous != mode))
    {
      test_command_received = false;
      test_stop_sent = false;
    }
    return;
  }
  if ((frame[0] == 'P') &&
      (CarControl_GetMode() == CAR_CONTROL_MODE_CLOSED_LOOP))
  {
    Test_ApplyPosition(frame);
    return;
  }
  if (CarControl_GetMode() != CAR_CONTROL_MODE_OPEN_LOOP)
  {
    return;
  }

  if (!Test_ParseCommand(frame, &forward, &left, &rotate))
  {
    return;
  }

  vx = ((float)forward / 1000.0F) * TEST_MAX_LINEAR_SPEED_MPS;
  vy = ((float)left / 1000.0F) * TEST_MAX_LINEAR_SPEED_MPS;
  wz = ((float)rotate / 1000.0F) * TEST_MAX_ANGULAR_SPEED_RAD_S;

  if (CarControl_SetVelocity(vx, vy, wz) == HAL_OK)
  {
    test_last_command_tick = HAL_GetTick();
    test_command_received = true;
    test_stop_sent = false;
  }
}

/* 将接收到的字节组装成一行控制帧。 */
static void Test_HandleByte(uint8_t byte)
{
  if (byte == '\n')
  {
    if (!test_frame_overflow)
    {
      if ((test_frame_length > 0U) &&
          (test_frame[test_frame_length - 1U] == '\r'))
      {
        test_frame_length--;
      }
      test_frame[test_frame_length] = '\0';
      Test_ApplyCommand(test_frame);
    }
    test_frame_length = 0U;
    test_frame_overflow = false;
    return;
  }

  if (((byte < ' ') && (byte != '\r')) || (byte > '~'))
  {
    test_frame_overflow = true;
  }
  if (test_frame_overflow)
  {
    return;
  }

  if (test_frame_length < TEST_FRAME_MAX_LENGTH)
  {
    test_frame[test_frame_length++] = (char)byte;
  }
  else
  {
    test_frame_overflow = true;
  }
}

/* 发送地址5电机的相对一圈位置指令。 */
static void Test_MoveButtonMotor(X42S_Direction direction)
{
  (void)X42S_SetPosition(TEST_BUTTON_MOTOR_ADDRESS, direction,
                          TEST_BUTTON_ACCELERATION_RPM_S,
                          TEST_BUTTON_DECELERATION_RPM_S,
                          TEST_BUTTON_SPEED_TENTH_RPM,
                          TEST_BUTTON_POSITION_TENTH_DEGREE,
                          X42S_MOVE_RELATIVE_NOW,
                          TEST_BUTTON_MAX_CURRENT_MA, false);
}

/* 初始化一个按键的去抖状态。 */
static void Test_InitButton(Test_ButtonState *button, GPIO_TypeDef *port,
                            uint16_t pin)
{
  GPIO_PinState sample;

  sample = HAL_GPIO_ReadPin(port, pin);
  button->port = port;
  button->pin = pin;
  button->stable_state = sample;
  button->last_sample_state = sample;
  button->last_change_tick = HAL_GetTick();
}

/* 检测按键稳定下降沿，按下时返回 true。 */
static bool Test_ButtonPressed(Test_ButtonState *button)
{
  GPIO_PinState sample;
  uint32_t now_tick;

  sample = HAL_GPIO_ReadPin(button->port, button->pin);
  now_tick = HAL_GetTick();
  if (sample != button->last_sample_state)
  {
    button->last_sample_state = sample;
    button->last_change_tick = now_tick;
    return false;
  }

  if ((now_tick - button->last_change_tick) < TEST_BUTTON_DEBOUNCE_MS)
  {
    return false;
  }

  if (button->stable_state == sample)
  {
    return false;
  }

  button->stable_state = sample;
  return (sample == GPIO_PIN_RESET);
}

/* 扫描两个按键并触发地址5电机的正反转调试动作。 */
static void Test_ProcessButtons(void)
{
  if (Test_ButtonPressed(&test_button_cw))
  {
    Test_MoveButtonMotor(X42S_DIR_CW);
  }

  if (Test_ButtonPressed(&test_button_ccw))
  {
    Test_MoveButtonMotor(X42S_DIR_CCW);
  }
}

/* 周期发送 GM6020 小电压，并在短时间后自动发送零电压。 */
static void Test_ProcessGM6020Voltage(void)
{
#if TEST_GM6020_VOLTAGE_TEST_ENABLE
  uint32_t now_tick;

  if (!test_gm6020_active)
  {
    return;
  }

  now_tick = HAL_GetTick();
  if (!test_gm6020_started)
  {
    if (GM6020_SetVoltage(TEST_GM6020_MOTOR_ADDRESS,
                          TEST_GM6020_VOLTAGE_COMMAND) == HAL_OK)
    {
      test_gm6020_started = true;
      test_gm6020_start_tick = now_tick;
      test_gm6020_last_send_tick = now_tick;
    }
    return;
  }

  if ((now_tick - test_gm6020_start_tick) >= TEST_GM6020_RUN_TIME_MS)
  {
    if (GM6020_SetVoltage(TEST_GM6020_MOTOR_ADDRESS, 0) == HAL_OK)
    {
      test_gm6020_active = false;
    }
    return;
  }

  if ((now_tick - test_gm6020_last_send_tick) >=
      TEST_GM6020_REFRESH_PERIOD_MS)
  {
    if (GM6020_SetVoltage(TEST_GM6020_MOTOR_ADDRESS,
                          TEST_GM6020_VOLTAGE_COMMAND) == HAL_OK)
    {
      test_gm6020_last_send_tick = now_tick;
    }
  }
#else
  /* 通过 TEST_GM6020_VOLTAGE_TEST_ENABLE 可关闭上电测试。 */
  return;
#endif
}

/* 初始化蓝牙控制帧缓存和失联保护状态。 */
void Test_Init(void)
{
  test_frame_length = 0U;
  test_frame_overflow = false;
  test_last_command_tick = HAL_GetTick();
  test_command_received = false;
  test_stop_sent = false;
#if TEST_GM6020_VOLTAGE_TEST_ENABLE
  test_gm6020_active = true;
  test_gm6020_started = false;
  test_gm6020_start_tick = 0U;
  test_gm6020_last_send_tick = 0U;
#endif
  Test_InitButton(&test_button_cw, GPIOD, GPIO_PIN_4);
  Test_InitButton(&test_button_ccw, GPIOD, GPIO_PIN_5);
}

/* 非阻塞处理蓝牙及调试，仅对开环速度指令执行失联停车。 */
void Test_Process(void)
{
  uint8_t byte;
  uint32_t now_tick;

  while (UART5_ReadByte(&byte) != 0U)
  {
    Test_HandleByte(byte);
  }

  Test_ProcessButtons();
  Test_ProcessGM6020Voltage();

  if (CarControl_GetMode() != CAR_CONTROL_MODE_OPEN_LOOP)
  {
    test_command_received = false;
    test_stop_sent = false;
    return;
  }

  if (!test_command_received || test_stop_sent)
  {
    return;
  }

  now_tick = HAL_GetTick();
  if ((now_tick - test_last_command_tick) >= TEST_CONTROL_TIMEOUT_MS)
  {
    if (CarControl_Stop() == HAL_OK)
    {
      test_stop_sent = true;
    }
  }
}
