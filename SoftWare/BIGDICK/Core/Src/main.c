/* 这是2027年工创赛智能搬运车的工控机源码，主控采用STM32 H743VIT6，启用8MHz HSE

*/
#include "main.h"
#include "i2c.h"
#include "fdcan.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_fdcan.h"
#include "usart.h"
#include "ops.h"
#include "gpio.h"
#include "oled.h"
#include "x42s_motor.h"
#include "gm6020.h"
#include "car_run.h"
#include "car_control.h"
#include "test.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

void SystemClock_Config(void);
static void MPU_Config(void);

/* 将浮点数据格式化为带一位小数的 OLED 行文本。 */
static void OPS_FormatDisplayLine(char *line, size_t line_size,
                                  const char *label, float value,
                                  const char *unit)
{
  int32_t scaled_value;
  int32_t magnitude;

  if ((value != value) || (value > 999999.0F) || (value < -999999.0F))
  {
    (void)snprintf(line, line_size, "%s---%s", label, unit);
    return;
  }

  scaled_value = (int32_t)(value * 10.0F);
  magnitude = (scaled_value < 0) ? -scaled_value : scaled_value;
  (void)snprintf(line, line_size, "%s%s%ld.%01ld%s", label,
                 (scaled_value < 0) ? "-" : "",
                 (long)(magnitude / 10), (long)(magnitude % 10), unit);
}

/* 将最近解析到的 OPS 六项数据刷新到 OLED。 */
static void OPS_DisplayData(void)
{
  const OPS_Data_t *data = OPS_GetData();
  char line[24];
  const uint8_t row_y[] = {0U, 8U, 16U, 24U, 32U, 40U};
  const float values[] = {data->heading, data->pitch, data->roll,
                          data->x, data->y, data->z};
  const char *labels[] = {"H:", "P:", "R:", "X:", "Y:", "Wz:"};
  const char *units[] = {"deg", "deg", "deg", "mm", "mm", "dps"};
  uint8_t index;

  OLED_Fill(OLED_COLOR_BLACK);
  for (index = 0U; index < (uint8_t)(sizeof(row_y) / sizeof(row_y[0])); index++)
  {
    OPS_FormatDisplayLine(line, sizeof(line), labels[index], values[index],
                          units[index]);
    OLED_SetCursor(0U, row_y[index]);
    OLED_WriteString(line, OLED_COLOR_WHITE);
  }
  (void)OLED_UpdateScreen();
}


int main(void)
{
  uint32_t last_oled_update_tick = 0U;
  const CarRun_Config car_run_config = {
    .wheel_diameter_m = 0.1F,
    .wheel_track_m = 0.2F,
    .wheel_base_m = 0.2F,
    .max_wheel_speed_rpm = 3000.0F,
    .acceleration_rpm_s = 500U,
    .wheel_sign = {1, -1, 1, -1}
  };

  //外设初始化
  MPU_Config();
  HAL_Init();
  SystemClock_Config();
  GPIO_Init();
  I2C1_Init();
  USART2_Init();
  UART5_Init();
  OPS_Init();
  FDCAN1_Init();

  /* FDCAN1_Init 已统一配置标准/扩展过滤器并启动总线，再绑定协议模块。 */
  if (GM6020_Init(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }

  //蓝牙连接成功
  UART5_Send((const uint8_t *)"Connect Successfully\r\n",sizeof("Connect successfully\r\n") - 1U);

  //执行屏幕初始化
  OLED_Init(&hi2c1);

  //电机初始化并使能
  if (X42S_Init(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }
  X42S_Enable(1U, true, false);
  X42S_Enable(2U, true, false);
  X42S_Enable(3U, true, false);
  X42S_Enable(4U, true, false);
  X42S_Enable(5U, true, false);



  //底盘运动解算初始化
  if (CarRun_Init(&hfdcan1, &car_run_config) != HAL_OK)
  {
    Error_Handler();
  }
  if (CarControl_Init() != HAL_OK)
  {
    Error_Handler();
  }
  Test_Init();




  while (1)
  {
    //GM6020_SetVoltage(1, 6000);
    uint8_t ops_cycle_processed = OPS_Process();
    FDCAN1_Process();
    Test_Process();
    if (ops_cycle_processed != 0U)
    {
      (void)CarControl_Process();
    }

  }
  
}






void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 120;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

 

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  
  HAL_MPU_Disable();

  
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}


//错误中断
void Error_Handler(void)
{
  
  
  __disable_irq();
  while (1)
  {
  }
  
}
#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{
  
  
  
}
#endif
