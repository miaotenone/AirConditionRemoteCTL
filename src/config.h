#pragma once
#include <stdint.h>

// ---- IR module serial ----
#define IR_BAUD 115200

// ---- OLED ----
// 1.3" modules are usually SH1106; 0.96" are usually SSD1306.
// Set to 1 for SH1106, 0 to use SSD1306.
#define OLED_SH1106 1
#define OLED_I2C_ADDR 0x3C  // fall back to 0x3D is auto-detected at boot

// ---- WiFiManager config portal ----
#define WIFI_PORTAL_AP_NAME "AC-Gateway-Setup"
#define WIFI_PORTAL_AP_PASS "acgateway"   // must be >= 8 chars
#define WIFI_PORTAL_TIMEOUT_S 180

// ---- defaults (overridable via config portal / NVS) ----
#define DEFAULT_TOPIC_PREFIX "ac"
#define DEFAULT_DEVICE_NAME "ac-gateway"
#define DEFAULT_MQTT_PORT 1883

// ---- behaviour ----
#define STATE_HEARTBEAT_MS 30000UL  // re-publish retained state periodically
#define OTA_ENABLED 1
