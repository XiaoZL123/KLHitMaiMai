#include "ops.h"
#include "test.h"
#include "usart.h"
#include "x42s_motor.h"
#include "gm6020.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

GPIO_TypeDef protocol_host_gpiod;
TIM_TypeDef protocol_host_tim6;

static uint32_t host_tick;
static uint8_t dma_buffer[USART2_RX_BUFFER_SIZE];
static uint16_t dma_length;
static uint8_t dma_event;
static uint8_t uart_buffer[4096];
static size_t uart_length;
static size_t uart_read;
static CarControl_Mode host_mode;
static bool host_control_active;
static unsigned int move_calls;
static unsigned int speed_calls;
static unsigned int mode_calls;
static unsigned int stop_calls;
static unsigned int hold_calls;
static HAL_StatusTypeDef host_move_status;
static HAL_StatusTypeDef host_speed_status;
static HAL_StatusTypeDef host_mode_status;
static HAL_StatusTypeDef host_stop_status;
static float move_values[3];
static float speed_values[3];

/* 返回可控制的毫秒时钟，便于验证超时及回绕。 */
uint32_t HAL_GetTick(void)
{
  return host_tick;
}

/* 按键保持松开，避免测试主动触发调试电机。 */
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
  (void)port;
  (void)pin;
  return GPIO_PIN_SET;
}

/* 主机不配置实际中断优先级。 */
void HAL_NVIC_SetPriority(int irq, uint32_t priority, uint32_t subpriority)
{
  (void)irq;
  (void)priority;
  (void)subpriority;
}

/* 主机不启用实际硬件中断。 */
void HAL_NVIC_EnableIRQ(int irq)
{
  (void)irq;
}

/* 提供DMA循环缓冲区当前位置。 */
uint16_t USART2_GetRxLength(void)
{
  return dma_length;
}

/* 提供DMA循环缓冲区。 */
const uint8_t *USART2_GetRxBuffer(void)
{
  return dma_buffer;
}

/* 查询模拟空闲线事件。 */
uint8_t USART2_IsRxEvent(void)
{
  return dma_event;
}

/* 清除模拟空闲线事件。 */
void USART2_ClearRxEvent(void)
{
  dma_event = 0U;
}

/* 从模拟UART队列逐字节读取，沿用真实Test_Process入口。 */
uint8_t UART5_ReadByte(uint8_t *data)
{
  if (uart_read == uart_length)
  {
    return 0U;
  }
  *data = uart_buffer[uart_read++];
  return 1U;
}

/* 控制桩只记录位移调用，不复制控制算法或目标累加逻辑。 */
HAL_StatusTypeDef CarControl_Move(float dx, float dy, float angle)
{
  move_calls++;
  move_values[0] = dx;
  move_values[1] = dy;
  move_values[2] = angle;
  if (host_move_status == HAL_OK)
  {
    host_control_active = true;
  }
  return host_move_status;
}

/* 记录蓝牙转换后的车体速度。 */
HAL_StatusTypeDef CarControl_SetVelocity(float vx, float vy, float wz)
{
  speed_calls++;
  speed_values[0] = vx;
  speed_values[1] = vy;
  speed_values[2] = wz;
  if (host_speed_status == HAL_OK)
  {
    host_control_active = true;
  }
  return host_speed_status;
}

/* 模式桩记录请求，仅模拟成功状态，不实现底盘控制。 */
HAL_StatusTypeDef CarControl_SetMode(CarControl_Mode mode)
{
  mode_calls++;
  if (host_mode_status == HAL_OK)
  {
    host_mode = mode;
  }
  return host_mode_status;
}

/* 返回当前模拟模式，供真实蓝牙分发判断。 */
CarControl_Mode CarControl_GetMode(void)
{
  return host_mode;
}

/* 记录看门狗停车调用。 */
HAL_StatusTypeDef CarControl_Stop(void)
{
  stop_calls++;
  if (host_stop_status == HAL_OK)
  {
    host_control_active = false;
  }
  return host_stop_status;
}

/* 旧航向入口必须不再用于开环失联处理。 */
HAL_StatusTypeDef CarControl_HoldHeading(void)
{
  hold_calls++;
  return HAL_OK;
}

/* 保留GM6020配置和运行逻辑，以无硬件桩隔离CAN。 */
HAL_StatusTypeDef GM6020_SetVoltage(uint8_t id, int16_t voltage)
{
  (void)id;
  (void)voltage;
  return HAL_OK;
}

/* 保留按键电机接口，以无硬件桩隔离CAN。 */
HAL_StatusTypeDef X42S_SetPosition(uint8_t address, X42S_Direction direction,
                                  uint16_t acceleration, uint16_t deceleration,
                                  uint16_t speed, uint32_t position,
                                  X42S_MoveMode mode, uint16_t current, bool sync)
{
  (void)address;
  (void)direction;
  (void)acceleration;
  (void)deceleration;
  (void)speed;
  (void)position;
  (void)mode;
  (void)current;
  (void)sync;
  return HAL_OK;
}

/* 把原始字节写入模拟DMA缓冲，不直接调用协议私有函数。 */
static void Host_DMAFeed(const uint8_t *data, size_t length)
{
  for (size_t i = 0U; i < length; i++)
  {
    dma_buffer[dma_length] = data[i];
    dma_length = (uint16_t)((dma_length + 1U) % USART2_RX_BUFFER_SIZE);
  }
  dma_event = 1U;
}

/* 使用手册帧格式构造输入：CR/LF、六个小端float、LF/CR。 */
static void Host_OPSFrame(float heading, float x, float y, float z)
{
  const float payload[6] = { heading, NAN, NAN, x, y, z };
  uint8_t frame[28] = { 0x0D, 0x0A };

  memcpy(&frame[2], payload, sizeof(payload));
  frame[26] = 0x0A;
  frame[27] = 0x0D;
  Host_DMAFeed(frame, sizeof(frame));
  OPS_TimerTick();
  assert(OPS_Process() == 1U);
  assert(dma_event == 0U);
}

/* 复位DMA和OPS状态，避免用例之间互相污染。 */
static void Host_ResetOPS(uint32_t tick)
{
  host_tick = tick;
  dma_length = 0U;
  dma_event = 0U;
  memset(dma_buffer, 0, sizeof(dma_buffer));
  OPS_Init();
}

/* 验证真实OPS帧解析、最新完整帧有限性、50 ms边界及回绕。 */
static void Host_TestOPS(void)
{
  Host_ResetOPS(100U);
  assert(OPS_IsDataFresh() == 0U);
  assert(OPS_IsPoseFresh() == 0U);
  assert(OPS_Process() == 0U);
  assert(TIM6->PSC == 11999U && TIM6->ARR == 49U);

  Host_OPSFrame(30.0F, 12.5F, -45.25F, 2.0F);
  assert(OPS_IsDataFresh() == 1U && OPS_IsPoseFresh() == 1U);
  assert(OPS_GetData()->x == 12.5F && OPS_GetData()->y == -45.25F);
  assert(isnan(OPS_GetData()->pitch));
  assert(memcmp(OPS_GetRawFrame() + 12, &OPS_GetData()->x, sizeof(float)) == 0);
  host_tick = 150U;
  assert(OPS_IsPoseFresh() == 1U);
  host_tick = 151U;
  assert(OPS_IsDataFresh() == 0U && OPS_IsPoseFresh() == 0U);

  Host_OPSFrame(30.0F, 1.0F, 2.0F, 2.0F);
  Host_OPSFrame(31.0F, NAN, 2.0F, 3.0F);
  assert(OPS_IsPoseFresh() == 0U && OPS_IsDataFresh() == 1U);
  Host_OPSFrame(32.0F, 1.0F, INFINITY, 4.0F);
  assert(OPS_IsPoseFresh() == 0U);
  Host_OPSFrame(33.0F, 4.0F, 5.0F, 4.0F);
  assert(OPS_IsPoseFresh() == 1U);
  Host_OPSFrame(NAN, 99.0F, 99.0F, 4.0F);
  assert(OPS_IsPoseFresh() == 0U);
  assert(OPS_GetData()->heading == 33.0F && OPS_GetData()->x == 4.0F);
  Host_OPSFrame(34.0F, 6.0F, 7.0F, INFINITY);
  assert(OPS_IsPoseFresh() == 0U);
  Host_OPSFrame(35.0F, 8.0F, 9.0F, 0.0F);
  assert(OPS_IsPoseFresh() == 1U && OPS_GetData()->x == 8.0F);

  /* 反复收帧跨越DMA循环边界。 */
  for (unsigned int i = 0U; i < 80U; i++)
  {
    host_tick++;
    Host_OPSFrame(35.0F, (float)i, 9.0F, 0.0F);
    assert(OPS_IsPoseFresh() == 1U && OPS_GetData()->x == (float)i);
  }

  /* 截断帧不发布；补齐后通过原状态机恢复。 */
  {
    const float payload[6] = { 90.0F, 0.0F, 0.0F, 10.0F, 11.0F, 12.0F };
    uint8_t frame[28] = { 0x0D, 0x0A };
    memcpy(frame + 2, payload, sizeof(payload));
    frame[26] = 0x0A;
    frame[27] = 0x0D;
    Host_DMAFeed(frame, 10U);
    assert(OPS_Process() == 0U);
    OPS_TimerTick();
    assert(OPS_Process() == 1U && OPS_GetData()->heading == 35.0F);
    Host_DMAFeed(frame + 10, 18U);
    OPS_TimerTick();
    assert(OPS_Process() == 1U && OPS_GetData()->heading == 90.0F);
  }

  Host_ResetOPS(UINT32_MAX - 20U);
  Host_OPSFrame(0.0F, 1.0F, 2.0F, 0.0F);
  host_tick = 29U;
  assert(OPS_IsPoseFresh() == 1U);
  host_tick = 30U;
  assert(OPS_IsPoseFresh() == 0U);
}

/* 通过真实UART轮询入口喂入数据，可拆帧或包含NUL。 */
static void Host_SendBytes(const uint8_t *data, size_t length)
{
  assert(length <= sizeof(uart_buffer));
  memcpy(uart_buffer, data, length);
  uart_length = length;
  uart_read = 0U;
  Test_Process();
  assert(uart_read == uart_length);
}

/* 发送文本帧，不绕过真实字节缓存和解析流程。 */
static void Host_Send(const char *text)
{
  Host_SendBytes((const uint8_t *)text, strlen(text));
}

/* 复位控制spy和真实蓝牙模块。 */
static void Host_ResetProtocol(CarControl_Mode mode, uint32_t tick)
{
  host_tick = tick;
  host_mode = mode;
  host_control_active = false;
  move_calls = speed_calls = mode_calls = stop_calls = hold_calls = 0U;
  host_move_status = host_speed_status = host_mode_status = host_stop_status = HAL_OK;
  uart_length = uart_read = 0U;
  Test_Init();
}

/* 验证位移、模式、速度帧的小数/符号与模式约束。 */
static void Host_TestDispatch(void)
{
  Host_ResetProtocol(CAR_CONTROL_MODE_CLOSED_LOOP, 100U);
  Host_Send("P,+500.25,-30.5,+90.125\r\n");
  assert(move_calls == 1U && move_values[0] == 500.25F);
  assert(move_values[1] == -30.5F && move_values[2] == 90.125F);
  Host_Send("P,.5,-.25,1.\n");
  assert(move_calls == 2U && move_values[0] == 0.5F && move_values[2] == 1.0F);
  Host_Send("P,500,0,0\r\nP,500,0,0\r\n");
  assert(move_calls == 4U); /* 重复P确实分发两次。 */
  Host_Send("C,1000,0,0\r\n");
  assert(speed_calls == 0U);
  host_tick += 1000U;
  Test_Process();
  assert(stop_calls == 0U && host_control_active);

  Host_Send("M,0\r\n");
  assert(host_mode == CAR_CONTROL_MODE_OPEN_LOOP && mode_calls == 1U);
  Host_Send("P,500,0,90\r\n");
  assert(move_calls == 4U);
  Host_Send("C,+1000,-1000,1000\r\n");
  assert(speed_calls == 1U);
  assert(speed_values[0] == TEST_MAX_LINEAR_SPEED_MPS);
  assert(speed_values[1] == -TEST_MAX_LINEAR_SPEED_MPS);
  assert(speed_values[2] == TEST_MAX_ANGULAR_SPEED_RAD_S);

  Host_Send("M,1\r\n");
  assert(host_mode == CAR_CONTROL_MODE_CLOSED_LOOP && mode_calls == 2U);
  host_tick += 1000U;
  Test_Process();
  assert(stop_calls == 0U); /* 旧C看门狗不得取消位置模式。 */
  Host_Send("M,0\r\n");
  host_tick += 1000U;
  Test_Process();
  assert(stop_calls == 0U); /* 没有新C时不继承旧计时。 */
}

/* 验证坏帧不提交命令，超长/控制字符整行丢弃后能恢复。 */
static void Host_TestInvalidFrames(void)
{
  const char *bad_position[] = {
    "P,1,2", "P,1,2,3,4", "P,nan,0,0", "P,inf,0,0", "P,-inf,0,0",
    "P,1e2,0,0", "P,0x10,0,0", "P, 1,0,0", "P,1,0,0 ", "P,.,0,0",
    "P,+,0,0", "P,,0,0", "P,1,,0", "P,1,0,", "P,1.2.3,0,0",
    "P,9999999999999999999999999999999999999999,0,0",
    "P,0.00000000000000000000000000000000000000000000000000001,0,0",
    "P,1\r2,0,0", "P,1,2,3\r\r", "p,1,2,3"
  };
  const char *bad_mode[] = {
    "M", "M,", "M,2", "M,-1", "M,00", "M,1,0", "M, 1", "M,1.0"
  };
  const char *bad_speed[] = {
    "C,1001,0,0", "C,-1001,0,0", "C,1.5,0,0", "C,1,0,0,0",
    "C, 1,0,0", "C,nan,0,0", "C,1,0", "C,,0,0", "C,+,0,0",
    "C,999999999999999999999999999999999999999999999999999999,0,0"
  };
  Host_ResetProtocol(CAR_CONTROL_MODE_CLOSED_LOOP, 100U);
  for (size_t i = 0U; i < sizeof(bad_position) / sizeof(bad_position[0]); i++)
  {
    Host_Send(bad_position[i]);
    Host_Send("\n");
  }
  for (size_t i = 0U; i < sizeof(bad_mode) / sizeof(bad_mode[0]); i++)
  {
    Host_Send(bad_mode[i]);
    Host_Send("\n");
  }
  assert(move_calls == 0U && mode_calls == 0U && stop_calls == 0U);
  Host_Send("P,12.5,");
  assert(move_calls == 0U);
  Host_Send("-2.5,90\r\n");
  assert(move_calls == 1U);

  {
    uint8_t long_line[200];
    memset(long_line, '1', sizeof(long_line));
    long_line[0] = 'P';
    long_line[1] = ',';
    Host_SendBytes(long_line, sizeof(long_line));
    Host_Send("P,1,2,3\n"); /* 超长行剩余后缀仍必须丢弃。 */
    assert(move_calls == 1U);
    Host_Send("P,1,2,3\n");
    assert(move_calls == 2U);
  }
  {
    const uint8_t embedded_zero[] = "P,1,2,3\0,999\n";
    Host_SendBytes(embedded_zero, sizeof(embedded_zero) - 1U);
    assert(move_calls == 2U);
    Host_Send("P,1,2,3\n");
    assert(move_calls == 3U);
  }
  Host_Send("M,0\n");
  for (size_t i = 0U; i < sizeof(bad_speed) / sizeof(bad_speed[0]); i++)
  {
    Host_Send(bad_speed[i]);
    Host_Send("\n");
  }
  assert(speed_calls == 0U);
  Host_Send("\n\r\n");
  assert(speed_calls == 0U && mode_calls == 1U && move_calls == 3U);
}

/* 验证速度看门狗的边界、坏帧/同模式不续期与时间回绕。 */
static void Host_TestWatchdog(void)
{
  Host_ResetProtocol(CAR_CONTROL_MODE_OPEN_LOOP, 100U);
  Host_Send("C,300,0,0\n");
  host_tick = 399U;
  Host_Send("M,0\nC,1001,0,0\nP,1,2,3\n");
  assert(stop_calls == 0U);
  host_tick = 400U;
  Test_Process();
  assert(stop_calls == 1U && hold_calls == 0U && !host_control_active);
  host_tick = 1000U;
  Test_Process();
  assert(stop_calls == 1U);
  Host_Send("C,0,300,0\n");
  assert(speed_calls == 2U && host_control_active);
  host_tick = 1299U;
  Test_Process();
  assert(stop_calls == 1U);
  host_tick = 1300U;
  Test_Process();
  assert(stop_calls == 2U);

  Host_ResetProtocol(CAR_CONTROL_MODE_OPEN_LOOP, UINT32_MAX - 100U);
  Host_Send("C,300,0,0\n");
  host_tick = 198U;
  Test_Process();
  assert(stop_calls == 0U);
  host_tick = 199U;
  Test_Process();
  assert(stop_calls == 1U);

  Host_ResetProtocol(CAR_CONTROL_MODE_OPEN_LOOP, 100U);
  Host_Send("C,300,0,0\n");
  host_tick = 399U;
  host_speed_status = HAL_ERROR;
  Host_Send("C,500,0,0\n");
  host_tick = 400U;
  host_stop_status = HAL_TIMEOUT;
  Test_Process();
  assert(stop_calls == 1U);
  host_stop_status = HAL_OK;
  Test_Process();
  assert(stop_calls == 2U && !host_control_active);
  Test_Process();
  assert(stop_calls == 2U);

  Host_ResetProtocol(CAR_CONTROL_MODE_OPEN_LOOP, 100U);
  host_speed_status = HAL_ERROR;
  Host_Send("C,300,0,0\n");
  host_tick = 1000U;
  Test_Process();
  assert(stop_calls == 0U); /* 未成功提交的首条速度不启用看门狗。 */
}

/* 执行真实协议源码验证，不接触硬件。 */
int main(void)
{
  Host_TestOPS();
  Host_TestDispatch();
  Host_TestInvalidFrames();
  Host_TestWatchdog();
  puts("OPS/protocol host validation passed");
  return 0;
}
