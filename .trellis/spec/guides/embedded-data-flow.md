# 嵌入式数据流检查

当前 OPS 数据路径为：USART2 寄存器接收 -> `DMA1_Stream0` 循环缓冲 -> `USART2_IRQHandler` 更新长度/事件 -> `TIM6_DAC_IRQHandler` 置位周期标志 -> `OPS_Process` 消费新字节并解析 -> `main` 主循环继续处理（`Core/Src/usart.c`、`ops.c`、`stm32h7xx_it.c`、`main.c`）。

- IRQ 只记录硬件状态或置位标志；协议解析、CAN FIFO 读取和 OLED 刷新放在主循环/模块函数。
- 共享状态使用 `volatile`，环形位置按 `USART2_RX_BUFFER_SIZE` 取模。
- 修改帧长度、结束符或解析状态机时，同时检查 `OPS_Data_t`、原始帧缓存和消费者 `OPS_GetData/OPS_GetRawFrame`。

底盘运动控制遵循同一边界：麦轮浮点解算和 X42S 命令组织放在 `car_run.c` 的主循环/固定周期调用中，不放入 IRQ；四个轮子的同步速度命令全部发送成功后，再发送一次 `X42S_SyncStart()`，避免以四次函数调用的先后时间作为启动同步依据。

修改底盘/蓝牙流程时，对照 [位姿闭环规范](../stm32/chassis-control.md) 检查：P/M/C 是否进入统一控制层、固定启动坐标是否正确转换、完整位姿有效性是否覆盖 X/Y、开环速度看门狗是否与闭环位置目标隔离。失效恢复或模式切换不得隐式改变目标；无输出时避免重复发送停止帧。
