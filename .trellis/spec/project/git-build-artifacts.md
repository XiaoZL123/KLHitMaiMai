# Git 与构建产物

- 根 `.gitignore` 忽略 `other/`；任何规范分析、任务上下文和提交清单都排除该目录。
- `SoftWare/BIGDICK/.gitignore` 忽略 `build`。`build/Debug/` 中已存在 `BIGDICK.elf`、`BIGDICK.map`、`compile_commands.json`、Ninja/CMake 缓存，这些是可再生产物，不提交。
- 需要交付固件时以构建目录生成的 `BIGDICK.elf` 为依据；不要把 `.o`、`.d`、`.ninja_*`、`.cmake` 缓存复制到源码目录。
- 受版本控制的构建输入包括 `BIGDICK.ioc`、`CMakeLists.txt`、`CMakePresets.json`、`cmake/`、链接脚本、启动文件和 `Core`/`Drivers` 源码。
- 提交按一个可说明的资料或固件变更组织，消息风格以仓库历史为准；本次引导只修改 `.trellis/`，不改业务、硬件或机械文件。
