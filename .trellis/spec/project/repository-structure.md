# 仓库目录职责

仓库是工创赛综合工程资料库，不是前后端 Web 项目。当前顶层目录如下：

| 目录 | 当前职责与证据 |
|---|---|
| `HardWare/` | 硬件与器件资料，当前包含 `ZDT_X42S第二代闭环步进电机用户手册V1.0.4_260401.pdf`、`全方位平面定位系统OPS使用说明书.pdf`。 |
| `Machine/` | 机械结构与装配资料，当前以 `总装/` 为主，包含 `.SLDPRT` 零件、`.SLDASM` 装配体和 `.STEP` 外部模型。 |
| `SoftWare/` | 嵌入式软件工程；当前唯一可见工程为 `BIGDICK/`。 |
| `Veiw/` | 视觉资料预留目录；当前没有文件，保持原拼写 `Veiw`，不要擅自改名。 |
| `other/` | 根 `.gitignore` 忽略的目录，不纳入规范、构建或提交分析。 |

## BIGDICK 边界

`SoftWare/BIGDICK/` 是 STM32H743VIT6 的 C/ASM 固件工程：

- `BIGDICK.ioc` 是 STM32CubeMX 配置源，设备为 `STM32H743VITx`，目标工具链为 CMake（文件中的 `ProjectManager.TargetToolchain=CMake`）。
- `Core/Inc` 与 `Core/Src` 放 HAL 初始化、启动流程、中断和本项目外设/协议模块；业务文件例如 `Core/Src/oled.c`、`ops.c`、`x42s_motor.c`。
- `Drivers/CMSIS` 与 `Drivers/STM32H7xx_HAL_Driver` 是芯片和 HAL 依赖，不能把业务逻辑放入其中。
- `cmake/` 放 GCC/Clang 工具链和 CubeMX 聚合目标；根 `CMakeLists.txt` 仅额外列出业务源文件。
- `.vscode/` 放构建任务、Cortex-Debug 配置、clangd/CMake 设置；`.settings/` 为工具状态文件。
- `STM32H743XX_FLASH.ld` 与 `startup_stm32h743xx.s` 是链接和启动文件，修改需同时检查目标芯片和内存布局。

新增 STM32 模块沿用 `Core/Inc/<module>.h` + `Core/Src/<module>.c`，并在 `CMakeLists.txt` 的 `target_sources` 中显式加入源文件。
