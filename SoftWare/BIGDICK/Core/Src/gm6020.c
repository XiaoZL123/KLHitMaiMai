#include "gm6020.h"

#include <string.h>

#define GM6020_TX_TIMEOUT_MS 5U

static FDCAN_HandleTypeDef *gm6020_can;
static GM6020_Feedback gm6020_feedback[GM6020_MAX_ID + 1U];
static int16_t gm6020_voltage_low[4U];
static int16_t gm6020_voltage_high[3U];

/* 将经典 CAN 数据长度转换为 HAL 数据长度编码。 */
static uint32_t GM6020_LengthToDlc(uint8_t length)
{
  static const uint32_t dlc_table[9] = {
    FDCAN_DLC_BYTES_0, FDCAN_DLC_BYTES_1, FDCAN_DLC_BYTES_2,
    FDCAN_DLC_BYTES_3, FDCAN_DLC_BYTES_4, FDCAN_DLC_BYTES_5,
    FDCAN_DLC_BYTES_6, FDCAN_DLC_BYTES_7, FDCAN_DLC_BYTES_8
  };

  return (length <= 8U) ? dlc_table[length] : FDCAN_DLC_BYTES_0;
}

/* 将有符号 16 位数据按大端格式写入 CAN 数据区。 */
static void GM6020_WriteI16(uint8_t *data, int16_t value)
{
  uint16_t raw_value = (uint16_t)value;

  data[0] = (uint8_t)(raw_value >> 8U);
  data[1] = (uint8_t)raw_value;
}

/* 从大端 CAN 数据区读取有符号 16 位数据。 */
static int16_t GM6020_ReadI16(const uint8_t *data)
{
  uint16_t raw_value = ((uint16_t)data[0] << 8U) | data[1];

  return (int16_t)raw_value;
}

/* 发送一个 GM6020 标准 CAN 控制帧。 */
static HAL_StatusTypeDef GM6020_SendVoltageFrame(uint16_t identifier,
                                                 const int16_t *voltage,
                                                 uint8_t count)
{
  FDCAN_TxHeaderTypeDef header = {0};
  uint8_t data[8] = {0};
  uint8_t index;
  uint32_t start_tick;

  if ((gm6020_can == NULL) || (voltage == NULL) || (count > 4U))
  {
    return HAL_ERROR;
  }

  for (index = 0U; index < count; index++)
  {
    GM6020_WriteI16(&data[index * 2U], voltage[index]);
  }

  start_tick = HAL_GetTick();
  while (HAL_FDCAN_GetTxFifoFreeLevel(gm6020_can) == 0U)
  {
    if ((HAL_GetTick() - start_tick) >= GM6020_TX_TIMEOUT_MS)
    {
      return HAL_TIMEOUT;
    }
  }

  header.Identifier = identifier;
  header.IdType = FDCAN_STANDARD_ID;
  header.TxFrameType = FDCAN_DATA_FRAME;
  header.DataLength = GM6020_LengthToDlc(8U);
  header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  header.BitRateSwitch = FDCAN_BRS_OFF;
  header.FDFormat = FDCAN_CLASSIC_CAN;
  header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;

  return HAL_FDCAN_AddMessageToTxFifoQ(gm6020_can, &header, data);
}

/* 解析一帧 GM6020 标准反馈数据。 */
void GM6020_HandleFrame(const FDCAN_RxHeaderTypeDef *header,
                        const uint8_t *data, uint8_t length)
{
  uint8_t id;
  GM6020_Feedback *feedback;

  if ((header == NULL) || (data == NULL) ||
      (header->IdType != FDCAN_STANDARD_ID) || (length < 7U) ||
      (header->Identifier < (GM6020_FEEDBACK_ID_BASE + GM6020_MIN_ID)) ||
      (header->Identifier > (GM6020_FEEDBACK_ID_BASE + GM6020_MAX_ID)))
  {
    return;
  }

  id = (uint8_t)(header->Identifier - GM6020_FEEDBACK_ID_BASE);
  feedback = &gm6020_feedback[id];
  feedback->valid = true;
  feedback->id = id;
  feedback->angle_ecd = ((uint16_t)data[0] << 8U) | data[1];
  feedback->speed_rpm = GM6020_ReadI16(&data[2]);
  feedback->torque_current = GM6020_ReadI16(&data[4]);
  feedback->temperature = data[6];
  feedback->rx_count++;
  feedback->last_rx_tick = HAL_GetTick();
}

/* 绑定 GM6020 使用的 FDCAN 句柄并清零反馈状态。 */
HAL_StatusTypeDef GM6020_Init(FDCAN_HandleTypeDef *hfdcan)
{
  if (hfdcan == NULL)
  {
    return HAL_ERROR;
  }

  gm6020_can = hfdcan;
  memset(gm6020_feedback, 0, sizeof(gm6020_feedback));
  memset(gm6020_voltage_low, 0, sizeof(gm6020_voltage_low));
  memset(gm6020_voltage_high, 0, sizeof(gm6020_voltage_high));
  /* 标准反馈过滤器由 FDCAN1_Init 统一配置，此处只绑定句柄并清零状态。 */
  return HAL_OK;
}

/* 兼容旧调用方式，统一交由 FDCAN1_Process 读取和分发 FIFO。 */
void GM6020_Process(void)
{
  FDCAN1_Process();
}

/* 设置指定 GM6020 电机的电压并发送对应分组控制帧。 */
HAL_StatusTypeDef GM6020_SetVoltage(uint8_t id, int16_t voltage)
{
  int16_t *group;

  if ((id < GM6020_MIN_ID) || (id > GM6020_MAX_ID) ||
      (voltage < -GM6020_VOLTAGE_LIMIT) ||
      (voltage > GM6020_VOLTAGE_LIMIT))
  {
    return HAL_ERROR;
  }

  if (id <= 4U)
  {
    group = gm6020_voltage_low;
    group[id - 1U] = voltage;
    return GM6020_SendVoltageFrame(GM6020_VOLTAGE_ID_LOW, group, 4U);
  }

  group = gm6020_voltage_high;
  group[id - 5U] = voltage;
  return GM6020_SendVoltageFrame(GM6020_VOLTAGE_ID_HIGH, group, 3U);
}

/* 将两个分组的电压指令清零，令所有 GM6020 停止输出。 */
HAL_StatusTypeDef GM6020_Stop(void)
{
  HAL_StatusTypeDef status_low;
  HAL_StatusTypeDef status_high;

  memset(gm6020_voltage_low, 0, sizeof(gm6020_voltage_low));
  memset(gm6020_voltage_high, 0, sizeof(gm6020_voltage_high));
  status_low = GM6020_SendVoltageFrame(GM6020_VOLTAGE_ID_LOW,
                                       gm6020_voltage_low, 4U);
  status_high = GM6020_SendVoltageFrame(GM6020_VOLTAGE_ID_HIGH,
                                        gm6020_voltage_high, 3U);

  return (status_low != HAL_OK) ? status_low : status_high;
}

/* 获取指定地址最近一次 GM6020 反馈。 */
const GM6020_Feedback *GM6020_GetFeedback(uint8_t id)
{
  if ((id < GM6020_MIN_ID) || (id > GM6020_MAX_ID))
  {
    return NULL;
  }

  return &gm6020_feedback[id];
}
