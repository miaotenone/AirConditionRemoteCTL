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

红外模块：TTL 串口，115200 8N1，38KHz 载波，遥控距离约 5~8m。

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
