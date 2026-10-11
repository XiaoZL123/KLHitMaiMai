#include "ops.h"
#include "usart.h"
#include <math.h>
#include <string.h>

#define OPS_DATA_TIMEOUT_MS 50U

static OPS_Data_t ops_data;
static uint8_t ops_frame[24];
static uint8_t ops_raw_frame[24];
static uint8_t ops_state;
static uint8_t ops_index;
static uint16_t ops_dma_position;
static volatile uint8_t ops_process_pending;
static uint32_t ops_last_valid_tick;
static uint8_t ops_data_valid;
static uint8_t ops_pose_valid;



//将单字节送入OPS帧状态机。
static void OPS_ParseByte(uint8_t byte)
{
  switch (ops_state)
  {
    case 0U:
      if (byte == 0x0DU) ops_state = 1U;
      break;
    case 1U:
      if (byte == 0x0AU)
      {
        ops_index = 0U;
        ops_state = 2U;
      }
      else
      {
        ops_state = (byte == 0x0DU) ? 1U : 0U;
      }
      break;
    case 2U:
      ops_frame[ops_index++] = byte;
      if (ops_index >= sizeof(ops_frame)) ops_state = 3U;
      break;
    case 3U:
      ops_state = (byte == 0x0AU) ? 4U : 0U;
      break;
    default:
      if (byte == 0x0DU)
      {
        OPS_Data_t parsed_data;

        memcpy(ops_raw_frame, ops_frame, sizeof(ops_raw_frame));
        memcpy(&parsed_data.heading, &ops_frame[0], sizeof(float));
        memcpy(&parsed_data.pitch, &ops_frame[4], sizeof(float));
        memcpy(&parsed_data.roll, &ops_frame[8], sizeof(float));
        memcpy(&parsed_data.x, &ops_frame[12], sizeof(float));
        memcpy(&parsed_data.y, &ops_frame[16], sizeof(float));
        memcpy(&parsed_data.z, &ops_frame[20], sizeof(float));

        ops_pose_valid = (uint8_t)(isfinite(parsed_data.x) &&
                                  isfinite(parsed_data.y) &&
                                  isfinite(parsed_data.heading) &&
                                  isfinite(parsed_data.z));
        if (isfinite(parsed_data.heading) && isfinite(parsed_data.z))
        {
          ops_data = parsed_data;
          ops_last_valid_tick = HAL_GetTick();
          ops_data_valid = 1U;
        }
      }
      ops_state = 0U;
      break;
  }
}

/* 初始化OPS解析器状态。 */
void OPS_Init(void)
{
  memset(&ops_data, 0, sizeof(ops_data));
  ops_state = 0U;
  ops_index = 0U;
  ops_dma_position = 0U;
  ops_process_pending = 0U;
  ops_last_valid_tick = 0U;
  ops_data_valid = 0U;
  ops_pose_valid = 0U;

  /* TIM6时钟约120 MHz，配置为5 ms周期中断。 */
  __HAL_RCC_TIM6_CLK_ENABLE();
  TIM6->PSC = 11999U;
  TIM6->ARR = 49U;
  TIM6->EGR = TIM_EGR_UG;
  TIM6->SR = 0U;
  TIM6->DIER = TIM_DIER_UIE;
  TIM6->CR1 = TIM_CR1_CEN;
  HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 6U, 0U);
  HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
}

/* 判断是否到达下一次OPS处理周期。 */
uint8_t OPS_IsProcessPending(void)
{
  return ops_process_pending;
}

/* 置位OPS处理周期标志。 */
void OPS_TimerTick(void)
{
  ops_process_pending = 1U;
}

/* 处理DMA数据，并报告是否消费了一个TIM6处理周期。 */
uint8_t OPS_Process(void)
{
  const uint8_t *buffer = USART2_GetRxBuffer();
  uint16_t current = (uint16_t)(USART2_GetRxLength() % USART2_RX_BUFFER_SIZE);

  if (ops_process_pending == 0U)
  {
    return 0U;
  }
  ops_process_pending = 0U;

  while (ops_dma_position != current)
  {
    OPS_ParseByte(buffer[ops_dma_position]);
    ops_dma_position = (uint16_t)((ops_dma_position + 1U) % USART2_RX_BUFFER_SIZE);
  }

  if (USART2_IsRxEvent())
  {
    USART2_ClearRxEvent();
  }

  return 1U;
}

/* 获取最近一次解析成功的OPS数据。 */
const OPS_Data_t *OPS_GetData(void)
{
  return &ops_data;
}

/* 判断最近一次有限航向和角速度数据是否仍在有效期内。 */
uint8_t OPS_IsDataFresh(void)
{
  return (uint8_t)((ops_data_valid != 0U) &&
                   ((HAL_GetTick() - ops_last_valid_tick) <= OPS_DATA_TIMEOUT_MS));
}

/* 最新完整位姿必须全部有限，不能用旧位置掩盖新帧中的无效值。 */
uint8_t OPS_IsPoseFresh(void)
{
  return (uint8_t)((ops_pose_valid != 0U) && (OPS_IsDataFresh() != 0U));
}

/* 获取最近接收完整帧的24字节原始数据。 */
const uint8_t *OPS_GetRawFrame(void)
{
  return ops_raw_frame;
}
