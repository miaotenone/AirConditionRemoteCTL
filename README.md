# AirConditionRemoteCTL

ESP32-WROOM-32E 空调网关：通过局域网 MQTT 收指令，经 UART 红外模块控制空调，OLED 显示状态，DHT11 上报室温/湿度。配网与 MQTT 配置写入 NVS，无需把账号密码编入固件。飞书/树莓派服务在独立仓库，本仓库只负责设备端。

```text
服务端 ← MQTT → ESP32 → UART 红外模块 → 空调
                    ├─ I2C OLED
                    └─ DHT11
```

## 快速开始（无需硬件）

需要 Python 3.10+、Git 和支持 C++17 的 `g++`（Windows 可用 MinGW-w64，macOS 可用 Xcode Command Line Tools）。在仓库根目录执行：

```sh
python -m venv .venv
# Windows PowerShell: .venv\Scripts\Activate.ps1
# Linux / macOS:      source .venv/bin/activate
python -m pip install -r requirements-dev.txt
python scripts/check.py
python -m platformio run -e esp32dev -e selftest
```

如果系统命令为 `python3`，用它创建虚拟环境；激活后统一使用 `python`。PowerShell 禁止激活脚本时可直接运行 `.venv\Scripts\python.exe`，不必修改全局执行策略。

- `check.py`：C++ 格式、Python 格式/静态检查、原生测试、辅助脚本测试；失败即停，不连接串口或读取凭据。
- `python scripts/check.py --build`：在上述门禁后追加两环境编译；**不烧录**。首次构建需要下载已固定的工具链/库。
- `python scripts/check.py --fix --format-only`：修复 C++/Python 格式。
- `python scripts/run_native_tests.py`：只跑原生测试，不需要 PlatformIO 或格式工具。旧 `./scripts/run_native_tests.ps1` 保持兼容。可用 `CXX` 或 `--compiler` 指定编译器。

固定版本见 [platformio.ini](platformio.ini) 和 [requirements-dev.txt](requirements-dev.txt)：平台 `espressif32@6.12.0`，依赖不自动追最新版。升级必须重新跑全部门禁及两环境编译。

## 接线与首次使用

硬件：ESP32-WROOM-32E、高配串口红外模块（115200 8N1）、1.3 英寸 SH1106 OLED、DHT11。按 [接线表](docs/wiring.md) 连接，尤其确认独立供电模块与 ESP32 共地。ESP32 只支持 2.4GHz WiFi。

**以下命令会写设备；确认端口和安全条件后手动执行，不属于自动门禁：**

```sh
python -m platformio run -e esp32dev -t upload
python -m platformio device monitor -e esp32dev
```

多块串口设备时加 `--upload-port <实际端口>` / `device monitor -p <实际端口>`。无需硬件时不要执行 `scripts/serial_*.py`、`provision.py`、`wifi_reset.py` 或 `hw_selftest.py`。

1. 未配网或保存网络连接失败后，连接热点 `AC-Gateway-Setup`（出厂默认密码 `acgateway`）。
2. 打开 `http://192.168.4.1`，填写 2.4GHz WiFi、MQTT 地址/端口/认证、topic 前缀（默认 `ac`）、设备名与品牌码（美的1：十六进制 `10`）。
3. 保存并等待连接；参数及影子状态保存在 NVS。OLED 的 `LINK` 页区分 WiFi、MQTT 与服务心跳状态。
4. 恢复出厂：**先上电，再按住 BOOT 3 秒**，或开机 2 秒内串口发 `r`；这会清 WiFi、应用配置及记住的空调状态。上电前按住 BOOT 是下载模式，不是恢复出厂。

串口配网、自检、故障定位见 [硬件联调文档](docs/hardware-validation.md)。历史真机记录不代表当前代码已做真机回归。

可选三键只用于屏幕导航和只读诊断：左右切页并固定，OK 短按切换自动/固定，长按进入/退出诊断。GPIO25/26/27；**2026-10-31 已接线、烧录并通过启动检查，用户反馈基础按键与屏幕测试正常**。见 [使用说明](docs/runtime.md) 和 [联调记录](docs/superpowers/plans/2026-10-27-oled-buttons-diagnostics.md#2026-10-31-接线后联调)。

## 必须了解

- 红外单向发送，`state` 是**影子状态**，不是空调实测状态；实体遥控器操作不会自动同步。
- 设备先与 MQTT 和服务心跳都建立联系才启用失联保护。之后全链路失联满 5 分钟发送一次关机；服务心跳 90 秒失效，网络 15 秒开始重拨、10 分钟仍未恢复则重启。详见 [运行时约定](docs/runtime.md)。
- 配网属于启动流程，期间不运行主循环失联保护；门户单次上限 15 分钟，反复无法配网仍可能再次进入门户。不要把此机制当安全认证保护装置。
- 默认 OTA 已启用，当前代码未配置 OTA 密码，MQTT 是明文局域网连接。只在可信隔离网络使用，不向公网开放。见 [安全说明](SECURITY.md)。

## 文档与贡献

| 文档 | 内容 |
|---|---|
| [开发指南](docs/development.md) | 环境、质量门禁、测试边界、代码导航 |
| [运行时约定](docs/runtime.md) | MQTT、状态/NVS、计时、保护与恢复语义 |
| [串口协议](docs/protocol.md) | 红外帧、校验与品牌表 |
| [硬件联调与历史验收](docs/hardware-validation.md) | 人工联调、自检、排障与历史证据 |
| [服务端接口规格](docs/rpi-feishu-interface-spec.md) | 跨仓库契约及服务端设计背景 |
| [贡献指南](CONTRIBUTING.md) / [变更记录](CHANGELOG.md) | 提交流程与未发布变更 |

CI 配置位于 [.github/workflows/ci.yml](.github/workflows/ci.yml)，覆盖主机门禁及 `esp32dev`/`selftest` 编译，绝不自动刷机。许可证见 [LICENSE](LICENSE)。
