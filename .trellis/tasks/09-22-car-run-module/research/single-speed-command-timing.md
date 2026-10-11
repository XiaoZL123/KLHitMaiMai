# Research: single speed command timing

- Query: 检查蓝牙单帧速度指令的实际保持时间、停止延迟、加速度影响，以及手机 App 的发送周期。
- Scope: mixed（SoftWare/BIGDICK 固件与 SoftWare/PhoneApp）
- Date: 2026-09-25

## Findings

### Firmware command lifetime

- `SoftWare/BIGDICK/Core/Src/test.c:87-119` 以 `\n` 作为一帧结束；`\r` 被忽略。收到合法完整帧后，`Test_ApplyCommand()` 在 `test.c:69-83` 调用 `CarRun_SetVelocity()`，并在调用返回后记录 `test_last_command_tick = HAL_GetTick()`。
- `SoftWare/BIGDICK/Core/Src/test.c:131-154` 的失联保护只在主循环调用 `Test_Process()` 时检查，阈值是 `TEST_CONTROL_TIMEOUT_MS = 300U`（`Core/Inc/test.h:16-18`）。因此最后一帧合法指令后，目标速度至少保持约 300 ms，随后主循环才发送一次 `CarRun_Stop()`。
- 这不是“一帧时长的速度脉冲”：`SoftWare/BIGDICK/Core/Src/x42s_motor.c:316-346` 发送的是 X42S 速度模式目标值，电机内部会持续保持目标转速，直到收到下一条速度目标或停止命令。
- `SoftWare/BIGDICK/Core/Src/car_run.c:117-151` 将非零车体速度转换为四轮 RPM，并对四个电机调用 `X42S_SetSpeed(..., acceleration_rpm_s, true)`；当前 `main.c:25-32` 配置的加速度为 `1000 RPM/s`。因此速度切换还受电机自身加减速斜坡影响。

### Approximate timing

- MCU 收到 LF 后，单次 `CarRun_SetVelocity()` 包含 4 个速度命令和 1 个同步广播。每个速度命令 payload 长度为 10 字节，会被 `X42S_SendRaw()` 拆成 2 个经典 CAN 帧（`x42s_motor.c:247-291`），所以一组更新通常是 9 个 CAN 帧。
- FDCAN 配置为 500 kbit/s、8 个 TX FIFO 元素（`Core/Src/fdcan.c:5-39`）。以经典扩展帧估算，9 帧总线占用约 2~3 ms；正常 FIFO 有空间时 CPU 调用通常也在数毫秒内完成。`X42S_SendFrame()` 在 FIFO 满时最多等待 `X42S_TX_TIMEOUT_MS = 5U` 每帧（`x42s_motor.c:61-97`），极端拥塞会让一次底盘更新明显变长。
- 停止路径同样发送 4 个带同步标志的停止命令和 1 个同步广播（`car_run.c:154-175`）。因此正常情况下“最后一帧合法蓝牙指令 -> 停止帧真正发出”约为 300 ms + 主循环调度/CAN 排队的数毫秒；不能按 50 ms 手机发送周期理解为 50 ms 自动停止。
- UART5 为 9600 8N1（`Core/Src/usart.c:8-29`）。`C,300,0,0\r\n` 共 11 字节，串口线上约 11.5 ms 才能接收完整；这段时间还未开始解析，故从手机开始发送到 MCU 记录时间约增加 12 ms。

### Phone app cadence

- `SoftWare/PhoneApp/app/src/main/java/com/gongchuang/remote/MainActivity.java:29` 将周期设为 50 ms，`sendRunnable` 在 `MainActivity.java:40-45` 循环调度，`sendCurrentCommand()` 在 `MainActivity.java:161-169` 发送 `C,%d,%d,%d\r\n`。
- 因而正常 App 操作会约每 50 ms刷新目标速度，300 ms 看门狗不会触发。松手或页面暂停时 `ControlView.releaseControl()`/`onPause()` 发送零速（`MainActivity.java:227-236`），但底层仍以 X42S 速度模式和电机加速度参数执行。
- `BluetoothService.send()`（`BluetoothService.java:74-86`）在 synchronized 方法中直接 `OutputStream.write()` + `flush()`；发送量只有 11 字节，通常远小于 50 ms 周期，但它运行在主线程的 `Handler` 回调内，系统蓝牙阻塞时可能短暂拖慢 UI。

### Likely cause of perceived delayed switching

1. 用发送软件只发一帧非零速度时，X42S 会持续保持目标速度；固件设计为 300 ms 无新帧后才发停止命令，因此单帧至少造成约 300 ms 运动命令保持。
2. 连续切换方向时，当前 `1000 RPM/s` 加速度使电机按斜坡追踪新目标，而不是瞬时改变速度。最大轮速若接近 3000 RPM，单向减速理论上可达约 3 s；手机 App 的 0.5 m/s 上限对应约 95.5 RPM，典型减速约 95 ms，反向切换还需再加速，约 191 ms。
3. 每次底盘更新最多 9 个 CAN 帧；TX FIFO 满时 `X42S_SendFrame()` 可按帧等待最多 5 ms。若上层快速堆积蓝牙帧，`Test_Process()` 的 `while (UART5_ReadByte())` 会连续解析并逐帧同步发送，可能造成旧指令排队后才执行。正常 50 ms 周期、500 kbit/s 总线下，理论总线占用约数毫秒，主要风险是异常 FIFO 拥塞而不是固定 50 ms 延迟。

## Caveats / Not Found

- 没有在实车上测量 FDCAN FIFO 深度、蓝牙模块 UART 缓冲和电机内部实际减速曲线；上述 CAN 时间为按 500 kbit/s 经典扩展帧的估算。
- X42S 手册的具体速度模式内部停止/减速实现未能从本地 PDF 文本抽取验证；代码明确发送 `0xFE 0x98` 停止命令，但停止后的机械惯性仍取决于电机固件与负载。
- 若需要“每帧只保持一个周期”，不能仅调整 App 周期；应明确改变固件看门狗阈值/周期，或使用每帧带有效期的控制协议。降低 `TEST_CONTROL_TIMEOUT_MS` 会缩短失联停车时间，但不会消除电机加速度斜坡。
