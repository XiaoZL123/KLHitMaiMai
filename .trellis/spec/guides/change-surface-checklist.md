# 变更面检查

- 修改 `BIGDICK.ioc` 后检查 `Core/Src` 初始化、`Core/Inc` 句柄声明和 `cmake/stm32cubemx/CMakeLists.txt` 源文件清单。
- 修改 DMA 缓冲区后同时检查 `Core/Src/usart.c` 的段属性和 `STM32H743XX_FLASH.ld` 的 `.dma_buffer` 段。
- 修改构建输入后清理或重新配置 `SoftWare/BIGDICK/build/Debug`，确认 `compile_commands.json` 与实际编译器一致。
- 修改机械装配文件时检查同目录被引用的 `.SLDPRT`、`.SLDASM`、`.STEP` 文件是否仍存在；不要移动单个零件破坏装配引用。
