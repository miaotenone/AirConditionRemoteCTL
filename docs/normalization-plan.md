# 仓库规范化（2026-10-27）

范围仅限本仓库；保留 MQTT、红外字节、计时、失联保护和 NVS 语义。
不读取真实凭据，不连接真机，不烧录，不自动提交。

- [x] 核对干净工作区与适用指引；原有 5 个原生套件通过，esp32dev/selftest 编译通过。
- [x] 以回归测试驱动跨平台、失败即停入口；保留 PowerShell 兼容包装。
- [x] 用原生测试保护失联状态转换，简化主循环；不改变副作用次序。
- [x] 统一 C++/Python 格式与检查，固定已验证依赖，加入 CI。
- [x] 精简中文快速开始，迁移历史验收/排障，补开发与协作规范。
- [x] 实跑全部质量门禁和两环境编译，审查差异并列出未验证项。

基线工具：Python 3.14.3、PlatformIO Core 6.2.0、espressif32 6.12.0、g++ 15.2.0。
Linux/macOS、硬件、串口和实际 MQTT 链路需单独验证；构建成功不等于真机通过。

## 验证结果

- `python scripts/check.py --build`：退出 0；clang-format 检查通过，Ruff 静态检查通过，11 个 Python 文件格式检查通过，6 个原生套件通过，11 个 Python 测试通过。
- `python -m platformio run -e esp32dev -e selftest`（由统一门禁调用）：两环境 SUCCESS。主固件 RAM 51,932 / 327,680 字节，Flash 991,337 / 1,310,720 字节；自检 RAM 23,184，Flash 304,749 字节。
- 原 `scripts/run_native_tests.ps1`：6 套件通过；传入不存在的编译器时，Python/PowerShell 均传播退出码 2。
- 回归敏感性：临时副本将失联 `< timeout` 改成 `<= timeout`，两个精确边界测试失败；将同温度命令改成“不识别”，对应回归失败。生产文件未应用这些变异。
- `git diff --check` 通过；32 个 Markdown 本地文件链接全部存在（不含远端可用性或标题锚点验证）。
- 除 main 的显式策略提取外，22 个已有 C++ 源/头文件去注释并合并相邻字符串后 token 等价；已有 Python 辅助脚本 AST 保持，provision 仅更新过时 docstring。

本地完整输出保存在忽略的 `build/final-verification.log`、`build/final-powershell.log`。未创建提交或 worktree，未修改服务端。

## 主会话独立集成复核

- 独立审查发现 Python runner 测试的辅助编译器写死 `g++`；现已同样使用
  `CXX`，默认仍为 `g++`。新增真实子进程回归，在 PATH 前放置返回 93 的
  默认编译器陷阱，同时指定有效的绝对路径 CXX，确认不会调用默认编译器。
- 原缺陷在回归中实际失败；外部临时副本恢复硬编码后再次失败（退出 93），
  修复后全部 **12 项 Python 测试**通过。保留 MinGW DLL 搜索路径，避免把
  工具链运行库缺失误当作编译器配置缺陷。
- 最终 `python scripts/check.py --build` 再次退出 0：6 套 native、12 项
  Python 测试、格式/静态检查及 esp32dev/selftest 编译通过。
- 与原 HEAD 对照的 45,637 组红外帧和状态字段处理结果逐字节相同。
- failsafe 纯策略测试不等于主循环副作用完整集成测试；NVS → IR → MQTT
  顺序通过代码对照确认，真实持久化和物理行为仍属未验证范围。

未验证：托管 CI 实际运行、Linux/macOS 与 Python 3.10 运行、串口/OTA、真实 NVS 断电语义、红外时序、空调物理执行、真实 MQTT 链路与恢复。未读取 Windows WLAN/真实凭据，未连接或烧录设备。服务端协调和最终独立集成审查由主会话负责。
