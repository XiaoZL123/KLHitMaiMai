# 嵌入式数据流检查

当前 OPS 数据路径为：USART2 寄存器接收 -> `DMA1_Stream0` 循环缓冲 -> `USART2_IRQHandler` 更新长度/事件 -> `TIM6_DAC_IRQHandler` 置位周期标志 -> `OPS_Process` 消费新字节并解析 -> `main` 主循环继续处理（`Core/Src/usart.c`、`ops.c`、`stm32h7xx_it.c`、`main.c`）。

- IRQ 只记录硬件状态或置位标志；协议解析、CAN FIFO 读取和 OLED 刷新放在主循环/模块函数。
- 共享状态使用 `volatile`，环形位置按 `USART2_RX_BUFFER_SIZE` 取模。
- 修改帧长度、结束符或解析状态机时，同时检查 `OPS_Data_t`、原始帧缓存和消费者 `OPS_GetData/OPS_GetRawFrame`。
