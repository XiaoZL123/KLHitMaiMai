# HAL、外设、中断与 DMA

## HAL 与初始化

- `main()` 的顺序是 `MPU_Config`、`HAL_Init`、`SystemClock_Config`，随后调用 `GPIO_Init`、`I2C1_Init`、`USART2_Init`、`OPS_Init`、`FDCAN1_Init`（`Core/Src/main.c:19-28`）。新增模块应在依赖初始化之后调用。
- 外设初始化使用 HAL 配置结构体和 `HAL_*_Init`，失败即 `Error_Handler()`；GPIO、时钟和复用配置放在对应 `HAL_<PERIPH>_MspInit` 或 `HAL_TIM_MspPostInit`，见 `Core/Src/i2c.c`、`tim.c`、`fdcan.c`。
- 已有低层模块也直接操作寄存器：`Core/Src/usart.c` 配置 USART2/DMA 寄存器，`Core/Src/ops.c` 配置 TIM6。新增寄存器代码必须保留时钟、标志清除、NVIC 优先级和启动顺序。

## 中断与回调

- IRQ 入口放在 `Core/Src/stm32h7xx_it.c` 或对应外设模块；`TIM6_DAC_IRQHandler` 只清 `TIM6->SR` 并调用 `OPS_TimerTick()`，不要在 IRQ 中解析整帧数据。
- `USART2_IRQHandler` 只清 IDLE 标志、记录 `DMA1_Stream0->NDTR` 计算长度并置 `usart2_rx_event`（`Core/Src/usart.c`）；主循环再由 `OPS_Process` 消费。
- HAL 回调/扩展点按 HAL 约定命名并放在外设模块中，如 `HAL_FDCAN_MspInit`、`HAL_I2C_MspInit`、`HAL_TIM_MspPostInit`。当前 `.ioc` 将 `USE_HAL_*_REGISTER_CALLBACKS` 设为 `0U`，不要无依据引入动态注册回调。
- HardFault、MemManage、BusFault、UsageFault 当前采用关停循环，保持 `Error_Handler` 同样的故障停机语义，除非任务明确要求诊断输出。

## DMA 与缓存

- USART2 RX 使用循环 DMA：`Core/Src/usart.c` 中 `DMA1_Stream0` 配置 `DMA_SxCR_MINC | DMA_SxCR_CIRC`，数据进入 `usart2_rx_buffer`，通过 `USART2_GetRxLength` 和环形位置读取。
- DMA 缓冲区声明为 `__attribute__((section(".dma_buffer"), aligned(32)))`；链接脚本 `STM32H743XX_FLASH.ld` 将 `.dma_buffer` 放入 `RAM_D2` 并按 32 字节对齐。改变缓冲区时同步检查这两处。
- 中断共享状态使用 `volatile`，例如 `usart2_rx_length`、`usart2_rx_event`、`ops_process_pending`；主循环清除事件标志并推进消费位置。
