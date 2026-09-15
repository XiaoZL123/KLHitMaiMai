# 工创赛综合工程规范

本包描述仓库级目录职责、资料组织、Git 与构建产物规则。

| 文档 | 内容 |
|---|---|
| [repository-structure.md](./repository-structure.md) | 顶层目录与工程边界 |
| [materials-organization.md](./materials-organization.md) | 硬件、机械、视觉资料组织 |
| [git-build-artifacts.md](./git-build-artifacts.md) | Git、忽略规则与构建产物 |

## 开发前检查

- 确认改动属于 `HardWare`、`Machine`、`SoftWare`、`Veiw` 中的哪一类。
- 软件改动先阅读 `stm32/index.md`；资料改动先检查原文件格式和所在子目录。
- 不把 `other/` 纳入分析或提交；根目录 `.gitignore` 明确忽略该目录。

## 质量检查

- 规范中的路径必须能在仓库中找到；目录为空时明确记录为空，不虚构文件。
- 检查构建缓存和生成文件没有进入 Git；`SoftWare/BIGDICK/.gitignore` 忽略 `build`。
