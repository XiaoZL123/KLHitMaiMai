#include "x42s_motor.h"

#include <string.h>

#define X42S_MAX_PAYLOAD_LENGTH 32U
#define X42S_TX_TIMEOUT_MS 5U
#define X42S_SPEED_MAX_CURRENT_MA 1000U    //最大电机xx电流限制

static FDCAN_HandleTypeDef *x42s_can;
static X42S_Feedback x42s_feedback[256];

/* 将字节数转换为 FDCAN 数据长度编码。 */
static uint32_t X42S_LengthToDlc(uint8_t length)
{
  static const uint32_t dlc_table[9] = {
    FDCAN_DLC_BYTES_0, FDCAN_DLC_BYTES_1, FDCAN_DLC_BYTES_2,
    FDCAN_DLC_BYTES_3, FDCAN_DLC_BYTES_4, FDCAN_DLC_BYTES_5,
    FDCAN_DLC_BYTES_6, FDCAN_DLC_BYTES_7, FDCAN_DLC_BYTES_8
  };

  return dlc_table[length];
}

/* 将 FDCAN 数据长度编码转换为字节数。 */
static uint8_t X42S_DlcToLength(uint32_t dlc)
{
  return (uint8_t)dlc;
}

/* 将 16 位数值按大端格式写入缓冲区。 */
static void X42S_WriteU16(uint8_t *buffer, uint16_t value)
{
  buffer[0] = (uint8_t)(value >> 8U);
  buffer[1] = (uint8_t)value;
}

/* 将 32 位数值按大端格式写入缓冲区。 */
static void X42S_WriteU32(uint8_t *buffer, uint32_t value)
{
  buffer[0] = (uint8_t)(value >> 24U);
  buffer[1] = (uint8_t)(value >> 16U);
  buffer[2] = (uint8_t)(value >> 8U);
  buffer[3] = (uint8_t)value;
}

/* 从大端缓冲区读取 16 位数值。 */
static uint16_t X42S_ReadU16(const uint8_t *buffer)
{
  return ((uint16_t)buffer[0] << 8U) | buffer[1];
}

/* 从大端缓冲区读取 32 位数值。 */
static uint32_t X42S_ReadU32(const uint8_t *buffer)
{
  return ((uint32_t)buffer[0] << 24U) |
         ((uint32_t)buffer[1] << 16U) |
         ((uint32_t)buffer[2] << 8U) |
         buffer[3];
}

/* 发送一帧 X42S 扩展 CAN 数据。 */
static HAL_StatusTypeDef X42S_SendFrame(uint32_t identifier,
                                       const uint8_t *data, uint8_t length)
{
  FDCAN_TxHeaderTypeDef header = {0};
  uint32_t start_tick;

  if ((x42s_can == NULL) || (data == NULL) || (length > 8U))
  {
    return HAL_ERROR;
  }

  start_tick = HAL_GetTick();
  while (HAL_FDCAN_GetTxFifoFreeLevel(x42s_can) == 0U)
  {
    if ((HAL_GetTick() - start_tick) >= X42S_TX_TIMEOUT_MS)
    {
      return HAL_TIMEOUT;
    }
  }

  header.Identifier = identifier;
  header.IdType = FDCAN_EXTENDED_ID;
  header.TxFrameType = FDCAN_DATA_FRAME;
  header.DataLength = X42S_LengthToDlc(length);
  header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  header.BitRateSwitch = FDCAN_BRS_OFF;
  header.FDFormat = FDCAN_CLASSIC_CAN;
  header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;

  if (HAL_FDCAN_AddMessageToTxFifoQ(x42s_can, &header,
                                     (uint8_t *)data) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

/* 解析电机返回的短帧数据。 */
static void X42S_ParseFeedback(uint8_t address, uint8_t packet,
                              const uint8_t *data, uint8_t length)
{
  X42S_Feedback *feedback = &x42s_feedback[address];
  uint32_t value;

  if ((packet != 0U) || (data == NULL) || (length < 2U) ||
      (data[length - 1U] != X42S_CHECKSUM_FIXED))
  {
    return;
  }

  feedback->valid = true;
  feedback->address = address;
  feedback->function = data[0];
  feedback->rx_count++;

  if ((data[0] != 0x3AU) && (length == 3U) &&
      ((data[1] == X42S_REPLY_OK) ||
                         (data[1] == X42S_REPLY_REACHED) ||
                         (data[1] == X42S_REPLY_PARAM_ERROR) ||
                         (data[1] == X42S_REPLY_FORMAT_ERROR)))
  {
    feedback->reply = data[1];
    return;
  }

  feedback->reply = X42S_REPLY_NONE;
  switch (data[0])
  {
    case 0x24:
      if (length == 4U)
      {
        feedback->bus_voltage_mv = X42S_ReadU16(&data[1]);
      }
      break;

    case 0x27:
      if (length == 4U)
      {
        feedback->phase_current_ma = X42S_ReadU16(&data[1]);
      }
      break;

    case 0x35:
      if (length == 5U)
      {
        value = X42S_ReadU16(&data[2]);
        feedback->speed_tenth_rpm = (data[1] == 0U) ?
                                    (int16_t)value : -(int16_t)value;
      }
      break;

    case 0x36:
      if (length == 7U)
      {
        value = X42S_ReadU32(&data[2]);
        feedback->position_tenth_degree = (data[1] == 0U) ?
                                          (int32_t)value : -(int32_t)value;
      }
      break;

    case 0x3A:
      if (length == 3U)
      {
        feedback->motor_status = data[1];
      }
      break;

    default:
      break;
  }
}

/* 配置扩展帧过滤器并启动 X42S CAN 通信。 */
HAL_StatusTypeDef X42S_Init(FDCAN_HandleTypeDef *hfdcan)
{
  FDCAN_FilterTypeDef filter = {0};

  if (hfdcan == NULL)
  {
    return HAL_ERROR;
  }

  x42s_can = hfdcan;
  memset(x42s_feedback, 0, sizeof(x42s_feedback));

  filter.IdType = FDCAN_EXTENDED_ID;
  filter.FilterIndex = 0;
  filter.FilterType = FDCAN_FILTER_MASK;
  filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  filter.FilterID1 = 0U;
  filter.FilterID2 = 0U;
  if (HAL_FDCAN_ConfigFilter(x42s_can, &filter) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (HAL_FDCAN_ConfigGlobalFilter(x42s_can, FDCAN_REJECT, FDCAN_REJECT,
                                   FDCAN_REJECT_REMOTE,
                                   FDCAN_REJECT_REMOTE) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_FDCAN_Start(x42s_can);
}

/* 轮询接收 FIFO 并更新各电机反馈数据。 */
void X42S_Process(void)
{
  FDCAN_RxHeaderTypeDef header;
  uint8_t data[8];
  uint8_t length;
  uint8_t address;
  uint8_t packet;

  if (x42s_can == NULL)
  {
    return;
  }

  while (HAL_FDCAN_GetRxFifoFillLevel(x42s_can, FDCAN_RX_FIFO0) > 0U)
  {
    if (HAL_FDCAN_GetRxMessage(x42s_can, FDCAN_RX_FIFO0,
                               &header, data) != HAL_OK)
    {
      break;
    }

    if (header.IdType != FDCAN_EXTENDED_ID)
    {
      continue;
    }

    length = X42S_DlcToLength(header.DataLength);
    if (length > 8U)
    {
      continue;
    }
    address = (uint8_t)(header.Identifier >> 8U);
    packet = (uint8_t)header.Identifier;
    X42S_ParseFeedback(address, packet, data, length);
  }
}

/* 按 X42S CAN 协议自动拆分并发送命令。 */
HAL_StatusTypeDef X42S_SendRaw(uint8_t address, const uint8_t *payload,
                              uint8_t length)
{
  uint8_t frame[8];
  uint8_t packet = 0U;
  uint8_t offset = 0U;
  uint8_t chunk;
  HAL_StatusTypeDef status;

  if ((payload == NULL) || (length < 2U) ||
      (length > X42S_MAX_PAYLOAD_LENGTH))
  {
    return HAL_ERROR;
  }

  chunk = (length > 8U) ? 8U : length;
  memcpy(frame, payload, chunk);
  status = X42S_SendFrame(((uint32_t)address << 8U) | packet, frame, chunk);
  if (status != HAL_OK)
  {
    return status;
  }

  offset = chunk;
  while (offset < length)
  {
    packet++;
    frame[0] = payload[0];
    chunk = (uint8_t)(length - offset);
    if (chunk > 7U)
    {
      chunk = 7U;
    }
    memcpy(&frame[1], &payload[offset], chunk);
    status = X42S_SendFrame(((uint32_t)address << 8U) | packet,
                            frame, (uint8_t)(chunk + 1U));
    if (status != HAL_OK)
    {
      return status;
    }
    offset += chunk;
  }

  return HAL_OK;
}

/* 设置电机使能状态。 */
HAL_StatusTypeDef X42S_Enable(uint8_t address, bool enable, bool sync)
{
  uint8_t command[] = {0xF3, 0xAB, enable ? 0x01U : 0x00U,
                       sync ? 0x01U : 0x00U, X42S_CHECKSUM_FIXED};
  return X42S_SendRaw(address, command, sizeof(command));
}

/* 解除堵转、过热和过流保护。 */
HAL_StatusTypeDef X42S_ClearProtection(uint8_t address)
{
  const uint8_t command[] = {0x0E, 0x52, X42S_CHECKSUM_FIXED};
  return X42S_SendRaw(address, command, sizeof(command));
}

/* 将电机当前实时位置设置为零点。 */
HAL_StatusTypeDef X42S_SetCurrentPositionZero(uint8_t address)
{
  const uint8_t command[] = {0x0A, 0x6D, X42S_CHECKSUM_FIXED};
  return X42S_SendRaw(address, command, sizeof(command));
}

/* 电机速度指令 ：电机地址, 目标速度(正顺负逆), 加速度, 是否开启多机同步*/
/* 按实际RPM设置速度，支持一位小数，正数顺时针，负数逆时针。 */
HAL_StatusTypeDef X42S_SetSpeed(uint8_t address, float target_speed_rpm,
                               uint16_t acceleration_rpm_s, bool sync)
{
  uint8_t command[10];
  int32_t target_speed_tenth_rpm;
  uint16_t speed_tenth_rpm;

  if ((target_speed_rpm < -3000.0f) || (target_speed_rpm > 3000.0f))
  {
    return HAL_ERROR;
  }

  target_speed_tenth_rpm = (target_speed_rpm >= 0.0f) ?
                           (int32_t)(target_speed_rpm * 10.0f + 0.5f) :
                           (int32_t)(target_speed_rpm * 10.0f - 0.5f);
  speed_tenth_rpm = (target_speed_tenth_rpm < 0) ?
                    (uint16_t)(-target_speed_tenth_rpm) :
                    (uint16_t)target_speed_tenth_rpm;

  command[0] = 0xC6;
  command[1] = (target_speed_tenth_rpm < 0) ?
               (uint8_t)X42S_DIR_CCW : (uint8_t)X42S_DIR_CW;
  X42S_WriteU16(&command[2], acceleration_rpm_s);
  X42S_WriteU16(&command[4], speed_tenth_rpm);
  command[6] = sync ? 0x01U : 0x00U;
  X42S_WriteU16(&command[7], X42S_SPEED_MAX_CURRENT_MA);
  command[9] = X42S_CHECKSUM_FIXED;
  return X42S_SendRaw(address, command, sizeof(command));
}

/* 使用 X 固件梯形曲线限电流位置模式控制电机。 */
HAL_StatusTypeDef X42S_SetPosition(uint8_t address, X42S_Direction direction,
                                  uint16_t acceleration_rpm_s,
                                  uint16_t deceleration_rpm_s,
                                  uint16_t speed_tenth_rpm,
                                  uint32_t position_tenth_degree,
                                  X42S_MoveMode mode,
                                  uint16_t max_current_ma, bool sync)
{
  uint8_t command[17];

  if ((direction > X42S_DIR_CCW) || (mode > X42S_MOVE_RELATIVE_NOW) ||
      (speed_tenth_rpm > 30000U) || (max_current_ma > 5000U))
  {
    return HAL_ERROR;
  }

  command[0] = 0xCD;
  command[1] = (uint8_t)direction;
  X42S_WriteU16(&command[2], acceleration_rpm_s);
  X42S_WriteU16(&command[4], deceleration_rpm_s);
  X42S_WriteU16(&command[6], speed_tenth_rpm);
  X42S_WriteU32(&command[8], position_tenth_degree);
  command[12] = (uint8_t)mode;
  command[13] = sync ? 0x01U : 0x00U;
  X42S_WriteU16(&command[14], max_current_ma);
  command[16] = X42S_CHECKSUM_FIXED;
  return X42S_SendRaw(address, command, sizeof(command));
}

/* 立即停止指定电机。 */
HAL_StatusTypeDef X42S_Stop(uint8_t address, bool sync)
{
  uint8_t command[] = {0xFE, 0x98, sync ? 0x01U : 0x00U,
                       X42S_CHECKSUM_FIXED};
  return X42S_SendRaw(address, command, sizeof(command));
}

/* 使用广播地址触发已缓存的多电机命令。 */
HAL_StatusTypeDef X42S_SyncStart(void)
{
  const uint8_t command[] = {0xFF, 0x66, X42S_CHECKSUM_FIXED};
  return X42S_SendRaw(X42S_BROADCAST_ADDR, command, sizeof(command));
}

/* 请求读取电机实时转速。 */
HAL_StatusTypeDef X42S_ReadSpeed(uint8_t address)
{
  const uint8_t command[] = {0x35, X42S_CHECKSUM_FIXED};
  return X42S_SendRaw(address, command, sizeof(command));
}

/* 请求读取电机实时位置。 */
HAL_StatusTypeDef X42S_ReadPosition(uint8_t address)
{
  const uint8_t command[] = {0x36, X42S_CHECKSUM_FIXED};
  return X42S_SendRaw(address, command, sizeof(command));
}

/* 请求读取电机状态标志。 */
HAL_StatusTypeDef X42S_ReadStatus(uint8_t address)
{
  const uint8_t command[] = {0x3A, X42S_CHECKSUM_FIXED};
  return X42S_SendRaw(address, command, sizeof(command));
}

/* 请求读取电机总线电压。 */
HAL_StatusTypeDef X42S_ReadBusVoltage(uint8_t address)
{
  const uint8_t command[] = {0x24, X42S_CHECKSUM_FIXED};
  return X42S_SendRaw(address, command, sizeof(command));
}

/* 请求读取电机实际相电流。 */
HAL_StatusTypeDef X42S_ReadPhaseCurrent(uint8_t address)
{
  const uint8_t command[] = {0x27, X42S_CHECKSUM_FIXED};
  return X42S_SendRaw(address, command, sizeof(command));
}

/* 获取指定地址电机最近一次解析后的反馈。 */
const X42S_Feedback *X42S_GetFeedback(uint8_t address)
{
  return &x42s_feedback[address];
}
