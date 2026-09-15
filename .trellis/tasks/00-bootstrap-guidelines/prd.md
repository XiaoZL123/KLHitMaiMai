# 00-bootstrap-guidelines：工创赛工程规范引导

## 目标

基于仓库真实文件重建 `.trellis/spec/`，删除不适用的 Web 后端/前端模板，覆盖综合工程目录、STM32H743 固件、硬件/机械/视觉资料以及 Git 和构建产物。

## 约束

- 只修改 `.trellis/spec/` 和本任务资料，不修改业务代码、硬件文件、机械资料或 `other/`。
- 每条重要约定引用真实路径或代码样例；不写理想化且当前不存在的约定。
- 任务完成后保留任务为未归档状态。

## 验收标准

- [x] 已检查顶层目录、文件类型、Git 忽略和 `Veiw/` 当前为空。
- [x] 已检查 `SoftWare/BIGDICK` 的 `.ioc`、CMake、工具链、`.vscode`、`Core`、`Drivers`、链接脚本和构建目录。
- [x] 已以 `project`、`stm32`、`guides` 替换 backend/frontend 模板。
- [x] 已覆盖代码风格、HAL、外设、中断、DMA、回调、CubeMX 保护、构建/调试/烧录、资料组织和产物管理。
- [x] 已检查索引、链接、占位符和路径证据；任务不归档。
