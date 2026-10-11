# STM32 C 代码风格

## 文件与命名

- 当前构建显式设置 `CMAKE_C_STANDARD 11`（`SoftWare/BIGDICK/CMakeLists.txt:3`）；使用 C11 可用语法，C++ 仅在实际引入时遵循同一嵌入式约束。
- 文件名按模块使用小写蛇形：`fdcan.c`、`rgb_led.c`、`x42s_motor.c`；头文件与源文件同名。
- HAL/CubeMX 初始化函数保留 `MX_` 前缀（如 `MX_TIM1_Init`、`MX_TIM8_Init`），项目模块使用模块大写前缀（如 `OPS_Init`、`OLED_Init`、`X42S_Init`）。
- 常量和宏使用大写下划线并带无符号后缀：`OLED_TIMEOUT_MS`、`RGB_PWM_STEPS`、`24U`；文件私有状态使用 `static`，如 `ops_data`、`oled_buffer`（`Core/Src/ops.c:5-11`、`oled.c:5-10`）。

## 函数与控制流

- 函数使用两空格缩进、K&R 花括号；硬件初始化失败统一调用 `Error_Handler()`，例如 `Core/Src/tim.c` 的 `HAL_TIM_PWM_Init` 检查。
- 模块接口返回 `HAL_StatusTypeDef` 或简单状态值并显式检查参数，例 `OLED_Init` 和 `X42S_SendRaw` 对空指针、长度进行校验。
- 指针参数使用 `const` 表达只读数据；解析函数保持小范围 `static`，例 `X42S_ParseFeedback`、`OLED_WriteData`。
- 每个新增功能函数添加一句简短中文注释，说明用途；现有模块普遍使用函数前注释（`Core/Src/usart.c`、`tim.c`、`ops.c`）。
- 保持寄存器常量和 HAL 常量的 `U` 后缀、位运算显式类型转换风格，参考 `Core/Src/x42s_motor.c` 的 `X42S_ReadU16/ReadU32`。

## 头文件

- 头文件使用 include guard，当前既有 `#define OLED_H`、`#define OPS_H` 等形式；公共类型和函数声明放在 `Core/Inc`，实现细节留在 `.c`。
- 模块通过 HAL 类型暴露硬件句柄时直接包含对应 HAL/外设头，例如 `oled.h` 接受 `I2C_HandleTypeDef *`，不要复制 HAL 结构体。

### 头文件与源文件同步说明

- 每个项目自维护的 `.h` 文件应在接口区域说明对应 `.c` 文件的模块职责，并列出公开函数及其作用；头文件名和源文件名保持同名配对，例如 `x42s_motor.h` 对应 `x42s_motor.c`。
- 公开函数声明前使用简洁中文注释，说明调用目的；函数参数、返回值和错误条件发生变化时，必须同步更新头文件声明、头文件说明和 `.c` 实现。
- 新增、删除或重命名公开函数时，必须同时更新 `.h` 中的函数清单和 `.c` 中的实现；私有 `static` 函数不放入公开函数清单。
- 实现细节、静态状态和协议解析辅助函数留在 `.c`，头文件只暴露调用者必须使用的类型、宏和函数。
- HAL/CMSIS 等第三方头文件不进行批量改写；已有业务头文件在后续修改对应模块时逐步补齐说明。

示例：

```c
/* x42s_motor.c：通过 FDCAN 发送 X42S 电机命令并解析反馈。 */

/* 初始化电机 CAN 驱动。 */
HAL_StatusTypeDef X42S_Init(FDCAN_HandleTypeDef *hfdcan);
/* 设置指定电机的目标速度。 */
HAL_StatusTypeDef X42S_SetSpeed(uint8_t address, float speed_rpm,
                                uint16_t acceleration_rpm_s, bool sync);
```

维护检查：修改模块接口后，至少检查对应 `.h`、`.c` 和 `CMakeLists.txt` 的文件清单，确认声明、实现和构建输入一致。
