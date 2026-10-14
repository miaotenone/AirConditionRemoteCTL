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
#define WIFI_STA_TIMEOUT_MS 20000UL  // try our own stored SSID before the portal

// ---- defaults (overridable via config portal / NVS) ----
#define DEFAULT_TOPIC_PREFIX "ac"
#define DEFAULT_DEVICE_NAME "ac-gateway"
#define DEFAULT_MQTT_PORT 1883

// ---- behaviour ----
#define STATE_HEARTBEAT_MS 30000UL  // re-publish retained state periodically
#define OTA_ENABLED 1

// ---- DHT11 room sensor ----
// DHT11 needs >= 1s between reads and is slow (~25ms); 30s is plenty for a
// room that changes over minutes, and it matches the service's recording rate.
#define DHT_READ_MS 30000UL

// ---- fail-safe: turn the AC off when we lose contact with the service ----
//
// The service publishes <prefix>/hb every HB_INTERVAL_S (30s, see the server's
// settings). Silence from EITHER the network/broker OR that heartbeat means the
// decisions have stopped, and an unattended AC is worse than a warm room, so
// after LINK_LOST_OFF_MS of no contact we send one IR "off" frame.
#define LINK_LOST_OFF_MS 300000UL      // 5 minutes (design §8)
#define SERVICE_HB_TIMEOUT_MS 90000UL  // no ac/hb for 90s -> service looks dead

// ---- NTP ----
// ac/event carries a wall-clock `ts` that the service checks for freshness, so
// the clock must be real. CST (UTC+8) with two nearby pools.
#define NTP_SERVER1 "ntp.aliyun.com"
#define NTP_SERVER2 "pool.ntp.org"
#define NTP_TZ_OFFSET_S (8 * 3600)
