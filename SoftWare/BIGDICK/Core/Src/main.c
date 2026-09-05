/* 这是2027年工创赛智能搬运车的工控机源码，主控采用STM32 H743VIT6，启用8MHz HSE

*/
#include "main.h"
#include "i2c.h"
#include "fdcan.h"
#include "usart.h"
#include "ops.h"
#include "gpio.h"
#include "oled.h"
#include "x42s_motor.h"
#include <stdbool.h>

void SystemClock_Config(void);
static void MPU_Config(void);

int main(void)
{

  //外设初始化
  MPU_Config();
  HAL_Init();
  SystemClock_Config();
  GPIO_Init();
  I2C1_Init();
  USART2_Init();
  OPS_Init();
  FDCAN1_Init();

  //执行屏幕初始化
  OLED_Init(&hi2c1);
  OLED_WriteString("HelloWorld", OLED_COLOR_WHITE);
  OLED_UpdateScreen();

  //电机初始化并使能
  X42S_Init(&hfdcan1);
  X42S_Enable(1U, true, false);
  X42S_Enable(2U, true, false);
  X42S_Enable(3U, true, false);
  X42S_Enable(4U, true, false);


  //X42S_SetSpeed(1U, 100.0f, 0U, false);
  //X42S_SetSpeed(2U, 100.0f, 0U, false);
  //X42S_SetSpeed(3U, 100.0f, 0U, false);
  //X42S_SetSpeed(4U, 100.0f, 0U, false);
  

  while (1)
  {

    if (OPS_IsProcessPending()){OPS_Process();}  //ops数据获取，串口DMA+空闲读取，周期5ms

    X42S_Process();
    
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
