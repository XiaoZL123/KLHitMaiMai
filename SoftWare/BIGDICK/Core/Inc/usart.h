#ifndef USART_H
#define USART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define USART2_RX_BUFFER_SIZE 256U

/* 初始化USART2，配置为115200 8N1。 */
void USART2_Init(void);
/* 通过USART2发送一段数据，并等待发送完成。 */
void USART2_Send(const uint8_t *data, uint16_t length);

/* 返回最近一次空闲线事件对应的接收长度。 */
uint16_t USART2_GetRxLength(void);
/* 返回DMA循环接收缓冲区首地址。 */
const uint8_t *USART2_GetRxBuffer(void);
/* 清除最近一次空闲线事件标志。 */
void USART2_ClearRxEvent(void);
/* 判断是否收到新的空闲线事件。 */
uint8_t USART2_IsRxEvent(void);

#ifdef __cplusplus
}
#endif

#endif
