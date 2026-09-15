# STM32 BIGDICK 工程规范

适用范围：`SoftWare/BIGDICK/` 的 STM32H743VIT6 固件。

| 文档 | 内容 |
|---|---|
| [code-style.md](./code-style.md) | C/C++（当前以 C11 为主）格式、命名和模块接口 |
| [hal-peripherals-interrupts.md](./hal-peripherals-interrupts.md) | HAL、外设、中断、DMA 和回调模式 |
| [cubemx-protection.md](./cubemx-protection.md) | `.ioc` 与生成代码边界 |
| [build-debug-flash.md](./build-debug-flash.md) | CMake、编译、烧录和调试 |

## 开发前检查

- 先确认改动属于生成初始化层还是业务模块；阅读 `BIGDICK.ioc` 和对应 `Core/Src/*.c`。
- 新增源文件要加入根 `CMakeLists.txt` 的 `target_sources`，并提供同名头文件（若有公开接口）。
- 涉及 DMA 缓冲区时同时检查 `STM32H743XX_FLASH.ld` 的 `.dma_buffer` 段和 D2 RAM 地址。

## 质量检查

- Debug 配置应能生成 `build/Debug/BIGDICK.elf`，并保持 `-Wall`、链接脚本和内存用量输出。
- 中断处理只做置位、清标志或 HAL 转发；协议解析和显示刷新在主循环或模块处理函数中完成。
- 生成代码重生成后检查业务模块、`CMakeLists.txt` 源文件清单和用户区代码是否仍在。
