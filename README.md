# AirConditionRemoteCTL — 空调远程控制网关（ESP32）

通过 **MQTT（局域网）** 接收指令，经 **UART** 驱动 **高配版红外模块** 控制 **美的空调**，并用 **1.3" I2C OLED** 显示状态。支持 **网页配网/配置**（换网络无需重新编译）。

完整链路：

```
[手机] ⇄ 飞书机器人 ⇄ [Linux 设备 / 树莓派]  ⇄(MQTT/局域网)⇄  [ESP32]  ⇄(UART)⇄  [红外模块]  →红外→ [美的空调]
                                                                    └─(I2C)─ [1.3" OLED]
```

> 飞书机器人与树莓派侧服务属于**另一个仓库**。本仓库只负责最靠近硬件的 ESP32 固件，与树莓派通过下面固定的 MQTT 契约对接。

## 目录结构

```
platformio.ini              工程与依赖
src/
  main.cpp                  编排：收指令→发帧→发布状态→刷新 OLED
  ac_encoder.{h,cpp}        协议帧编码（纯函数，可原生单测）
  ac_state.{h,cpp}          空调状态模型 + 边界夹取（纯函数）
  ir_module.{h,cpp}         UART 驱动红外模块
  net_mqtt.{h,cpp}          WiFi + MQTT（重连 / 遗嘱 / retained 状态）
  display.{h,cpp}           U8g2 OLED 显示
  config_store.{h,cpp}      NVS 配置读写
  web_config.{h,cpp}        WiFiManager 配网 + 配置页
  config.h / pins.h         默认参数与引脚
docs/
  protocol.md               串口协议编解码表
  wiring.md                 接线表
  superpowers/specs/...     设计文档
  superpowers/plans/...     实现计划
tests/native/               原生单元测试
```

## 硬件

- ESP32-WROOM-32E 开发板
- 高配版红外模块（TTL 串口 115200 8N1）
- 1.3" 4 脚 I2C OLED
- 接线见 [`docs/wiring.md`](docs/wiring.md)

## 构建与烧录

1. 安装 [PlatformIO](https://platformio.org/)（VS Code 扩展，或 `pip install platformio`）。
2. 编译：
   ```bash
   pio run
   ```
3. 连接开发板后烧录：
   ```bash
   pio run -t upload
   ```
4. 串口监视：
   ```bash
   pio device monitor
   ```

## 首次配网与配置

1. 上电后若从未配网（或连不上），ESP32 会开一个热点 **`AC-Gateway-Setup`**（密码 `acgateway`）。
2. 手机连上该热点，浏览器打开配置页，填写：
   - WiFi 名称/密码
   - **MQTT 服务器**（树莓派 IP）、端口、账号、密码
   - Topic 前缀（默认 `ac`）、设备名、空调品牌（十六进制，美的1 = `0x10`）
3. 保存后自动重启并连接。**以后换网络/换服务器只需重新进入配置页，无需重新编译烧录。**

## MQTT 契约（与树莓派仓库的接口）

| Topic | 方向 | Payload 示例 |
|---|---|---|
| `ac/cmd` | 树莓派 → ESP32 | `{"power":1}` / `{"temp":24}` / `{"mode":1,"fan":2}` |
| `ac/state` | ESP32 → 树莓派（retained）| `{"online":true,"power":1,"mode":1,"temp":24,"fan":0,"brand":16}` |
| `ac/status` | ESP32 遗嘱（LWT）| `{"online":false}` |

字段取值：`power` 0/1；`mode` 自动0/制冷1/除湿2/送风3/制热4；`temp` 16~30；`fan` 自动0/低1/中2/高3。指令只带需要改的字段，越界值会被夹取到合法范围。

## 串口协议

见 [`docs/protocol.md`](docs/protocol.md)（含 `0x30` 多参数 / `0x31` 单参数 / `0x10`,`0x12` 学习帧与校验算法与品牌码表）。

## 测试

协议编码与状态夹取是纯函数，可脱离硬件做原生单元测试：

```powershell
./scripts/run_native_tests.ps1
```

## 说明与限制

- 本固件发的是**多参数（0x30）**帧，一次下发完整状态，最稳。
- `0x31` 单参数与 `0x10/0x12` 学习帧编码器已提供（`ac_encoder`），主路径暂未使用，便于后续扩展。
- OLED 默认按 **SH1106** 驱动 1.3" 屏；0.96" SSD1306 请改 `src/config.h` 的 `OLED_SH1106` 为 `0`。
- 默认引脚：OLED SDA=21/SCL=22，红外模块 RX=16/TX=17（见 `src/pins.h`）。
