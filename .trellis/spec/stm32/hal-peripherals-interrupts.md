# HAL、外设、中断与 DMA

## HAL 与初始化

- `main()` 的顺序是 `MPU_Config`、`HAL_Init`、`SystemClock_Config`，随后调用 `GPIO_Init`、`I2C1_Init`、`USART2_Init`、`OPS_Init`、`FDCAN1_Init`（`Core/Src/main.c:19-28`）。新增模块应在依赖初始化之后调用。
- 外设初始化使用 HAL 配置结构体和 `HAL_*_Init`，失败即 `Error_Handler()`；GPIO、时钟和复用配置放在对应 `HAL_<PERIPH>_MspInit` 或 `HAL_TIM_MspPostInit`，见 `Core/Src/i2c.c`、`tim.c`、`fdcan.c`。
- 已有低层模块也直接操作寄存器：`Core/Src/usart.c` 配置 USART2/DMA 寄存器，`Core/Src/ops.c` 配置 TIM6。新增寄存器代码必须保留时钟、标志清除、NVIC 优先级和启动顺序。
- UART5 专用于蓝牙调试：使用 PB12（RX）和 PB13（TX）的 `GPIO_AF14_UART5` 复用，配置为 9600 8N1；基础初始化通过 `UART5_Init()` 完成，短消息可使用阻塞式 `UART5_Send()` 发送，空指针直接返回；不与 USART2 的 OPS 接收 DMA 共用状态。

## 中断与回调

- IRQ 入口放在 `Core/Src/stm32h7xx_it.c` 或对应外设模块；`TIM6_DAC_IRQHandler` 只清 `TIM6->SR` 并调用 `OPS_TimerTick()`，不要在 IRQ 中解析整帧数据。
- `USART2_IRQHandler` 只清 IDLE 标志、记录 `DMA1_Stream0->NDTR` 计算长度并置 `usart2_rx_event`（`Core/Src/usart.c`）；主循环再由 `OPS_Process` 消费。
- HAL 回调/扩展点按 HAL 约定命名并放在外设模块中，如 `HAL_FDCAN_MspInit`、`HAL_I2C_MspInit`、`HAL_TIM_MspPostInit`。当前 `.ioc` 将 `USE_HAL_*_REGISTER_CALLBACKS` 设为 `0U`，不要无依据引入动态注册回调。
- HardFault、MemManage、BusFault、UsageFault 当前采用关停循环，保持 `Error_Handler` 同样的故障停机语义，除非任务明确要求诊断输出。

## FDCAN 标准帧与扩展帧共存

### 1. Scope / Trigger

当前 FDCAN1 同时连接 GM6020 和 X42S。GM6020 使用标准帧，X42S 使用扩展帧；接收过滤器和 FIFO 消费必须保持统一，否则一个驱动会取走另一个驱动的反馈帧。

### 2. Signatures

```c
void FDCAN1_Process(void);
void GM6020_HandleFrame(const FDCAN_RxHeaderTypeDef *header,
                        const uint8_t *data, uint8_t length);
void X42S_HandleFrame(const FDCAN_RxHeaderTypeDef *header,
                      const uint8_t *data, uint8_t length);
```

### 3. Contracts

- `FDCAN1_Init()` 预留一个标准过滤器和一个扩展过滤器，并只启动一次 FDCAN1。
- 标准过滤器接收 GM6020 反馈 `0x205～0x20B`，扩展过滤器接收 X42S 帧；两者都进入 FIFO0。
- `FDCAN1_Process()` 是 FIFO0 的唯一消费者：标准帧交给 `GM6020_HandleFrame()`，扩展帧交给 `X42S_HandleFrame()`。
- GM6020 反馈为 DLC 8：角度 `DATA[0:1]` 无符号大端，转速和转矩电流 `DATA[2:3]`、`DATA[4:5]` 为有符号大端，温度为 `DATA[6]`。

### 4. Validation & Error Matrix

- 空句柄或空数据：帧处理函数直接返回。
- 非标准 GM6020 ID、帧长度小于 7：丢弃，不更新反馈。
- GM6020 电压 ID 不在 `1～7` 或电压超出 `-25000～25000`：发送接口返回 `HAL_ERROR`。
- TX FIFO 超时：发送接口返回 `HAL_TIMEOUT`，不得继续等待。

### 5. Good/Base/Bad Cases

- Good：主循环只调用 `FDCAN1_Process()`，两个协议均能更新反馈。
- Base：暂时只接 GM6020，仍保留扩展过滤器和统一分发器。
- Bad：再次让 `X42S_Process()` 或其他模块直接轮询 FIFO0，导致 GM6020 帧被提前取走。

### 6. Tests Required

- Debug 构建无警告并生成 `build/Debug/BIGDICK.elf`。
- 总线上发送或接收 GM6020 `0x205` 帧，断言 `GM6020_GetFeedback(1)` 的角度、转速、电流和温度更新。
- 同时接收 X42S 扩展反馈，断言 X42S 反馈计数更新且 GM6020 数据不丢失。
- 调用 `GM6020_SetVoltage()` 验证标准帧 ID、DLC、字节序和范围错误返回。

### 7. Wrong vs Correct

错误做法：`X42S_Process()` 和 `GM6020_Process()` 各自调用 `HAL_FDCAN_GetRxMessage()`。

正确做法：只由 `FDCAN1_Process()` 读取 FIFO，再按 `header.IdType` 分发到两个协议解析器。

## OPS 位姿闭环与底盘周期控制

接口、坐标约定、完整位姿有效性、默认闭环、开环/闭环切换和蓝牙 P/M/C 合约以 [chassis-control.md](./chassis-control.md) 为准。

- `OPS_Process()` 返回 1 表示消费了 TIM6 5 ms 节拍；主循环先解析 OPS/FDCAN/蓝牙，再执行一次 `CarControl_Process()`。
- 中断不承担位置/角度浮点计算、蓝牙协议解析或 CAN 发送。
- 位置闭环须使用 `OPS_IsPoseFresh()`，不得以仅航向有效代替 X/Y 有效性。
- 位姿超时停车并保留零点和目标；开环速度 300 ms 看门狗不取消闭环位置目标。
- 每组四轮速度先缓存到 X42S，再统一同步启动；TX FIFO 满时等待上限仍为 5 ms，周期抖动需另行验证。

## DMA 与缓存

- USART2 RX 使用循环 DMA：`Core/Src/usart.c` 中 `DMA1_Stream0` 配置 `DMA_SxCR_MINC | DMA_SxCR_CIRC`，数据进入 `usart2_rx_buffer`，通过 `USART2_GetRxLength` 和环形位置读取。
- DMA 缓冲区声明为 `__attribute__((section(".dma_buffer"), aligned(32)))`；链接脚本 `STM32H743XX_FLASH.ld` 将 `.dma_buffer` 放入 `RAM_D2` 并按 32 字节对齐。改变缓冲区时同步检查这两处。
- 中断共享状态使用 `volatile`，例如 `usart2_rx_length`、`usart2_rx_event`、`ops_process_pending`；主循环清除事件标志并推进消费位置。
