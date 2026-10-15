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
  selftest.cpp              硬件自检固件（`[env:selftest]`，不依赖 WiFi）
  config.h / pins.h         默认参数与引脚
docs/
  protocol.md               串口协议编解码表（含模块应答帧）
  wiring.md                 接线表
  rpi-feishu-interface-spec.md  树莓派 + 飞书服务接口规格（另一仓库）
  superpowers/specs/...     设计文档
  superpowers/plans/...     实现计划
scripts/
  run_native_tests.ps1      原生单元测试
  hw_selftest.py            真机串口自检抓取（自检固件）
  serial_cap.py             复位 + 抓取串口日志（主固件）
  wifi_reset.py             触发开机 `r` 恢复出厂
tests/native/               原生单元测试
```

## 硬件

- ESP32-WROOM-32E 开发板
- 高配版红外模块（TTL 串口 115200 8N1）
- 1.3" 4 脚 I2C OLED
- DHT11 室温/湿度传感器（GPIO4，数据线要 4.7k~10k 上拉到 3V3）
- 接线见 [`docs/wiring.md`](docs/wiring.md)

## 运行时行为

**OLED 三页轮播**。屏幕分三页，每 5 秒横向滑动切换（约 240 ms 过渡），只在
内容真的变了才重绘，不再是每秒无脑刷屏：

| 页 | 标题 | 内容 |
|---|---|---|
| 1 | `AIRCON` | 设定温度（大字号）、电源徽标、模式、风速 |
| 2 | `ROOM` | DHT11 室温（大字号）、湿度、与设定值的差 `d±n` |
| 3 | `LINK` | WiFi/MQTT 状态、IP、**服务心跳**（`SVC HB OK/LOST`）、运行时长 |

传感器读失败时 ROOM 页直接写 `no signal`，**不会**拿上一次的旧值冒充。

**失联自动关机（fail-safe）**。设备把「WiFi+MQTT 通」和「收到服务心跳
`<prefix>/hb`」都算作「还联系得上」，任何一半断了 **5 分钟**就发一帧红外关机，
并把 `g_state.power` 置 0。这是因为 Mosquitto 虽然跑在树莓派上，**却是独立的
systemd 单元**——broker 活着不代表发命令的那个服务活着，所以光看 MQTT 连接
是不够的。

- 启动守卫：**先成功连上一次**才武装；否则一个没网的板子上电就会去"保护"一台
  它从没控制过的空调。
- 恢复：重新连上服务后补发 `<prefix>/event`，服务端据此把空调还原成失联前的
  样子（本来就关着就不动）。
- ⚠️ 运维提示：**树莓派停机（重启/更新/断电）超过 5 分钟，空调就会被关掉**。
  这是刻意设计的行为，不是 bug。
- `ac/event` 带 `ts`，所以固件用 SNTP 同步时钟；没同步成功就**不发**事件
  （宁可不发，也不发个服务端无法判断新旧的假事件）。

**DHT11 只负责测量**。闭环决策（要不要调、调多少）在树莓派上做：固件每 30 s
读一次，把 `{temp,humidity,ok}` 发到 `<prefix>/env`。

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

1. 上电后若从未配网（或连不上），ESP32 会开一个热点 **`AC-Gateway-Setup`**（密码 `acgateway`），OLED 会显示这个热点名和密码。
2. 手机连上该热点，浏览器打开配置页（`192.168.4.1`），填写：
   - WiFi 名称/密码（**只能 2.4GHz**）
   - **MQTT 服务器**（树莓派 IP）、端口、账号、密码
   - Topic 前缀（默认 `ac`）、设备名、空调品牌（十六进制，美的1 = `0x10`）
3. 保存后自动重启并连接。**以后换网络/换服务器只需重新进入配置页，无需重新编译烧录。**
4. 配错了想重来？**上电 2 秒内从串口发 `r`** 恢复出厂（清 WiFi + 配置），见「联调检查清单」3.4。
5. 不想用手机配网？可以直接**从串口写配置**，见「联调检查清单」3.5（联调阶段推荐，脚本一条命令搞定）。

## MQTT 契约（与树莓派仓库的接口）

| Topic | 方向 | Payload 示例 |
|---|---|---|
| `ac/cmd` | 树莓派 → ESP32 | `{"power":1}` / `{"temp":24}` / `{"mode":1,"fan":2}` |
| `ac/state` | ESP32 → 树莓派（retained）| `{"online":true,"power":1,"mode":1,"temp":24,"fan":0,"brand":16}` |
| `ac/status` | ESP32 遗嘱（LWT）| `{"online":false}` |
| `ac/env` | ESP32 → 树莓派（retained）| `{"temp":26,"humidity":55,"ok":true}` |
| `ac/hb` | 树莓派 → ESP32（QoS0，不 retained）| `{"ts":1792078200}` |
| `ac/event` | ESP32 → 树莓派（retained）| `{"type":"failsafe_off","ts":1792078200,"prev":{...}}` |

字段取值：`power` 0/1；`mode` 自动0/制冷1/除湿2/送风3/制热4；`temp` 16~30；`fan` 自动0/低1/中2/高3。指令只带需要改的字段，越界值会被夹取到合法范围。

`env` / `event` 是 retained 的，broker 会把最后一条重放给新订阅者——**服务端要按
retained 标志和 `ts` 把"重放"和"刚发生"区分开**。`hb` 反过来必须**不** retained：
一个活得比进程还久的心跳是假的。

> 这是两个仓库的**唯一接口**。树莓派侧服务的完整规格（飞书卡片、按钮动作、时序、部署、验收）见 [`docs/rpi-feishu-interface-spec.md`](docs/rpi-feishu-interface-spec.md)。

## 串口协议

见 [`docs/protocol.md`](docs/protocol.md)（含 `0x30` 多参数 / `0x31` 单参数 / `0x10`,`0x12` 学习帧与校验算法与品牌码表）。

## 测试

协议编码与状态夹取是纯函数，可脱离硬件做原生单元测试：

```powershell
./scripts/run_native_tests.ps1
```

## 联调检查清单（接真机）

按顺序来，每步先确认「预期现象」再进下一步。任何一步不过，先停下修，不要往下叠。

> ✅ **已验证（2026-10-12，真机）**
> - ESP32 启动正常（`ESP32-D0WD-V3 rev3 @240MHz`，4MB flash）
> - **OLED**：I2C 扫描到 `0x3C`，显示正常（SH1106 驱动），屏幕上直接显示本机 IP
> - **红外模块**：UART 双向 OK，每条指令都回 ACK `68 08 00 00 01 9C 9D 16`（见 `docs/protocol.md`）
> - **美的1**：开机 / 温度 / 风速，**空调均正确响应**
> - **配网**：手机连 `AC-Gateway-Setup` → 配置页保存 → `[wifi] connected, IP=192.168.8.105`；断电 / 重新烧录后**自动重连，无需重配**（NVS 持久化）
> - **OTA**：`ac-gateway.local:3232`（mDNS 可用，后续可直接 OTA 升级）
>
> 尚缺：**MQTT 端到端**（等树莓派 broker 就绪）。

### 硬件自检固件（不依赖 WiFi，推荐先跑）

```powershell
python -m platformio run -e selftest -t upload   # 烧自检固件（端口自动探测）
python -m platformio device monitor              # 打开串口菜单
```

> **端口不写死**：板子的 COM 号**重新插拔就会变**（`COM16` 就是这样变成了 `COM11`）。
> PlatformIO 会自动探测；下面这些脚本也都自动探测（`scripts/_port.py`），
> 多个 USB 串口同时插着时会报错并让你显式指定，例如 `--upload-port COM11` /
> `-p COM11`，或设环境变量 `AC_PORT=COM11`。

串口菜单：`h` 帮助、`i` I2C 扫描、`o` OLED 测试图案、`r` 发参考帧、`p` 开关、
`t 24` 温度、`m 1` 模式、`f 0` 风速、`b 10` 品牌、`l/s` 学习/回放、`d` 状态。

也可以脚本一键抓取（自动复位 + 跑命令 + 打印结果）：`python scripts/hw_selftest.py`

> 自检固件是**独立环境** `[env:selftest]`，不影响主固件；恢复主固件用
> `python -m platformio run -e esp32dev -t upload`（板子上跑的是哪个环境由最后一次烧录决定）。
> 若 COM 口被占用（`PermissionError(13, '拒绝访问')`），先关掉 Arduino IDE 的串口监视器 / PlatformIO Serial Monitor。

### 0. 上电自检（只插 USB，不接外设）

| 步骤 | 操作 | 预期现象 |
|---|---|---|
| 0.1 | `pio run -t upload` 后开 `pio device monitor` | 打印 `[ac-gateway] boot`，无反复重启 |
| 0.2 | 观察串口 | 出现 `[wifi] connecting (portal: AC-Gateway-Setup)` |

### 1. OLED（先接屏，其余不接）

接线：`GND/VCC(3V3)/SCL→GPIO22/SDA→GPIO21`。

| 现象 | 结论 / 处理 |
|---|---|
| 4 行：`AirCon Gateway` / `AC ON  COOL  24C` / `Fan AUTO   Net OK` / `192.168.x.x` | ✅ 正常 |
| 首启（未配网）时显示 `connecting WiFi...` + `AP: AC-Gateway-Setup` / `pw: acgateway` | ✅ 正常（配网门户提示） |
| 第四行 `no IP` | 还没连上 WiFi |
| 串口 `[oled] no display answering -> running headless` | 总线上**没有任何屏应答**（没接 / 没供电 / 线松）。网关照常跑，只是不显示；接好线再上电即可 |
| 复位（刷机、按 reset）后像死机、串口停在 `[oled]` 附近 | 复位打断了 OLED 传输、从机压住总线。固件现在**开机先做一次总线恢复**（九拍时钟 + STOP）再接管 I2C，正常不会再出现 |
| 全黑，串口有 `[oled] using 0x3C` | 多为控制器不符：把 `src/config.h` 的 `OLED_SH1106` 改 `0` 重烧 |
| 串口 `[oled] using 0x3D` 或探测不到 | 检查 SDA/SCL 是否接反、是否 3.3V 供电 |
| 花屏 / 上半屏错位 | 典型 SH1106 参数不符，同上切换 `OLED_SH1106` |

### 2. 红外模块

接线：`ESP32 GPIO16(RX) ← 模块 TX`、`GPIO17(TX) → 模块 RX`、`VCC(3.3~5V)`、`GND`。

| 步骤 | 操作 | 预期现象 |
|---|---|---|
| 2.1 | 用手机摄像头对着红外发射管（摄像头能看到红外） | 连接正常后不会常亮，只在发码瞬间闪 |
| 2.2 | 给一次控制指令（见第 4 步） | 串口打印 `[ir] -> 68 0C 00 FF 30 10 ...` |
| 2.3 | 对着空调发码 | 空调响应（滴一声 / 改变状态） |
| 2.4 | 若空调无反应 | 确认品牌码：美的1 = `0x10`，可试 `0x11`/`0x12`（`docs/protocol.md` 品牌表） |

> 交叉验证：用厂家 IrStudy 软件生成同一品牌的空调编码，和本固件打印的帧逐字节比对。

### 3. 配网

| 步骤 | 操作 | 预期现象 |
|---|---|---|
| 3.1 | 手机连热点 `AC-Gateway-Setup`（密码 `acgateway`） | OLED 显示 `connecting WiFi...` + 热点名/密码；浏览器自动弹配置页，否则手动开 `192.168.4.1` |
| 3.2 | 选你家 **2.4GHz** WiFi、填密码（+ MQTT 树莓派 IP / 1883）后保存 | 热点消失，串口 `[wifi] connected, IP=192.168.x.x` |
| 3.3 | 断电再上电（或重新烧录） | 自动重连同一 IP，**不再开热点**（配置在 NVS）|

> ⚠️ ESP32 **只支持 2.4GHz**。若保存后连不上、串口反复重连，多半是连到了 5GHz 段或密码错。

### 3.4 配网逃生通道（`r` 恢复出厂）

配错网 / 换路由器连不上 / 串口出现 `4WAY_HANDSHAKE_TIMEOUT` 时，**上电后 2 秒内**从串口发一个字符 `r`：

```
[ac-gateway] send 'r' within 2s to reset WiFi+config ... RESET
[ac-gateway] WiFi + config cleared, rebooting
```

会清掉 WiFi 凭据 + 应用配置并重启，下次开机重新进入配网门户，**不用重新烧录**。

### 3.5 串口配网（免手机，联调推荐）

不想开手机热点配网时，可以直接从串口写配置：

```powershell
# 端口自动探测（多个 USB 串口时显式给，如 provision.py COM11）
# 从 Windows 已保存的 WiFi 档案取 SSID+密码（不会打印明文），再写 MQTT 地址
python scripts/provision.py --windows-profile HUAWEI-B311-98C3 --host 192.168.8.103

# 也可以直接给参数
python scripts/provision.py --ssid MyWiFi --pass 12345678 --host 192.168.8.103 --mqtt-port 1883
```

也可以手动敲（脚本会先复位板子；`--no-reset` 可跳过）：

```powershell
python scripts/serial_cmd.py "show" "scan"
```

| 命令 | 作用 |
|---|---|
| `scan` | 列出可见 WiFi（含信号强度，用来确认 2.4GHz 名称）|
| `show` | 打印当前配置（密码打码）|
| `set ssid\|pass\|host\|port\|user\|mqttpass\|prefix\|name\|brand <值>` | 改一个字段并立即存 NVS |
| `restart` | 重启（改完 `ssid`/`pass` 后**必须**发一次才会去连）|
| `reset` | 清 WiFi + 配置并重启（等同 3.4 的 `r`）|

> ⚠️ 配置台只在**上电后 20 秒内**开放。之后固件进入**阻塞的**配置门户，那时串口控制台不再运行 ——
> 所以 `provision.py` / `serial_cmd.py` 默认先复位板子抢这个窗口。
>
> 配置存在**我们自己的 NVS**里，开机优先用它直连；只有连不上才回落到配置门户。因此串口配好的板子
> 以后每次上电都直连，不再开热点。

### 4. MQTT 端到端

> ✅ **已完整验证（2026-10-13）**：树莓派 `192.168.8.103` ⇄ ESP32 `192.168.8.105`，下表 4.1–4.9 全过。
> 关键证据：拒绝类命令（坏 JSON / 未知字段 / 空 payload）串口回 `[cmd] ... ignored` 且**不发 IR、不动空调**；
> 接受类发出的 IR 帧与 `ac/state` 同步（`68 0C 00 FF 30 10 P 01 T 00 CK 16`，第 7 字节电源位、第 9 字节温度位，
> 例：开机 `...10 01 01 18...`、温度 22 `...01 16...`）；越界 `{"temp":99}`→30、`{"temp":0}`→16；
> 连续快发 18→22 共 5 帧、最终状态 = 最后一条；用 iptables 隔离 ESP32 约 25 s 后遗嘱 `{"online":false}` 触发，
> 规则移除后自动回到 `{"online":true}`。

树莓派上装好 Mosquitto（见另一仓库），用 `mosquitto_sub` / MQTTX 验证。

| 步骤 | 操作 | 预期现象 |
|---|---|---|
| 4.1 | 订阅 `ac/#` | 立刻收到 retained 的 `ac/state`，`online:true` |
| 4.2 | 看 `ac/status` | `{"online":true}` |
| 4.3 | 发 `{"power":1}` 到 `ac/cmd` | 空调开机 + OLED 变 `AC ON` + `ac/state` 更新为 `power:1` |
| 4.4 | 发 `{"temp":24,"mode":1}` 到 `ac/cmd` | 温度/模式同步，OLED 与 `ac/state` 一致 |
| 4.5 | 发越界值 `{"temp":99}` | 被夹到 `30`，不报错 |
| 4.6 | 发未知字段 `{"foo":1}` | 串口打印 `[cmd] unknown field 'foo' ignored`，状态不变 |
| 4.7 | 发非法 JSON `{` | 串口 `[cmd] bad json, ignored`，设备不崩 |
| 4.8 | 把 ESP32 断电 | 约一个心跳内 `ac/status` 变 `{"online":false}`（遗嘱） |
| 4.9 | 连续快发 3 条改温度的指令 | 最终 `ac/state` 等于**最后一条**的值 |

### 5. 常见故障速查

| 现象 | 可能原因 |
|---|---|
| 串口刷重启 | 供电不足（红外模块加屏电流较大，用独立 5V/3V3）或接线短路 |
| 烧录报 `could not open COMx ... 拒绝访问` | 串口被占用：关掉 Arduino IDE 的串口监视器 / PlatformIO Serial Monitor / PuTTY，或杀掉 `serial-monitor` 进程 |
| 首启屏幕全黑（但串口正常） | 早期固件把 OLED 初始化排在阻塞的配网之后；现已改为**先初始化屏幕**（≥ `56b4d20`）|
| 串口反复 `4WAY_HANDSHAKE_TIMEOUT`、热点迟迟不出现 | 板内残留连不上的 WiFi 凭据；**上电 2 秒内发 `r`** 恢复出厂（见 3.4）|
| OLED 显示 `Net MQTT?` | WiFi 通了但没有 broker（正常，等树莓派就绪）|
| `[mqtt] connect failed rc=-2` | 服务器地址/端口错，或防火墙挡 1883 |
| `rc=4` / `rc=5` | 用户名密码错（`rc=5` 未授权） |
| 状态与空调不一致 | 红外没打到（对准）、品牌码不对、或空调被遥控器改过（本固件不知道 → 发一次完整状态即可对齐） |
| 空调收到但方向/模式相反 | 该品牌码表变体问题，换美的 `0x11`/`0x12` 试 |

## 说明与限制

- 本固件发的是**多参数（0x30）**帧，一次下发完整状态，最稳。
- `0x31` 单参数与 `0x10/0x12` 学习帧编码器已提供（`ac_encoder`），主路径暂未使用，便于后续扩展。
- OLED 默认按 **SH1106** 驱动 1.3" 屏；0.96" SSD1306 请改 `src/config.h` 的 `OLED_SH1106` 为 `0`。
- 默认引脚：OLED SDA=21/SCL=22，红外模块 RX=16/TX=17（见 `src/pins.h`）。
- **网络**：只需 **2.4GHz** WiFi。IP 由路由器 DHCP 分配（实测 `192.168.8.105`），设备名 `ac-gateway`；建议在路由器上做 **IP 绑定**，或直接用 mDNS 名 `ac-gateway.local`。
- **OTA**：`ArduinoOTA` 已启用（`OTA_ENABLED=1`），端口 `3232`，可直接无线升级，不必每次插 USB。
- **恢复出厂**：上电 2 秒内串口发 `r` → 清 WiFi + 配置并重启（见「联调检查清单」3.4）。
