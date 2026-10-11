# Research: GM6020 与 X42S 共用 FDCAN 接收架构审查

- Query: 审查当前 BIGDICK 的 FDCAN/X42S 架构，并给出 GM6020 标准 CAN 帧接收与电压协议集成建议。
- Scope: mixed（内部代码 + GM6020 用户手册）
- Date: 2026-10-04

## Findings

### 1. FDCAN 位时序和帧格式

- `SoftWare/BIGDICK/Core/Src/fdcan.c:9` 将 `FrameFormat` 设为 `FDCAN_FRAME_CLASSIC`，满足 GM6020 和现有 X42S 的经典 CAN 需求。
- `fdcan.c:18-21` 的时序为 `8 MHz / (1 * (1 + 5 + 2)) = 1 Mbps`；GM6020 手册要求 CAN 比特率 1 Mbps，匹配。
- `fdcan.c:27` 当前 `StdFiltersNbr = 0`，无法配置/接收 GM6020 的 11 位标准帧过滤器；`ExtFiltersNbr = 1` 仅为 X42S 扩展帧预留。
- `fdcan.c:29` FIFO0 只有 8 个元素，而 GM6020 反馈频率为 1 kHz；若主循环暂时不消费或同时存在多个反馈源，容易溢出，建议统一消费后按实际负载评估增大。

### 2. 当前 X42S 初始化/接收边界

- `SoftWare/BIGDICK/Core/Src/x42s_motor.c:176-206` 的 `X42S_Init()` 配置 1 个扩展帧掩码过滤器（`FilterID1=0`, `FilterID2=0`，匹配所有扩展 ID），随后调用 `HAL_FDCAN_Start()`。
- `x42s_motor.c:199-201` 将标准帧和未匹配扩展帧的全局策略设为 `FDCAN_REJECT`，因此新增标准过滤器后才会有 GM6020 标准帧进入 FIFO0。
- `x42s_motor.c:210-245` 的 `X42S_Process()` 循环读取 FIFO0 中所有消息；遇到非扩展帧直接 `continue` 丢弃。因此 GM6020 反馈即使通过过滤器，也会被 X42S 消费并丢弃。
- `X42S_Init()` 与潜在的 `GM6020_Init()` 不能各自调用 `HAL_FDCAN_Start()`：HAL 在 `HAL_FDCAN_STATE_BUSY` 时再次启动会返回 `HAL_ERROR_NOT_READY`（HAL 实现 `stm32h7xx_hal_fdcan.c:2613-2635`）。FDCAN 过滤器配置和启动必须由单一初始化流程完成，或由模块协同保证只启动一次。

### 3. 推荐的 FIFO 单一消费者方案

建议建立统一的接收分发层（可以放在 `fdcan.c`，或由一个独立 `fdcan_router.c` 实现）：

1. FDCAN 初始化时同时预留至少 1 个标准过滤器和 1 个扩展过滤器。
2. 标准过滤器使用范围匹配 `0x205..0x20B`，送入 FIFO0；扩展过滤器保留 X42S 的现有匹配规则，送入同一 FIFO0。
3. 只允许一个 `FDCAN1_Process()` 轮询 `HAL_FDCAN_GetRxFifoFillLevel()` / `HAL_FDCAN_GetRxMessage()`。
4. 每取出一帧，按 `header.IdType` 分发：
   - `FDCAN_EXTENDED_ID` → X42S 帧解析器；
   - `FDCAN_STANDARD_ID` 且 ID 在 `0x205..0x20B` → GM6020 解析器；其他标准帧丢弃。
5. 原有 `X42S_Process()` 不应再独立清空 FIFO。可将其改成“处理已取出的扩展帧”接口（例如 `X42S_HandleRxFrame(...)`），或让路由层调用一个公开的扩展帧解析入口。

这样可以保持 X42S 扩展帧和 GM6020 标准帧共存，避免模块间抢读/丢帧。

### 4. GM6020 用户手册协议核对

手册 `RoboMaster GM6020直流无刷电机使用说明20231013.pdf` 的 CAN 协议页给出：

- ID 拨码 1~7 有效；反馈标准帧 ID 为 `0x204 + ID`，即 ID 1 为 `0x205`，ID 7 为 `0x20B`。
- 电压控制使用标准数据帧、DLC 8：ID 1~4 共用控制 ID `0x1FF`，每个电机占两个字节；ID 5~7 共用 `0x2FF`，DATA[6:7] 保留为 Null。
- 电压给定为有符号范围 `-25000..25000`，应按 16 位大端发送。GM6020 ID 为 1 时写 `DATA[0]` 高字节和 `DATA[1]` 低字节，其余字节清零或保留同组其他电机命令。
- 反馈 DLC 8：`DATA[0:1]` 机械角度（0~8191，无符号大端）；`DATA[2:3]` 转速 rpm（有符号 16 位大端）；`DATA[4:5]` 实际转矩电流（有符号 16 位大端）；`DATA[6]` 温度；`DATA[7]` 保留。反馈频率 1 kHz。
- 电机从输出轴观察 CCW 为正；额定供电 24 V；CAN_H 为红线、CAN_L 为黑线；拨码第 4 位用于终端电阻。
- 电流协议（后续可选）为标准 ID `0x1FE`（ID1~4）或 `0x2FE`（ID5~7），给定范围 `-16384..16384` 对应 `-3..3 A`。本阶段电压控制不应误用电流帧。

### 5. GM6020 模块接口与构建清单

建议 `Core/Inc/gm6020.h` 说明对应 `Core/Src/gm6020.c` 的职责和公开函数：初始化/过滤器协同、接收单帧解析、读取反馈、发送电压指令、停止。协议层不应自行再次启动 FDCAN，除非明确成为唯一的 FDCAN 启动拥有者。

建议的最小公开接口方向：

```c
HAL_StatusTypeDef GM6020_Init(FDCAN_HandleTypeDef *hfdcan);
void GM6020_HandleRxFrame(const FDCAN_RxHeaderTypeDef *header,
                          const uint8_t *data);
HAL_StatusTypeDef GM6020_SetVoltage(uint8_t id, int16_t voltage);
HAL_StatusTypeDef GM6020_Stop(uint8_t id);
const GM6020_Feedback *GM6020_GetFeedback(uint8_t id);
```

如需兼容现有 X42S 初始化，可把 `GM6020_Init()` 限定为清零状态和注册标准过滤器，不调用 `HAL_FDCAN_Start()`；由统一 `FDCAN1_Init/Start` 或一个主模块在全部过滤器配置完成后启动一次。

必须将 `Core/Src/gm6020.c` 加到 `SoftWare/BIGDICK/CMakeLists.txt` 的 `target_sources`，头文件由 `Core/Inc` 统一包含。新增 `.h/.c` 中的职责、函数清单、单位和错误条件应同步。

### 6. 发送帧共存与控制节拍

- GM6020 电压帧应设 `header.IdType = FDCAN_STANDARD_ID`，`Identifier = 0x1FF/0x2FF`，`TxFrameType = FDCAN_DATA_FRAME`，`FDFormat = FDCAN_CLASSIC_CAN`，`DataLength = FDCAN_DLC_BYTES_8`。
- X42S 的 `X42S_SendFrame()` 固定扩展帧（`x42s_motor.c:82-89`），不会与标准帧 ID 冲突。
- GM6020 的单次电压指令不能只发送一帧后停止；协议实际使用“最新目标值”，应周期性（建议 1~5 ms）刷新同组 8 字节，停止/超时则持续发送零，避免保持旧输出。若只做通信验证，可先发送零帧/小幅值，并设置软件超时。
- 电压控制不是内部位置闭环；位置模式仍需外部角度反馈位置环/速度环，后续再考虑电流模式。

### 7. 实现快照复核（代码已出现后）

- 当前 `gm6020.c` 已按上述协议设置标准过滤器、解析反馈和发送 0x1FF/0x2FF 电压帧；`fdcan.c` 已把 `StdFiltersNbr` 改为 1；本地 Debug 构建已通过。
- 当前主函数顺序是 `GM6020_Init()` 配置标准过滤器，随后 `X42S_Init()` 配置扩展过滤器、全局过滤器并唯一启动 FDCAN，主循环只调用 `GM6020_Process()`。这个顺序能工作，但把 FDCAN 启动责任耦合到 X42S 模块；后续建议抽出统一启动函数。
- `X42S_Process()` 仍保留并会独立消费 FIFO0；虽然当前主函数不调用它，后续调用必须避免与 `GM6020_Process()` 并存，否则会发生帧竞争和丢弃。
- `GM6020_SetVoltage()` 当前为单次发送并保存分组最后值；电机控制场景仍应由上层周期刷新，通信超时/失联时发送零值。

## Related specs

- `.trellis/spec/stm32/index.md`
- `.trellis/spec/stm32/code-style.md`
- `.trellis/spec/stm32/hal-peripherals-interrupts.md`
- `.trellis/spec/project/repository-structure.md`
- `.trellis/tasks/09-22-car-run-module/prd.md`
- `.trellis/tasks/09-22-car-run-module/design.md`

## Caveats / Not Found

- 初始审查时工作区尚未发现 `gm6020.c/.h`；实现快照已补充在第 7 节。后续若继续修改过滤器索引、启动顺序、路由接口或 CMake 清单，应重新运行构建和本审查。
- 本文不修改业务代码；只记录风险与建议。若将来让 GM6020 模块自行配置过滤器，需把 `StdFiltersNbr` 的容量与过滤器索引作为显式约定，避免模块各自使用 `FilterIndex=0`。
- GM6020 手册没有明确写出反馈转速/转矩电流有符号字节序说明，但该类 DJI 协议及数据语义为有符号大端；实现应使用 `int16_t` 读取并在实机用正反转验证符号。
