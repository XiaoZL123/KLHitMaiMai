# OPS与蓝牙协议主机验证

从仓库根目录执行：

```powershell
& 'C:/mingw64/bin/gcc.exe' -std=c11 -Wall -Wextra -Werror -pedantic -I '.trellis/tasks/09-22-car-run-module/validation/protocol' -I 'SoftWare/BIGDICK/Core/Inc' '.trellis/tasks/09-22-car-run-module/validation/protocol/protocol_host.c' 'SoftWare/BIGDICK/Core/Src/ops.c' 'SoftWare/BIGDICK/Core/Src/test.c' -lm -o "$env:TEMP/gongchuang_protocol_host.exe"
& "$env:TEMP/gongchuang_protocol_host.exe"
```

直接编译真实 `ops.c` / `test.c` 与公开头文件；HAL、UART、GPIO、CAN和控制API由主机桩隔离。控制桩只记录接口调用，没有复制位置控制算法；实际控制算法由同任务的控制验证覆盖。

覆盖：OPS完整/截断帧、DMA环绕、X/Y/heading/z的NaN/Inf、新无效帧立即失效、50 ms边界及毫秒回绕；P的有符号小数与重复分发、M/C模式约束、错误/超长/控制字符帧恢复；P超过300 ms不会触发速度停车，开环C的300 ms边界、重连、失败提交、停车重试、同模式/坏帧不续期及毫秒回绕。

这是纯主机验证，不加入固件构建，不烧录或驱动车辆。
