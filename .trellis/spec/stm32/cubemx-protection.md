# CubeMX 生成代码保护

- `BIGDICK.ioc` 是外设和引脚的事实来源：芯片为 `STM32H743VITx`，启用 I2C1、TIM1、TIM8、CORTEX_M7，并设置 `ProjectManager.KeepUserCode=true`、`ProjectManager.TargetToolchain=CMake`。
- 修改引脚、时钟、外设实例或 HAL 模块时先改 `.ioc`，再用 STM32CubeMX 重新生成；不要只手改生成函数后让配置与代码分离。
- `Core/Src/main.c`、`gpio.c`、`i2c.c`、`fdcan.c`、`tim.c`、`stm32h7xx_hal_msp.c`、`stm32h7xx_it.c` 以 CubeMX 生成结构为基础。保留生成的初始化函数、句柄、MSP 配置和错误处理；业务调用集中在用户逻辑区域或独立模块。
- 独立业务文件 `oled.c`、`rgb_led.c`、`ops.c`、`x42s_motor.c` 不应被 CubeMX 覆盖；重新生成后必须确认它们仍在 `CMakeLists.txt` 的 `target_sources` 中。
- `Drivers/` 中的 CMSIS/HAL 源码来自 `STM32Cube FW_H7 V1.12.1`（`.ioc` 的 `ProjectManager.FirmwarePackage`），不直接修改供应商实现；需要适配时在 `Core` 或 `cmake` 侧完成。
- `.ioc` 顶部明确写有 `#MicroXplorer Configuration settings - do not modify`，手工编辑仅限已验证的配置变更，并在提交说明中写明原因。
