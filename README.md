# 工创赛综合工程（KLHitMaiMai）

本仓库用于集中管理工创赛项目的嵌入式软件、硬件资料、机械结构和视觉相关内容。当前主要软件工程为基于 STM32H743VIT6 的 `BIGDICK` 固件，使用 STM32CubeMX、CMake、Ninja 和 ARM GNU 工具链开发。

## 目录结构

```text
GONGCHUANG/
├─ HardWare/             # 硬件、器件和定位系统资料
├─ Machine/              # 机械零件、装配体和交换模型
├─ SoftWare/
│  └─ BIGDICK/           # STM32H743VIT6 固件工程
├─ Veiw/                 # 视觉工程及标定资料预留目录
├─ .trellis/             # Trellis 规范、任务和会话记录
├─ .agents/              # Trellis 工作流技能
└─ .codex/               # Codex 项目配置、Hooks 和子代理
```

> `Veiw` 是仓库当前使用的目录名称，请勿擅自改为 `View`，以免破坏已有路径引用。

## 主要内容

### 嵌入式软件

`SoftWare/BIGDICK/` 是 STM32H743VIT6 固件工程：

- `BIGDICK.ioc`：STM32CubeMX 配置源文件；
- `Core/Inc`、`Core/Src`：初始化代码、中断处理和业务模块；
- `Drivers/`：CMSIS 与 STM32H7 HAL 驱动；
- `cmake/`：工具链和 CubeMX CMake 配置；
- `STM32H743XX_FLASH.ld`：链接脚本；
- `startup_stm32h743xx.s`：启动文件；
- `.vscode/`：构建及 Cortex-Debug 调试配置。

当前业务模块包括 OLED、RGB LED、定时器、串口、OPS 定位系统和 X42S 电机控制。

### 硬件资料

`HardWare/` 当前包含：

- 全方位平面定位系统 OPS 使用说明书；
- ZDT X42S 第二代闭环步进电机用户手册。

### 机械资料

`Machine/总装/` 保存 SolidWorks 零件、装配体、STEP 模型及相关渲染资源。

## 构建固件

### 环境要求

- CMake 3.22 或更高版本；
- Ninja；
- ARM GNU Toolchain（`arm-none-eabi-gcc`）；
- 可选：VS Code、Cortex-Debug 和 pyOCD。
