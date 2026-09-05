#ifndef RGB_LED_H
#define RGB_LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include <stdint.h>

HAL_StatusTypeDef RGB_Init(void);
void RGB_SetColor(uint8_t red, uint8_t green, uint8_t blue);
void RGB_Off(void);

#ifdef __cplusplus
}
#endif

#endif
