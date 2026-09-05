#include "rgb_led.h"

#include "tim.h"

#define RGB_PWM_STEPS 1000U

/* 将 8 位亮度转换为定时器比较值。 */
static uint32_t RGB_ToCompare(uint8_t brightness)
{
  return ((uint32_t)brightness * RGB_PWM_STEPS + 127U) / 255U;
}

/* 启动三个颜色通道的 PWM 输出。 */
HAL_StatusTypeDef RGB_Init(void)
{
  if (HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_3) != HAL_OK)
  {
    return HAL_ERROR;
  }
  if (HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_4) != HAL_OK)
  {
    HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_3);
    return HAL_ERROR;
  }
  if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK)
  {
    HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_3);
    HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_4);
    return HAL_ERROR;
  }

  RGB_Off();
  return HAL_OK;
}

/* 设置 PC8、PC9、PA8 对应的红、绿、蓝亮度。 */
void RGB_SetColor(uint8_t red, uint8_t green, uint8_t blue)
{
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, RGB_ToCompare(red));
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_4, RGB_ToCompare(green));
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, RGB_ToCompare(blue));
}

/* 关闭 RGB 的全部颜色通道。 */
void RGB_Off(void)
{
  RGB_SetColor(0U, 0U, 0U);
}
