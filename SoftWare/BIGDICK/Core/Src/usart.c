#include "usart.h"

static uint8_t usart2_rx_buffer[USART2_RX_BUFFER_SIZE]
    __attribute__((section(".dma_buffer"), aligned(32)));
static volatile uint16_t usart2_rx_length;
static volatile uint8_t usart2_rx_event;

/* 初始化USART2，配置为115200 8N1。 */
void USART2_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_USART2_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PCLK1为60 MHz时，过采样16对应115200波特率的BRR取521。 */
  USART2->CR1 = 0U;
  USART2->CR2 = 0U;
  USART2->CR3 = 0U;
  USART2->BRR = 521U;
  __HAL_RCC_DMA1_CLK_ENABLE();

  DMA1_Stream0->CR &= ~DMA_SxCR_EN;
  DMA1->LIFCR = DMA_LIFCR_CFEIF0 | DMA_LIFCR_CDMEIF0 |
                DMA_LIFCR_CTEIF0 | DMA_LIFCR_CHTIF0 | DMA_LIFCR_CTCIF0;
  DMAMUX1_Channel0->CCR = 43U; /* USART2_RX请求 */
  DMA1_Stream0->PAR = (uint32_t)&USART2->RDR;
  DMA1_Stream0->M0AR = (uint32_t)usart2_rx_buffer;
  DMA1_Stream0->NDTR = USART2_RX_BUFFER_SIZE;
  DMA1_Stream0->CR = DMA_SxCR_MINC | DMA_SxCR_CIRC | DMA_SxCR_PL_1;
  DMA1_Stream0->FCR = 0U;
  DMA1_Stream0->CR |= DMA_SxCR_EN;

  USART2->CR3 = USART_CR3_DMAR;
  USART2->ICR = USART_ICR_IDLECF;
  USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_IDLEIE |
                USART_CR1_UE;

  HAL_NVIC_SetPriority(USART2_IRQn, 5U, 0U);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
}

/* 返回最近一次空闲线事件对应的接收长度。 */
uint16_t USART2_GetRxLength(void)
{
  return usart2_rx_length;
}

/* 返回DMA循环接收缓冲区首地址。 */
const uint8_t *USART2_GetRxBuffer(void)
{
  return usart2_rx_buffer;
}

/* 清除最近一次空闲线事件标志。 */
void USART2_ClearRxEvent(void)
{
  usart2_rx_event = 0U;
}

/* 判断是否收到新的空闲线事件。 */
uint8_t USART2_IsRxEvent(void)
{
  return usart2_rx_event;
}

/* 处理USART2空闲线事件并记录当前DMA接收长度。 */
void USART2_IRQHandler(void)
{
  if ((USART2->ISR & USART_ISR_IDLE) != 0U)
  {
    USART2->ICR = USART_ICR_IDLECF;
    usart2_rx_length = (uint16_t)(USART2_RX_BUFFER_SIZE - DMA1_Stream0->NDTR);
    usart2_rx_event = 1U;
  }
}
