# 贡献指南

1. 先读 [开发指南](docs/development.md) 和 [运行时约定](docs/runtime.md)。一个改动解决一个问题，不顺手修改服务端或真实配置。
2. 修 bug / 重构先补行为回归：观察失败，再实现，再运行全套。不要仅断言源码文本或依赖 mock 自己的行为。
3. 保持 MQTT topic/payload/QoS/retained、红外字节、超时边界与 NVS key/写入时机不变。协议或策略变化需要单独设计并协调两端。
4. 提交前执行 `python scripts/check.py --build`、`git diff --check` 并检查 diff；格式修复用 `python scripts/check.py --fix --format-only`。
5. PR 写明动机、行为差异、测试命令/结果及未验证项。没有设备就明确写“未做真机回归”，不要把编译通过等同于硬件通过。

不要提交 WiFi/MQTT 密码、Windows WLAN 明文、串口配置输出、私有网络截图、`.env` 或构建产物。不要在自动化测试/CI 中连接设备、读真实凭据或执行 upload/OTA。

安装/升级依赖时更新固定版本，先记录理由，再跑原生门禁和两环境编译。发现安全问题先看 [SECURITY.md](SECURITY.md)，勿将密钥放进公开 issue。

## 提交规范

使用 [Conventional Commits 1.0.0](https://www.conventionalcommits.org/en/v1.0.0/)，提交说明统一使用英文。标题格式为 `type(scope): description`，不超过 72 个字符，描述使用祈使句、不加句号。

| type | 用途 |
|---|---|
| `feat` | 新功能 |
| `fix` | 行为或缺陷修复 |
| `docs` | 文档变更 |
| `refactor` | 不改变行为的重构 |
| `test` | 测试增加或调整 |
| `perf` | 性能优化 |
| `build` | 工具链、依赖与构建配置 |
| `ci` | 持续集成流程 |
| `chore` | 仓库维护 |
| `revert` | 撤销已有改动 |

scope 使用稳定的英文模块名，例如 `firmware`、`display`、`ui`、`wifi`、`mqtt`、`sensor`、`state`、`tooling`、`repo`。正文与标题空一行，说明变更动机、主要行为及必要的兼容性信息，每行不超过 72 个字符；验证记录仅写实际执行的命令和结果。不在历史提交中补写未经验证的测试结果，不虚构 issue 编号或签署信息。

```text
fix(display): recover I2C before probing the OLED

Recover a stuck bus before display initialization so the gateway can
start after an interrupted I2C transfer.
```

一个提交聚焦一项可审查的逻辑变更，同时包含必要的测试与文档。破坏兼容性的改动使用 `type(scope)!: description`，并在正文末尾以 `BREAKING CHANGE:` 说明影响与迁移方式。撤销提交应在正文列出被撤销的提交哈希和原因。

日常开发基于最新 `main` 建立短期功能分支；检查与评审完成后整合回主分支并删除已合并分支。公开历史改写必须有维护者明确授权、可恢复备份和并发更新保护，不作为日常合并方式。本次历史整理见 [迁移记录](docs/git-history-normalization.md)。
