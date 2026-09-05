#ifndef OLED_H
#define OLED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include <stdint.h>

#define OLED_WIDTH       128U
#define OLED_HEIGHT      64U
#define OLED_I2C_ADDRESS (0x3CU << 1)
#define OLED_TIMEOUT_MS  100U

typedef enum
{
  OLED_COLOR_BLACK = 0,
  OLED_COLOR_WHITE = 1
} OLED_Color;

HAL_StatusTypeDef OLED_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef OLED_UpdateScreen(void);
HAL_StatusTypeDef OLED_SetContrast(uint8_t contrast);
HAL_StatusTypeDef OLED_DisplayOn(void);
HAL_StatusTypeDef OLED_DisplayOff(void);
void OLED_Fill(OLED_Color color);
void OLED_DrawPixel(uint8_t x, uint8_t y, OLED_Color color);
void OLED_SetCursor(uint8_t x, uint8_t y);
char OLED_WriteChar(char ch, OLED_Color color);
void OLED_WriteString(const char *str, OLED_Color color);

#ifdef __cplusplus
}
#endif

#endif
