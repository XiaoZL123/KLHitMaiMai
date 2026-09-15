# CMake 构建、编译、烧录与调试

## 构建

- 从 `SoftWare/BIGDICK` 执行 CMake Presets。`CMakePresets.json` 使用 Ninja，工具链为 `cmake/gcc-arm-none-eabi.cmake`，输出目录为 `build/<presetName>`。
- Debug/Release 预设分别设置 `CMAKE_BUILD_TYPE`；默认构建类型在 `CMakeLists.txt` 中为 `Debug`。常用命令：

```text
cmake --preset Debug
cmake --build --preset Debug
cmake --preset Release
cmake --build --preset Release
```

- GCC 工具链目标为 `cortex-m7`、硬浮点 `fpv5-d16`，编译启用 `-Wall`、函数/数据分段；Debug 为 `-O0 -g3`，Release 为 `-Os -g0`（`cmake/gcc-arm-none-eabi.cmake`）。
- 根 `CMakeLists.txt` 通过 `add_subdirectory(cmake/stm32cubemx)` 引入 HAL/CMSIS，并显式添加 `oled.c`、`rgb_led.c`、`tim.c`、`x42s_motor.c`、`usart.c`、`ops.c`。新增 `.c` 文件必须加入这里或对应 `cmake/stm32cubemx/CMakeLists.txt`。
- 链接使用 `STM32H743XX_FLASH.ld`，输出 `.elf`、`.map` 并打印内存用量；检查 `build/Debug/BIGDICK.map` 中 RAM/FLASH 是否合理。

## VS Code 调试

- `.vscode/tasks.json` 的默认任务调用 `cmake --build --preset Debug`；`.vscode/launch.json` 使用 Cortex-Debug、`servertype: pyocd`，目标 `stm32h743xx`，可执行文件为 `build/Debug/BIGDICK.elf`，并在启动前执行构建任务。
- 调试入口设置为 `runToEntryPoint: main`；使用仓库已有的 `arm-none-eabi-gdb` 和 pyOCD 配置，不在规范中虚构 ST-Link 或其他烧录链路。
- `.vscode/c_cpp_properties.json` 从 `build/Debug/compile_commands.json` 获取编译数据库；配置改变后先重新配置 Debug 构建。

## 烧录与验证

- 当前仓库只明确记录 pyOCD 调试/下载入口，未提供独立的 `flash` CMake preset 或命令；烧录通过 Cortex-Debug 启动配置完成。
- 烧录前必须成功生成 `BIGDICK.elf`，确认目标为 STM32H743，并在调试器中从 `main` 停止/运行验证。
- `cmake/starm-clang.cmake` 是备用 ST ARM Clang 工具链，除非环境明确提供 `starm-*` 和相关变量，否则继续使用 Preset 默认的 GCC 工具链。
