#pragma once

// ---- 1.3" OLED (I2C, 4-pin: VCC / GND / SCL / SDA) ----
#define PIN_OLED_SDA 21
#define PIN_OLED_SCL 22

// ---- IR module (UART TTL, 115200 8N1) ----
// ESP32 RX  <- module TX
// ESP32 TX  -> module RX
#define PIN_IR_RX 16
#define PIN_IR_TX 17

// ---- DHT11 room sensor (single-wire; needs a 4.7k~10k pull-up to 3V3) ----
// GPIO4 is a plain input/output pin with no strapping or boot role, so it is
// safe to hold a sensor on it. See docs/wiring.md for the wiring table.
#define PIN_DHT 4
