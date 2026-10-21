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
// Absolute cap on one portal visit, measured from when it opened. The idle
// timeout above restarts while a phone is associated with the setup AP, so on
// its own it would let a phone someone walked away from pin the gateway in
// configuration forever -- no MQTT, no AC control.
#define PORTAL_HARD_TIMEOUT_S 900
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

// ---- network watch-dog: a dropped link is ours to fix ----
//
// The access point (a 4G router) re-keys / drops its clients a few times a day.
// The ESP32's implicit station auto-reconnect does not always bring the link
// back, and until this existed the firmware had no runtime recovery at all --
// `net_loop()` simply returned while `WiFi.status() != WL_CONNECTED`. So a
// dropped link could stay down for hours: the broker log has the gateway
// silent for 4h15m and 7h40m while the same AP's other clients were back
// within seconds, and `ac/cmd` could not reach the AC for that whole window.
//
// We now watch the whole path ourselves (WiFi associated AND an MQTT session
// up -- both failure shapes break that, so we do not trust WiFi.status() alone)
// and escalate from the last healthy moment:
//
//   15s of silence -> one fresh WiFi dial. Deliberately shorter than the
//                     broker's 1.5 x keepalive = 22.5s window, so a successful
//                     dial usually beats "has exceeded timeout, disconnecting"
//                     and the card never even flashes offline.
//   every 15s      -> dial again. Not faster: an association is already in
//                     flight and hammering begin() only delays it.
//   10min          -> reboot. After ~40 failed dials nothing else is left, and
//                     the boot path (try_stored, then a portal bounded by
//                     PORTAL_HARD_TIMEOUT_S) is the last recovery we have. The
//                     fail-safe has already sent the IR "off" by then, so this
//                     cannot make the room colder than it already is.
#define NET_REDIAL_AFTER_MS 15000UL
#define NET_REDIAL_EVERY_MS 15000UL
#define NET_REBOOT_AFTER_MS 600000UL   // 10 minutes

// ---- NTP ----
// ac/event carries a wall-clock `ts` that the service checks for freshness, so
// the clock must be real. CST (UTC+8) with two nearby pools.
#define NTP_SERVER1 "ntp.aliyun.com"
#define NTP_SERVER2 "pool.ntp.org"
#define NTP_TZ_OFFSET_S (8 * 3600)
