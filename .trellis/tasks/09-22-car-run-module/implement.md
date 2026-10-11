# car_run 实现清单

原清单保留已实施背景；本轮以末尾“2026-10-11：OPS 位置闭环实施计划”为执行范围。

1. 在 `Core/Inc/car_run.h` 定义模块说明、轮位配置结构、初始化/速度/停止公开接口和参数单位。
2. 在 `Core/Src/car_run.c` 实现配置校验、麦轮四轮解算、统一轮速限幅、RPM 换算和方向符号修正。
3. 在 `CarRun_SetVelocity` 中按轮位地址 1、2、4、3 发送四轮同步速度命令，并应用符号 `+1、-1、+1、-1`，全部成功后调用 `X42S_SyncStart()`。
4. 实现零速度和错误路径，确保未初始化、非法浮点、非法几何参数不会发送无效运动命令。
5. 将 `Core/Src/car_run.c` 加入 `SoftWare/BIGDICK/CMakeLists.txt`，并在 `main.c` 完成底盘模块初始化；主循环仅保留 `X42S_Process()`，不自动发送运动指令，实际运动由上层控制逻辑按需调用 `CarRun_SetVelocity()`。
6. 检查 `.h/.c` 的模块说明和公开函数注释是否同步。
7. 执行 `cmake --build build/Debug --clean-first --parallel 4`，确认无编译错误和新增警告。
8. 进行静态检查：确认地址映射、单位、轮速缩放、同步广播和错误返回路径与 PRD/design 一致。

## 风险点

- 麦轮滚子方向与电机正转符号已由用户确认，首版仍把方向修正集中为配置项，便于后续实车标定。
- 当前 X42S 发送函数可能在 TX FIFO 满时等待；首版保留该行为并记录为后续发送队列优化项。

## OPS 航向保持闭环实施清单

1. 扩展 `ops.h/.c`：为 `OPS_Process()` 增加“本次是否消费 5 ms 周期”的返回值；完整帧只在 `heading`、`z` 均为有限数时发布，并记录有效帧时间；新增 `OPS_IsDataFresh()`，阈值 `50 ms`。保持 DMA/IDLE/TIM6 中断职责不变。
2. 新建 `Core/Inc/car_control.h` 与 `Core/Src/car_control.c`：保存期望车体速度、航向目标和控制状态；公开初始化、速度更新、失联后航向保持、停止和周期处理接口；实现航向角 wrap、OPS 超时恢复捕获、PD（`Kp=2.0`、`Kd=0.2`、`1 deg` 死区、最大修正 `0.3 rad/s`）及状态错误处理。
3. 修改 `test.c`：蓝牙解析只更新 `CarControl_SetVelocity()` 目标；300 ms 失联逻辑改为调用 `CarControl_HoldHeading()`，保持 GM6020/地址 5 测试逻辑不变。
4. 修改 `main.c`：在 `CarRun_Init()` 后初始化控制层；每轮继续轮询原有处理函数，在 `OPS_Process()` 消费定时器节拍后处理 FDCAN 和蓝牙输入，再且仅在该周期调用一次 `CarControl_Process()`。
5. 将 `car_control.c` 加入 `SoftWare/BIGDICK/CMakeLists.txt`，同步更新新增 `.h/.c` 职责和公开函数注释。
6. 检查所有 `CarRun_SetVelocity` 调用点，确保遥控闭环路径不再绕开控制层；独立调试/其他底盘调用者保持显式可识别，不意外被闭环劫持。
7. 从 `SoftWare/BIGDICK` 执行 Debug CMake 构建；核对无蓝牙命令时初始化航向保持、角度跨界、主动旋转旁路、松开旋转重捕获、OPS 超时恢复、蓝牙失联停车后保持及显式停止路径。仅静态/构建验证，不烧录、不驱动车辆。

## 变更面

### 2026-10-08：主循环启用 OPS 显示

- 按用户追加要求，复用 `main.c` 的 `OPS_DisplayData()`，在主循环完成控制处理后每 200 ms 刷新一次 OLED，显示 H/P/R/X/Y/Wz。
- 不新增延时或中断处理，保留现有阻塞式 OLED 驱动；刷新仍可能使主循环控制周期延后，本次不改为 DMA 驱动。
- 验证：Debug 构建与调用顺序检查；不自动烧录。

- 预计改动：`Core/Inc/ops.h`、`Core/Src/ops.c`、`Core/Inc/test.h`（仅在公开调用说明需要同步时）、`Core/Src/test.c`、`Core/Src/main.c`、新增 `Core/Inc/car_control.h` / `Core/Src/car_control.c`、`CMakeLists.txt`、本任务设计记录。
- 明确不改：USART2 DMA/IDLE 配置、TIM6 5 ms 参数、中断内执行内容、FDCAN/X42S 协议、电机地址、OLED 刷新及蓝牙协议格式。

## 2026-10-11：OPS 位置闭环实施计划

1. 完成最终 PRD/设计评审，获得最新方案实施批准后编写固件；保留既有工作区和用户调参。
2. 修改 `Core/Inc/ops.h` 与 `Core/Src/ops.c`，增加 OPS_IsPoseFresh，检查最新完整帧 X/Y/heading/z 及 50 ms 新鲜度，保留航向查询及原始帧读取。
3. 修改 `Core/Inc/car_control.h` 与 `Core/Src/car_control.c`，加入模式枚举、Move/SetMode/GetMode，保存零点和累计目标，实现安装转换、固定坐标转车体速度、角度展开、位置 P/角度 PD、速度限幅和一次停止路径。
4. 验证默认闭环无命令保持零点、途中累加、OPS 超时停车保留目标、开环旁路、模式往返保留目标、显式 Stop 和 CAN 错误停机。
5. 修改 `Core/Inc/test.h` 与 `Core/Src/test.c`，加入 P/M 分发及浮点解析，C 仅开环有效；300 ms 速度看门狗不影响位置目标，保留按键和 GM6020 原逻辑。
6. 同步六个业务文件的职责、公开函数、单位、模式限制、协议与中文注释；不改 main 自动测试、CAN、DMA、ISR、OLED。
7. 对真实控制与解析源码做主机模拟：目标累加、跨 ±180/整圈、旋转后固定坐标运动、合速度限幅、无效/超时帧、模式切换、C/P/M 错误帧及看门狗。用外部桩替代 HAL/OPS/CAN，不往固件加调试代码，不复制算法作为断言依据。
8. 执行 `cmake --build build/Debug --parallel 4` 和相关 `git diff --check`，review 按新 PRD 全范围核对，必要修复后重检。
9. 用 trellis-update-spec 记录位移累加、模式切换、有效性、单位/限幅和蓝牙帧；报告编译/模拟结果及未实车验证，不自动提交或烧录。

### 验收映射与上下文

- PRD R1～R5：控制实现与真实源码主机模拟；R6：蓝牙解析/计时模拟。
- 实施限定六个已有业务 `.h/.c`；必要模拟位于任务研究/验证目录，不加入固件构建。
- 用户已选择 A：启动固定坐标内累加目标；原仅航向保持、松开旋转/OPS 恢复重捕获与新要求冲突时以当前 PRD 为准。

### 本轮实施与验证记录

- 已实现六个业务文件的控制、OPS 完整位姿有效性及 P/M/C 蓝牙接口；主循环、CAN、DMA、TIM6、OLED 和手机 App 未作本轮修改。
- `validation/control/run.ps1`：直接编译真实 `car_control.c`，默认配置 11 组控制场景及非零安装偏角/中心偏移/反向航向配置通过；硬件依赖以外部桩替代。
- `validation/protocol/README.md`：直接编译真实 `ops.c` 和 `test.c`，帧解析、最新无效位姿、50 ms 边界、时间回绕、P/M/C 分发及 300 ms 看门狗隔离通过；控制接口仅记录调用，算法验证由控制模拟覆盖。
- Debug 构建通过，生成 `SoftWare/BIGDICK/build/Debug/BIGDICK.elf`：FLASH 使用 50288 B，DTCMRAM 使用 10312 B。仅有 `main.c` 原有 OLED 函数/变量未使用两条警告，新增业务模块无警告。
- 最终六文件独立审查通过，未发现偏离新合约的业务代码问题；补充 Stop 期间整圈跟踪及非零初始角与安装参数同时存在的联合验证，重跑控制模拟通过。仅增加主机验证用例，固件未再修改。
- Trellis 规范、接口说明、验收与实施记录已同步。实车安装方向、X/Y 测量、航向符号与增益尚未验证。不自动烧录、提交或归档活动父任务。
