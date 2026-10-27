# 开发指南

## 环境与命令

先按 [README](../README.md) 创建虚拟环境并安装 `requirements-dev.txt`。主机测试要求 Python 3.10+ 与 C++17 编译器；不依赖 Arduino、串口设备、MQTT 服务或真实凭据。当前本地验证环境为 Windows、Python 3.14.3、g++ 15.2.0、PlatformIO Core 6.2.0。Linux/macOS 的运行由 CI 配置覆盖，未运行 CI 前不要声称已验证。

```sh
python scripts/check.py                      # 格式、静态检查、原生与 Python 测试
python scripts/check.py --build              # 再编译 esp32dev / selftest，不烧录
python scripts/check.py --fix --format-only  # 只修复格式
python scripts/run_native_tests.py           # 只跑原生测试
python -m unittest discover -s tests/python -v
```

`run_native_tests.ps1` 是兼容包装，转发参数和 Python 退出码，不再维护第二份套件列表。入口与当前工作目录无关。编译或执行任一套件失败立即停止，并先删除该套件旧可执行文件，避免运行陈旧产物。输出位于忽略的 `build/native/`。

`CXX` 或 `--compiler` 指向单个编译器可执行文件；路径含空格时按 shell 规则引用。额外选项用重复的 `--compiler-arg=...`，例如：

```sh
python scripts/run_native_tests.py --compiler clang++ --compiler-arg=-O2
```

格式由 `.clang-format`、`pyproject.toml`、`.editorconfig` 和 `.gitattributes` 定义：UTF-8、4 空格、提交使用 LF，C++ 列宽 100。不要改第三方库、构建目录或本地凭据文件。原生编译启用 `-Wall -Wextra -Werror`。Python 通过 Ruff 检查导入、语法和常见错误；SCons 注入的 `Import/env` 例外仅限版本注入脚本。

## 代码导航

| 位置 | 职责 |
|---|---|
| `src/main.cpp` | 启动顺序、命令执行、NVS/IR/MQTT 副作用编排 |
| `src/ac_state.*` / `src/ac_encoder.*` | 状态夹取、红外编码；无硬件依赖 |
| `src/failsafe.*` | 失联关机判定；副作用仍由 main 按原顺序执行 |
| `src/link_guard.*` / `src/wifi_policy.*` | 网络升级恢复、门户计时/凭据策略 |
| `src/net_mqtt.*` | 主题路由、MQTT 重连、状态/环境/事件发布、SNTP |
| `src/config_store.*` / `src/state_store.*` | 应用配置和影子状态持久化 |
| `src/web_config.*` / `src/serial_console.*` | 配网门户、BOOT 恢复和串口配置 |
| `src/display.*` / `src/ir_module.*` / `src/dht_sensor.*` | 硬件适配 |
| `src/selftest.cpp` | 独立硬件自检环境，不运行主固件网络控制 |
| `scripts/` | 质量入口、版本注入及需人工使用的串口辅助工具 |

## 回归策略

原生入口的套件列表是唯一清单：编码、状态、WiFi 策略、链路看门狗、复位原因、失联保护。新增纯逻辑套件时同步注册 `scripts/run_native_tests.py`。Python 测试覆盖入口的编译失败、执行失败、缺失编译器、文本规范，不访问真实串口。

先写能观察行为的测试，再做修改：边界值、`millis()` 回绕、首次连接、重复执行和恢复路径都比检查源码字符串更有价值。提取逻辑不应改变 MQTT payload/retained、红外字节、NVS key/写入时机，或 IR/MQTT/显示副作用顺序。当前 native 测试不等于硬件/NVS 断电或完整 MQTT 集成测试。

## 构建与升级

两个环境明确指定平台 `espressif32@6.12.0` 和已验证库版本；不使用浮动 `^` 版本。`esp32dev` 是主固件，`selftest` 是人工硬件调试固件；编译两者不会更改设备。`scripts/git_version.py` 注入 git 短哈希，脏工作树带 `-dirty`；无 Git 时退回 `unknown`，不阻止构建。

升级工具链/依赖时，单独说明旧/新版本、动机、全部测试与两环境编译输出。涉及硬件时还需独立人工回归记录；不要用“编译通过”代替“真机通过”。生产固件部署、真实凭据读取、串口/OTA 操作必须由操作人单独授权。

历史设计与计划保留于 `docs/superpowers/`，用于追溯，不覆盖当前源码与 [运行时约定](runtime.md)。
