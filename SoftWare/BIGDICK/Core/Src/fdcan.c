#include "fdcan.h"

#include "gm6020.h"
#include "x42s_motor.h"

FDCAN_HandleTypeDef hfdcan1;

/* 初始化 FDCAN1，使用 8MHz HSE 生成 1Mbit/s 经典 CAN。 */
void FDCAN1_Init(void)
{
  FDCAN_FilterTypeDef std_filter = {0};
  FDCAN_FilterTypeDef ext_filter = {0};

  hfdcan1.Instance = FDCAN1;
  hfdcan1.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
#if FDCAN1_USE_INTERNAL_LOOPBACK
  hfdcan1.Init.Mode = FDCAN_MODE_INTERNAL_LOOPBACK;
#else
  hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
#endif
  hfdcan1.Init.AutoRetransmission = ENABLE;
  hfdcan1.Init.TransmitPause = DISABLE;
  hfdcan1.Init.ProtocolException = DISABLE;
  hfdcan1.Init.NominalPrescaler = 1;
  hfdcan1.Init.NominalSyncJumpWidth = 2;
  hfdcan1.Init.NominalTimeSeg1 = 5;
  hfdcan1.Init.NominalTimeSeg2 = 2;
  hfdcan1.Init.DataPrescaler = 1;
  hfdcan1.Init.DataSyncJumpWidth = 2;
  hfdcan1.Init.DataTimeSeg1 = 13;
  hfdcan1.Init.DataTimeSeg2 = 2;
  hfdcan1.Init.MessageRAMOffset = 0;
  /* 同时保留一个标准帧过滤器（GM6020）和一个扩展帧过滤器（X42S）。 */
  hfdcan1.Init.StdFiltersNbr = 1;
  hfdcan1.Init.ExtFiltersNbr = 1;
  hfdcan1.Init.RxFifo0ElmtsNbr = 8;
  hfdcan1.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.RxFifo1ElmtsNbr = 0;
  hfdcan1.Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.RxBuffersNbr = 0;
  hfdcan1.Init.RxBufferSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.TxEventsNbr = 0;
  hfdcan1.Init.TxBuffersNbr = 0;
  hfdcan1.Init.TxFifoQueueElmtsNbr = 8;
  hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  hfdcan1.Init.TxElmtSize = FDCAN_DATA_BYTES_8;

  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }

  /* 接收 GM6020 反馈标准帧 0x205～0x20B。 */
  std_filter.IdType = FDCAN_STANDARD_ID;
  std_filter.FilterIndex = 0U;
  std_filter.FilterType = FDCAN_FILTER_RANGE;
  std_filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  std_filter.FilterID1 = GM6020_FEEDBACK_ID_BASE + GM6020_MIN_ID;
  std_filter.FilterID2 = GM6020_FEEDBACK_ID_BASE + GM6020_MAX_ID;
  if (HAL_FDCAN_ConfigFilter(&hfdcan1, &std_filter) != HAL_OK)
  {
    Error_Handler();
  }

  /* 接收 X42S 扩展帧，扩展过滤器掩码为 0，保留全部扩展地址。 */
  ext_filter.IdType = FDCAN_EXTENDED_ID;
  ext_filter.FilterIndex = 0U;
  ext_filter.FilterType = FDCAN_FILTER_MASK;
  ext_filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  ext_filter.FilterID1 = 0U;
  ext_filter.FilterID2 = 0U;
  if (HAL_FDCAN_ConfigFilter(&hfdcan1, &ext_filter) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_REJECT, FDCAN_REJECT,
                                   FDCAN_REJECT_REMOTE,
                                   FDCAN_REJECT_REMOTE) != HAL_OK)
  {
    Error_Handler();
  }

  /* 所有标准/扩展过滤器配置完成后只启动一次 FDCAN1。 */
  if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* 将 HAL 数据长度编码转换为经典 CAN 字节数。 */
static uint8_t FDCAN1_DlcToLength(uint32_t dlc)
{
  return (dlc <= FDCAN_DLC_BYTES_8) ? (uint8_t)dlc : 0U;
}

/* 唯一读取 RX FIFO0，并按标准帧/扩展帧分发协议数据。 */
void FDCAN1_Process(void)
{
  FDCAN_RxHeaderTypeDef header;
  uint8_t data[8];
  uint8_t length;

  while (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) > 0U)
  {
    if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0,
                               &header, data) != HAL_OK)
    {
      break;
    }

    length = FDCAN1_DlcToLength(header.DataLength);
    if (header.IdType == FDCAN_STANDARD_ID)
    {
      GM6020_HandleFrame(&header, data, length);
    }
    else if (header.IdType == FDCAN_EXTENDED_ID)
    {
      X42S_HandleFrame(&header, data, length);
    }
  }
}

/* 配置 FDCAN1 的时钟和 PD0、PD1 复用引脚。 */
void HAL_FDCAN_MspInit(FDCAN_HandleTypeDef *fdcanHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  if (fdcanHandle->Instance != FDCAN1)
  {
    return;
  }

  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
  PeriphClkInitStruct.FdcanClockSelection = RCC_FDCANCLKSOURCE_HSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_FDCAN_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF9_FDCAN1;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
}
