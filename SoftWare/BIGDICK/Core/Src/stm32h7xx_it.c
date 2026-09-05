

#include "main.h"
#include "stm32h7xx_it.h"
#include "ops.h"

void TIM6_DAC_IRQHandler(void)
{
  if ((TIM6->SR & TIM_SR_UIF) != 0U)
  {
    TIM6->SR = 0U;
    OPS_TimerTick();
  }
}

void NMI_Handler(void)
{
  

  
  
   while (1)
  {
  }
  
}

void HardFault_Handler(void)
{
  

  
  while (1)
  {
    
    
  }
}

void MemManage_Handler(void)
{
  

  
  while (1)
  {
    
    
  }
}

void BusFault_Handler(void)
{
  

  
  while (1)
  {
    
    
  }
}

void UsageFault_Handler(void)
{
  

  
  while (1)
  {
    
    
  }
}

void SVC_Handler(void)
{
  

  
  

  
}

void DebugMon_Handler(void)
{
  

  
  

  
}

void PendSV_Handler(void)
{
  

  
  

  
}

void SysTick_Handler(void)
{
  

  
  HAL_IncTick();
  

  
}
