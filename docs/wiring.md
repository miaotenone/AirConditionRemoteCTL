# 接线表

## ESP32-WROOM-32E

| ESP32 引脚 | 连接到 | 说明 |
|---|---|---|
| GPIO21 | OLED SDA | I2C 数据 |
| GPIO22 | OLED SCL | I2C 时钟 |
| 3V3 | OLED VCC | 1.3" OLED 供电（3.3V） |
| GND | OLED GND | 共地 |
| GPIO16 (RX) | 红外模块 TX | ESP32 接收 |
| GPIO17 (TX) | 红外模块 RX | ESP32 发送 |
| 5V/VIN 或 3V3 | 红外模块 VCC | 模块 3.3~5V |
| GND | 红外模块 GND | 共地 |
| GPIO4 | DHT11 DATA | 室温/湿度传感器（见下）|
| 3V3 | DHT11 VCC | DHT11 供电（3.3V）|
| GND | DHT11 GND | 共地 |

红外模块：TTL 串口，115200 8N1，38KHz 载波，遥控距离约 5~8m。

## DHT11 室温传感器

用于闭环夜间控制（设计 §6.1）：固件每 `DHT_READ_MS`（30s）读一次，把
`{temp,humidity,ok}` 发到 `<prefix>/env`；**决策在树莓派上做**，固件只负责测量。

### 接线

| DHT11 引脚 | 连接到 | 说明 |
|---|---|---|
| VCC / +(第 1 脚) | 3V3 | 3.3V；模块版多为中间脚 |
| DATA / OUT / S | **GPIO4** | 单总线数据 |
| NC | — | 空脚，不接 |
| GND / -(第 4 脚) | GND | 共地 |

**必须加 4.7kΩ~10kΩ 上拉电阻**：DATA ↔ 3V3。
买带小 PCB 的 DHT11 模块（3 脚：VCC/DATA/GND）的话，板上已经带了这个电阻，
直插即可；裸传感器（4 脚）一定要自己加上拉，否则读数会时好时坏。

- **为什么是 GPIO4**：它没有 strapping / 启动特殊功能，也不是输入专用脚，
  上电瞬间不会把数据线拉到影响启动的电平，是最省心的选择。
- 引脚在 `src/pins.h` 的 `PIN_DHT` 中定义，改一行即可。
- 上电后 1s 内不要读；固件把第一次读取放在启动约 5s 之后。
- 读数失败时固件发 `ok:false`（并打串口日志），服务端会当"没有数据"处理，
  不会拿它去调温度。

> DHT11 精度有限（1℃ 分辨率、±2℃ 误差、±5% RH），所以夜间闭环用了
> ±1℃ 死区 + 最小动作间隔，而不是见差就调（设计 §7.2）。

## 1.3" 4 脚 OLED

4 个引脚依次为 **GND / VCC / SCL / SDA**（不同批次顺序可能不同，以丝印为准）。I2C 地址通常为 `0x3C`，少数为 `0x3D`，固件会在启动时自动探测。

- 默认控制器：**SH1106**（1.3" 常见）。若为 0.96" SSD1306，把 `src/config.h` 里 `OLED_SH1106` 改为 `0`。

## 引脚可改

全部在 `src/pins.h` 中定义：

```c
#define PIN_OLED_SDA 21
#define PIN_OLED_SCL 22
#define PIN_IR_RX 16
#define PIN_IR_TX 17
```
