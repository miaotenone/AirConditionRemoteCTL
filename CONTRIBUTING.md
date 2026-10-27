# 贡献指南

1. 先读 [开发指南](docs/development.md) 和 [运行时约定](docs/runtime.md)。一个改动解决一个问题，不顺手修改服务端或真实配置。
2. 修 bug / 重构先补行为回归：观察失败，再实现，再运行全套。不要仅断言源码文本或依赖 mock 自己的行为。
3. 保持 MQTT topic/payload/QoS/retained、红外字节、超时边界与 NVS key/写入时机不变。协议或策略变化需要单独设计并协调两端。
4. 提交前执行 `python scripts/check.py --build`、`git diff --check` 并检查 diff；格式修复用 `python scripts/check.py --fix --format-only`。
5. PR 写明动机、行为差异、测试命令/结果及未验证项。没有设备就明确写“未做真机回归”，不要把编译通过等同于硬件通过。

不要提交 WiFi/MQTT 密码、Windows WLAN 明文、串口配置输出、私有网络截图、`.env` 或构建产物。不要在自动化测试/CI 中连接设备、读真实凭据或执行 upload/OTA。

安装/升级依赖时更新固定版本，先记录理由，再跑原生门禁和两环境编译。发现安全问题先看 [SECURITY.md](SECURITY.md)，勿将密钥放进公开 issue。
