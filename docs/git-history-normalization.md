# Git 历史规范化与主分支整合

2026-10-31，维护者授权将最新三键功能分支整合到 `main`，只保留一个主分支，并规范全部历史提交说明。

## 处理方式

将 `feature/oled-buttons-diagnostics` 快进整合到本地 `main`，随后对该主线的 42 次提交统一使用英文 Conventional Commits。保留原有提交顺序，不压缩历史；每次提交的文件树、作者、提交者及其时间均逐项核对一致。提交标题使用 `type(scope): description`，标题与正文每行不超过 72 个字符。详细规则见 [贡献指南](../CONTRIBUTING.md#提交规范)。

提交说明和父提交标识变化会改变所有提交哈希。初始提交原有的签名在改写后无法继续有效，故在新版本中移除；没有重新冒签原作者。原始签名和历史保存在迁移前的 Git bundle 中。

更新远端时，对 `main` 和两个功能分支均使用精确旧哈希作为 `--force-with-lease` 条件，并以原子推送同时更新 `main`、删除两个远端功能分支。这样任一远端分支在迁移期间有新提交，整次更新都会拒绝。本地随后删除旧功能分支、已经备份的旧历史引用，并清理失效的临时 worktree 注册。

## 历史标识

| 原提交 | 规范化后的对应提交 | 内容 |
|---|---|---|
| `701d6d8` | `27c1eff` | 启动计数与复位原因 |
| `efc226e` | `37b3a95` | OLED 仪表盘与工程规范化 |
| `18cac15` | `529f8f2` | 三键导航、只读诊断与基础验收 |

历史文档和真机日志中的旧哈希保留为当时的真实记录。设备已烧录版本 `efc226e-dirty` 的功能内容对应最新三键提交所记录的工作树；该脏树版本标记不能仅凭基线哈希视为 `efc226e` 的干净构建。本次整合不重新烧录设备；以后构建的固件将显示新历史中的哈希。

## 本机恢复资料

迁移前的全部引用已备份并通过 `git bundle verify` 验证，完整的新旧提交映射另行保存：

- Bundle：`C:\Users\DiRJ\AppData\Local\Temp\opencode\AirConditionRemoteCTL-before-normalization-20261031.bundle`
- 映射：`C:\Users\DiRJ\AppData\Local\Temp\opencode\AirConditionRemoteCTL-commit-map-20261031.json`

以上为本机临时目录中的恢复资料，不随仓库发布。需要恢复时可从 bundle 克隆到另一个目录检查，避免覆盖当前工作树。其他旧克隆应重新克隆，或先保存本地工作后将本地主分支对齐新的 `origin/main`，避免将旧历史再次合入主线。

三键基础验收沿用用户已反馈的结果，断网注入、BOOT 与长稳运行等扩展硬件测试仍以 [验收记录](superpowers/plans/2026-10-27-oled-buttons-diagnostics.md) 的范围为准。
