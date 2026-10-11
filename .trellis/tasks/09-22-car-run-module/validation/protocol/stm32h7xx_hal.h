#ifndef PROTOCOL_HOST_HAL_H
#define PROTOCOL_HOST_HAL_H

#include <stdint.h>

typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
typedef struct { uint32_t unused; } GPIO_TypeDef;
typedef struct { uint32_t unused; } FDCAN_HandleTypeDef;
typedef struct { uint32_t unused; } FDCAN_RxHeaderTypeDef;
typedef struct {
  uint32_t PSC, ARR, EGR, SR, DIER, CR1;
} TIM_TypeDef;

extern GPIO_TypeDef protocol_host_gpiod;
extern TIM_TypeDef protocol_host_tim6;
#define GPIOD (&protocol_host_gpiod)
#define TIM6 (&protocol_host_tim6)
#define GPIO_PIN_4 (1U << 4U)
#define GPIO_PIN_5 (1U << 5U)
#define TIM_EGR_UG 1U
#define TIM_DIER_UIE 1U
#define TIM_CR1_CEN 1U
#define TIM6_DAC_IRQn 54
#define __HAL_RCC_TIM6_CLK_ENABLE() ((void)0)

uint32_t HAL_GetTick(void);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);
void HAL_NVIC_SetPriority(int irq, uint32_t priority, uint32_t subpriority);
void HAL_NVIC_EnableIRQ(int irq);

#endif
