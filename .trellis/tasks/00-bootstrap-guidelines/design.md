# 规范引导设计

## 目标

将初始化时生成的 Web 模板替换为适合工创赛综合工程的仓库级规范，覆盖资料目录和 `SoftWare/BIGDICK` STM32H743 固件。

## 规范边界

- `project`：顶层目录、资料格式、Git 与构建产物。
- `stm32`：C11 代码、HAL/中断/DMA、CubeMX 保护、CMake 与 VS Code 调试。
- `guides`：跨文件变更面和嵌入式数据流检查。

## 证据来源

规范中的示例来自 `SoftWare/BIGDICK/BIGDICK.ioc`、`CMakeLists.txt`、`CMakePresets.json`、`cmake/`、`.vscode/`、`Core/`、`Drivers/`、`STM32H743XX_FLASH.ld`，以及 `HardWare/`、`Machine/`、`Veiw/`、根 `.gitignore` 的真实内容。
