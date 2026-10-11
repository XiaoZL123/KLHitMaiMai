# OPS 位姿闭环与蓝牙控制

## 1. Scope / Trigger

适用于 `car_control.c/.h`、`ops.c/.h` 与 `test.c/.h` 的位置/航向控制和蓝牙命令。`CarRun` 只完成麦轮解算和 X42S 同步发送，不依赖 OPS。控制由主循环在消费 TIM6 5 ms 节拍后调用，中断只置位或记录接收位置。

## 2. Signatures

```c
typedef enum {
  CAR_CONTROL_MODE_OPEN_LOOP = 0,
  CAR_CONTROL_MODE_CLOSED_LOOP = 1
} CarControl_Mode;

HAL_StatusTypeDef CarControl_Init(void);
HAL_StatusTypeDef CarControl_Move(float dx_mm, float dy_mm, float dangle_deg);
HAL_StatusTypeDef CarControl_SetMode(CarControl_Mode mode);
CarControl_Mode CarControl_GetMode(void);
HAL_StatusTypeDef CarControl_SetVelocity(float vx, float vy, float wz);
HAL_StatusTypeDef CarControl_HoldHeading(void);
HAL_StatusTypeDef CarControl_Stop(void);
HAL_StatusTypeDef CarControl_Process(void);
uint8_t OPS_IsDataFresh(void);
uint8_t OPS_IsPoseFresh(void);
```

## 3. Contracts

- 默认位置闭环：首次有效 OPS 位姿捕获启动零点，之后保持目标 `(0,0,0)`。未收到有效位姿前不输出位置运动。
- Move 单位为 mm/mm/deg，X 正值为启动时车头方向、Y 正值为启动时左侧、角度正值逆时针。方向固定，每次成功调用累加到上个目标，不能累计到当时实际位置。
- 目标抵达后仍闭环；重复位置帧表示重复调用，不能在上位机把 P 当成速度帧周期重发。
- OPS 推荐安装为 Y 朝前、X 朝右、传感器中心与底盘中心重合。控制中先转换坐标与安装偏移，再将固定坐标速度旋转到当前车体。实际安装方向/偏移/角度符号通过控制头文件宏设置，必须实车核对。
- 角度连续展开；角度目标不对 360 取模，`+270/+360 deg` 必须按正向完成。无法从长时间无数据期间的 ±180 deg 输出推断整圈数量。
- 平移使用位置 P，角度使用航向 PD；闭环平移合速度上限 `0.5 m/s`、角速度上限 `2 rad/s`。控制层定义统一速度宏，蓝牙归一化 1000 映射到同一上限。
- OPS 航向查询只要求 heading/z 有限及年龄不超过 50 ms；位置闭环必须用 PoseFresh，要求最新完整帧 X/Y/heading/z 全部有限。新无效位姿不能借旧数据继续报告可用。
- OPS 超时/无效停止闭环输出，保留零点和目标；数据恢复后继续原目标，不自动捕获新的零点或目标。已有零点时，航向仍可按有效航向数据持续展开。
- 开环旁路全部纠偏，仅 SetVelocity 有效。模式实际改变时停车、清旧手动速度，保留位置目标；切回闭环恢复原目标，同模式请求幂等。
- 显式 Stop 禁用周期输出并保留目标；新有效运动命令或实际模式切换可重新启用。发送错误尝试停车并禁用输出，不由正常周期自动解除。
- HoldHeading 保留为兼容入口：清手动速度并按需停车，不改变模式/位置目标、不捕获新航向、不解除显式 Stop。Move 在零点已建立后允许于反馈暂时失效时累加目标，输出等待有效反馈恢复。
- 无运动输出时不反复发送停止帧；从非零输出变为零或反馈失效时发送停止。

| 蓝牙帧（ASCII） | 生效条件与含义 |
|---|---|
| `P,500,0,90\r\n` | 已就绪闭环；目标 X 增加 500 mm、角度增加 90 deg |
| `M,0\r\n` | 切开环 |
| `M,1\r\n` | 切闭环并恢复保存的目标 |
| `C,1000,0,0\r\n` | 开环；前进速度 0.5 m/s，C 各值为 ±1000 整数 |

P 支持有符号普通十进制小数。300 ms 看门狗只作用于合法 C 速度帧；P 不启用看门狗，模式切换必须隔离旧速度计时状态，同模式请求不能延长 C 有效期。

## 4. Validation & Error Matrix

| 条件 | 行为 |
|---|---|
| 未初始化、零点未建立、Move 非闭环 | 拒绝位移，不部分修改目标 |
| 输入或累计目标非有限 | 返回错误，保留目标 |
| 新位姿 X/Y/heading/z 非有限，或年龄大于 50 ms | 闭环停车并保留零点/目标 |
| OPS 不可用但处于开环 | 不叠加纠偏，正常执行手动速度及其看门狗 |
| 开环合法 C 超过 300 ms 未更新 | 停止开环输出 |
| P 命令发送后超过 300 ms | 继续追踪/保持位置目标 |
| 非法 M、残缺字段、多余字段、超长行、NaN/Inf | 丢弃，不修改模式/目标或刷新速度计时 |
| CAN 发送失败 | 返回底层状态，尝试停止并禁用周期输出 |

## 5. Good / Base / Bad Cases

- Good：两次 `Move(500,0,0)` 得到 1000 mm 目标；转过 90 deg 后 X 位移仍沿启动前方。
- Base：反馈暂时失效则停车；恢复后仍追踪原目标。切开环遥控后，切回闭环驶向保留的目标。
- Bad：只看航向有效就用 NaN 的 X/Y；每收到一帧 P 就以当时位置为起点；速度看门狗取消位置任务；模式切换自动捕获新目标。

## 6. Tests Required

- Debug 构建与真实源码主机模拟：启动零点、目标累加、固定坐标旋转、非零起始航向、跨界/整圈、合速度限幅。
- OPS 最新无效帧、50/51 ms 有效性边界、数据恢复保留目标、开环和 Stop 期间航向跟踪。
- 模式往返、同模式幂等、Stop/发送失败、P/M/C 分发及错误帧恢复、tick 回绕和看门狗不影响闭环。
- 模拟不向固件加入测试入口；实车验证另外核对 OPS 轮接地、X/Y 测量、安装/符号和增益。

## 7. Wrong vs Correct

错误：`test.c` 直接发送 CarRun 速度，或在 TIM6 ISR 计算位置环；用重捕获目标代替传感器超时处理。

正确：蓝牙只更新控制命令/模式；主循环先处理 OPS/FDCAN/蓝牙，再按 5 ms 节拍调用 CarControl，经过坐标转换和限幅后统一调用 CarRun。
