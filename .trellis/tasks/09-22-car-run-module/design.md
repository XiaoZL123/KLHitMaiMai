# car_run 底盘控制模块设计

2026-10-11 更新：第 1～7 节保留原实现背景；第 8 节为本轮设计，替代第 7 节的手动旋转旁路、失联/OPS 恢复重捕获及默认仅航向保持行为。

## 1. 边界

`car_run.c/.h` 位于底盘控制层，负责车体速度到四个轮子目标转速的运动学解算，并调用 `x42s_motor` 发送同步速度命令。它不重新实现 CAN 帧协议，也不解析 OPS 数据。

## 2. 对外接口

```c
HAL_StatusTypeDef CarRun_Init(FDCAN_HandleTypeDef *hfdcan,
                              const CarRun_Config *config);
HAL_StatusTypeDef CarRun_SetVelocity(float vx, float vy, float wz);
HAL_StatusTypeDef CarRun_Stop(void);
```

速度约定：`vx` 为车体前进速度，`vy` 为车体左移速度，`wz` 为逆时针角速度；单位分别为 `m/s`、`m/s`、`rad/s`。

`CarRun_Config` 集中保存轮径、左右/前后中心距、最大轮速、加速度和四个轮子的方向修正。地址和轮位固定为左前 1、右前 2、左后 4、右后 3，但发送顺序在实现中明确写出，避免数组下标误解。

## 3. 解算与换算

先按标准麦轮 X 型布置计算四轮轮缘线速度，再除以轮半径换算为角速度，最后转换为 RPM：

```text
v_lf = vx - vy - (half_length + half_width) * wz
v_rf = vx + vy + (half_length + half_width) * wz
v_lr = vx + vy - (half_length + half_width) * wz
v_rr = vx - vy + (half_length + half_width) * wz
rpm = v / (PI * wheel_diameter_m) * 60
```

公式中的正负方向通过 `wheel_sign[4]` 做最终修正。当前确认的电机符号按轮位顺序为左前 `+1`、右前 `-1`、左后 `+1`、右后 `-1`，不能修改公式内的单个符号来临时补偿。

如果任一轮速超过 `max_wheel_speed_rpm`，按四轮最大绝对值统一缩放，保持运动方向和角速度比例，不单独截断某一个轮子。

## 4. 发送与同步

`CarRun_SetVelocity` 依次按左前、右前、左后、右后调用四次 `X42S_SetSpeed(address, signed_rpm, acceleration, true)`，对应地址顺序为 1、2、4、3；全部返回成功后调用一次 `X42S_SyncStart()`。任一命令失败时立即返回错误，不发送同步启动；上层可随后调用 `CarRun_Stop()`。

第一版沿用现有 X42S 发送接口和 FDCAN FIFO，不在中断中发送。若实测发现发送 FIFO 等待影响控制周期，再独立增加发送队列或扩大 FDCAN TX FIFO，不在本子任务中重写协议层。

## 5. 状态与错误

- `CarRun_Init` 收到空句柄、空配置、零轮径或非正几何参数时返回 `HAL_ERROR`。
- 未初始化时 `CarRun_SetVelocity` 和 `CarRun_Stop` 返回 `HAL_ERROR`。
- 非有限浮点输入（NaN/无穷）返回 `HAL_ERROR`。
- 零速度输入直接调用 `CarRun_Stop`，避免向四个电机发送无意义的速度帧。
- 底层任一 X42S 命令返回非 `HAL_OK` 时向上返回该状态。

## 6. 兼容与回滚

新增模块不改变 `x42s_motor` 的公开接口和 CAN 帧格式；删除 `car_run.c/.h` 及 CMake 源文件项即可回滚。现有 `main.c` 的用户修改不纳入本子任务。

## 7. OPS 航向保持闭环

### 边界与数据流

- 新增 `car_control.c/.h` 作为命令与执行器之间的底盘控制层。它保存期望 `vx/vy/wz`、航向目标与保持状态，运行 PD 并调用 `CarRun_SetVelocity()`；`car_run.c` 仍只做麦轮解算和 X42S 命令发送。
- `test.c` 解析有效蓝牙帧后调用 `CarControl_SetVelocity()` 更新期望速度，不再直接发电机命令；蓝牙 300 ms 超时调用 `CarControl_HoldHeading()`，清零平移/手动旋转目标但保留全局航向控制。
- `OPS_Process()` 每次消费 TIM6 的 5 ms 待处理标志后返回本周期已处理状态。主循环顺序为 OPS 解析、FDCAN 消费、蓝牙命令更新、若本周期到期则调用 `CarControl_Process()`。因此新蓝牙输入会先于同周期底盘输出生效。
- TIM6 ISR 继续只清标志并置 OPS 周期标志；串口 ISR 继续只记录 DMA 位置并置事件。浮点解算、PD、CAN 发送均在主循环执行。

### OPS 有效性与周期

- 完整 OPS 帧解析到 `heading`、`z` 后，只有二者均为有限数时才更新公开数据和最近有效时间戳。其他无关字段不参与航向闭环有效性判定。
- `OPS_IsDataFresh()` 使用最近有效完整帧时间戳判断数据是否在 `50 ms` 内。超时期间禁用姿态修正，但活动的手动平移/旋转命令仍可原样运行；蓝牙失联由 `CarControl_HoldHeading()` 停止旧运动目标并继续保持航向。
- 控制每个 TIM6 5 ms 处理周期最多运行一次，使用固定 `dt=0.005 s`。若主循环忙导致周期标志合并，控制不补发多次历史计算。

### 模式与 PD 控制

- 控制模块初始化后即进入全局航向保持待机，不依赖蓝牙命令；首个有效 OPS 周期且没有主动旋转时捕获当前航向。`wz!=0` 时原样执行主动旋转并停用保持；从非零变为零时，以当前有效 OPS 航向重设目标并恢复保持。
- OPS 由超时恢复时，在没有主动旋转的情况下重新捕获当前角度，避免重放传感器失联期间累积的误差。
- 蓝牙 300 ms 超时调用 `CarControl_HoldHeading()`：立即停止旧平移/旋转目标，保持控制任务活动，并在下个新鲜 OPS 周期捕获断连时当前航向；显式 `CarControl_Stop()` 仍用于真正禁用周期控制/故障停机。
- 航向误差按最短圆周距离归一化至 `[-180 deg, 180 deg]`。PD 输出为 `Kp * error_rad - Kd * measured_wz_rad_s`，`Kp=2.0 s^-1`，`Kd=0.2`；误差在 `1 deg` 死区内时 P 项置零、仍保留角速度阻尼，输出限幅为 `±0.3 rad/s`。
- 若 OPS 无效/超时，PD 输出设为零；蓝牙正常时仍执行有效平移/手动 `wz`。失联保持模式下旧平移和手动旋转为零，周期继续运行，但 OPS 无效时不产生纠偏。

### CAN 发送负载

每周期最多发送四条轮速帧和一条同步帧，即 200 Hz 时约 1000 帧/秒；当前 FDCAN 名义速率为 1 Mbit/s、TX FIFO 深度为 8。X42S 满队列时会轮询等待，单帧等待上限为 5 ms。正常链路带宽应可承载该负载，但总线故障或持续积压时可能占满一个控制周期；本次不改发送队列，实车验证时观察是否出现发送超时。

### 风险和回滚

- OPS 航向轴正方向与底盘 `wz` 正方向必须实车确认；若方向相反，需在控制层统一修正，不改麦轮解算公式。
- `Kp/Kd`、死区及超时作为 `car_control.h` 可调宏，首版仅提供保守起始值，需低速安全条件下验证。
- 回滚时移除 `car_control.c/.h` 与 CMake 项，恢复 `test.c` 对原速度接口的调用，并将 `OPS_Process()` 返回类型恢复；不改 FDCAN、X42S 协议及中断行为。

## 8. OPS 位置闭环与显式模式

### 边界与公开接口

```c
typedef enum {
  CAR_CONTROL_MODE_OPEN_LOOP = 0,
  CAR_CONTROL_MODE_CLOSED_LOOP = 1
} CarControl_Mode;

HAL_StatusTypeDef CarControl_Move(float dx_mm, float dy_mm,
                                  float dangle_deg);
HAL_StatusTypeDef CarControl_SetMode(CarControl_Mode mode);
CarControl_Mode CarControl_GetMode(void);
uint8_t OPS_IsPoseFresh(void);
```

- `car_control.c` 保存启动零点、累计目标、展开航向、模式和输出状态；仅在主循环周期计算，`car_run.c` 保持同步速度接口。
- `ops.c/.h` 新增位姿有效性查询：最新完整帧 X/Y/heading/z 全部有限，且原有效帧时间戳未超 50 ms。无效新帧不得以旧位置继续标记位姿新鲜。
- Move 仅在已初始化、闭环、零点就绪及参数/累加结果有限时成功；无效请求不部分写入目标。
- 切换模式保留目标，清除手动速度并停止旧输出；同模式重复设置无副作用。显式 Stop 禁用周期输出，新命令或实际模式切换可恢复。
- 保留原初始化/速度/停止/处理接口及必要兼容入口，更新说明；SetVelocity 只在开环接受，避免速度接口隐式劫持位置环。

### 启动固定坐标与传感器转换

- 首帧有效位姿在 MCU 内捕获零点，位置和角度目标初始化为零。
- 默认 OPS Y 朝车头、X 朝右：把 OPS 坐标 `(x,y)` 转为前进/左移约定 `(y,-x)`，根据安装角和传感器相对底盘中心偏移修正，再旋转到启动航向对应的固定坐标。
- 目标位置误差在固定坐标中计算；按当前相对启动航向的负角度旋转输出速度得到车体 vx/vy，传入 CarRun。
- 当前角度通过相邻有效 OPS 航向差的最短角展开得到累计相对角度；目标角度直接累计 dangle_deg，控制误差不做整圈取模，支持 +270 deg 或 +360 deg。
- 开环期间仍跟踪有效位姿与航向展开以保持同一零点，输出旁路 OPS；数据恢复不修改目标或零点。
- 头文件默认安装偏角和前/左中心偏移为零，航向正方向沿用当前约定；需要实车核对，不把默认值当作安装已确认。

### 控制与限幅

- 平移采用 `vx_fixed = Kp_position * ex_m`、`vy_fixed = Kp_position * ey_m`；默认 `Kp_position=1.0 s^-1`、径向死区 `5 mm`，死区内平移为零。
- 平移合速度统一缩放到 0.5 m/s；旋转保留当前 Kp=3.0/Kd=0.4 和 1 deg 死区，上限设为 2.0 rad/s，与旧归一化速度输入 1000 对应的上限一致。
- 闭环 OPS 无效/超时时停止输出、保留状态；无旧输出且无新输出时不重复发送停止帧。恢复后继续控制原目标。
- 发送失败禁用周期输出并尝试停止，返回底层状态，不由正常周期自动重启。

### 蓝牙分发与看门狗

| 帧 | 含义 | 生效条件 |
|---|---|---|
| `P,500,0,90\r\n` | 目标 X 增加 500 mm、角度增加 90 deg | 已就绪闭环 |
| `M,0\r\n` | 切为纯开环，清除旧手动速度 | 参数合法 |
| `M,1\r\n` | 切为闭环，恢复保存的目标 | 参数合法 |
| `C,1000,0,0\r\n` | 以 0.5 m/s 前进 | 开环 |

- P 参数严格解析有限浮点，C 保持原整数及 ±1000 范围；M 只接受 0/1，不由其他命令自动切换模式。
- 帧以换行结束；行缓存能容纳三项带符号小数，溢出整行丢弃至下个换行。解析完整成功后才改变目标或看门狗。
- 300 ms 看门狗只在开环且有合法 C 指令时工作；P 不启用看门狗，模式切换清除旧速度计时状态。
- 重复 P 按真实重复位移调用处理，无去重/重传协议。

### 实施与回滚

- 只改已有 car_control、ops、test 的 `.h/.c` 及 Trellis 文档；无需新增 CMake 源文件或 main 上电位移测试。
- 限定编译、模拟和静态检查；实车需核对 X/Y、角度符号及安装配置。
- 回退本轮需同时恢复控制/蓝牙/位姿接口，不删除已有 car_run 或覆盖无关用户修改。
