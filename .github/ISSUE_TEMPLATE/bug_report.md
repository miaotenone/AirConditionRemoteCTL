---
name: 缺陷报告
about: 可复现的固件、脚本、构建或文档问题
title: '[Bug] '
labels: bug
---

## 现象与预期

描述实际结果、预期结果及影响，勿贴真实凭据。

## 复现步骤

1. 使用的 git revision（含是否 dirty）、PlatformIO 环境：
2. 最少复现步骤/命令：
3. 脱敏日志、退出码：

## 环境

- OS / Python / 编译器 / PlatformIO 版本：
- 硬件型号、供电、接线差异（如相关）：
- 是否能用 `python scripts/check.py --build` 复现：

## 安全边界

- [ ] 已移除 WiFi/MQTT 密码、WLAN 明文和其他敏感信息
- [ ] 已区分主机测试/编译与真机表现
- [ ] 涉及设备操作时已说明是否复位、发送红外、清 NVS 或烧录

安全漏洞请按 [安全说明](../../SECURITY.md) 私密报告，不在公开 issue 披露。
