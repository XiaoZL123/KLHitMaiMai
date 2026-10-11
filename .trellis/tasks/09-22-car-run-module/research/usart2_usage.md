# Research: USART2 用途与数据链路

- Query: 检查工程中 USART2 的用途、连接设备、收发方向、串口参数、DMA/空闲线配置及数据消费路径。
- Scope: internal
- Date: 2026-09-25

## Findings

### 用途与连接设备

- USART2 是 OPS 全方位平面定位/姿态模块的数据接口。`main()` 先调用 `USART2_Init()`，随后调用 `OPS_Init()`（`SoftWare/BIGDICK/Core/Src/main.c:34-42`）；OPS 解析器通过 `usart.h` 获取 USART2 的 DMA 缓冲区和长度（`SoftWare/BIGDICK/Core/Src/ops.c:1-3,106-128`）。
- 实际引脚为 GPIOA 的 PA2/PA3，复用功能 AF7 USART2（`SoftWare/BIGDICK/Core/Src/usart.c:13-21`）。按 STM32 USART2 默认映射，PA2 为 TX、PA3 为 RX；外部 TTL/RS232 转换器或 OPS 接口应交叉连接：MCU PA2/TX→设备 RX，MCU PA3/RX←设备 TX，并共地。工程源码未记录外部转换芯片型号或连线标签，电气电平需以实际板卡为准。

### 串口参数与收发方式

- 初始化注释和头文件均声明 115200、8N1（`usart.c:8`, `usart.h:12`）。在当前系统时钟下，代码按 PCLK1=120 MHz 设置 `BRR=1042`（`usart.c:23-27`），即约 115200 baud；CR1 同时使能 TE、RE、IDLEIE、UE（`usart.c:41-44`），因此硬件收发均开启。
- 接收采用 DMA1 Stream0 循环模式，DMAMUX1 Channel0 请求号 43（USART2_RX），外设地址是 `USART2->RDR`，目标是 256 字节 `usart2_rx_buffer`（`usart.c:3-4,28-39`；`usart.h:10`）。缓冲区位于 `.dma_buffer` 段，链接到 D2 RAM 并按 32 字节对齐（`usart.c:3-4`; `STM32H743XX_FLASH.ld:176-181`）。DMA 只配置 RX，TX 不使用 DMA。
- 空闲线中断已开启，USART2 IRQ 优先级 5；IRQ 清 IDLE、用 `256-NDTR` 记录 DMA 已写入长度并置 `usart2_rx_event`（`usart.c:41-47,91-100`）。DMA Stream0 本身没有配置或启用 DMA 中断；接收事件依靠 USART2 IDLE IRQ。
- `USART2_Send()` 是轮询发送，逐字节等待 TXE/TXFNF，最后等待 TC（`usart.c:50-65`）。当前工程没有引用该发送接口，因此没有主动向 OPS 发送数据的路径。

### OPS 消费路径与当前状态

- `OPS_Process()` 每次被调用时读取 DMA 环形缓冲区从上次位置到当前 `NDTR` 位置的字节，并把字节送入 OPS 帧状态机（`ops.c:106-128`）。帧状态机按 `0x0D 0x0A` 起始、24 字节 payload、`0x0D 0x0A` 结束来解析 6 个 `float`：heading、pitch、roll、x、y、z（`ops.c:31-71`）。
- TIM6 被配置为约 5 ms 周期，TIM6 IRQ 仅调用 `OPS_TimerTick()` 置位待处理标志（`ops.c:73-92`; `stm32h7xx_it.c:7-14`）；设计意图是由主循环调用 `OPS_Process()` 完成解析。
- 当前 `main.c` 主循环仅调用 `X42S_Process()`（`main.c:59-62`），没有调用 `OPS_Process()`。因此虽然 USART2 RX DMA 和空闲中断可以接收并累计数据，但 OPS 帧不会被消费解析，`OPS_GetData()` 不会随新数据更新，除非后续恢复 `OPS_Process()` 调用。
- `USART2_IRQHandler()` 定义在 `usart.c` 而非 `stm32h7xx_it.c`；启动文件将 USART2 向量指向该符号（`startup_stm32h743xx.s:132,388-389`），因此链接层面可生效。`stm32h7xx_it.c` 中没有重复的 USART2 IRQ 实现。

## Caveats / Not Found

- `BIGDICK.ioc` 仅保留时钟/NVIC 基础条目，没有 USART2 的 CubeMX 外设配置；USART2、GPIO、DMA、IDLE 均由 `Core/Src/usart.c` 手工寄存器初始化。因此重新生成 CubeMX 代码时需防止覆盖该业务实现。
- 代码未配置 DMA Stream0 的 NVIC，也未处理 DMA 溢出/半传输事件；在 256 字节环形缓冲区被追上前，主循环必须及时推进 `ops_dma_position`，否则可能丢帧。
- `usart2_rx_length` 记录的是 DMA 当前写位置，但只在 IDLE 中断更新；若 OPS 连续无空闲间隔发送，主循环不会看到新位置，需确认设备帧间是否有足够空闲时间。
- 本次检查未验证实际硬件电平转换芯片和物理线缆。PA2/PA3 是 MCU 侧 TTL UART 引脚，若 OPS 输出为 RS-232 电平，必须经过兼容的电平转换器，不能直接相连。
