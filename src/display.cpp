#include "display.h"
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <stdio.h>
#include <string.h>
#include "pins.h"
#include "config.h"
#include "version.h"

#if OLED_SH1106
static U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#else
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#endif

static uint8_t s_addr = OLED_I2C_ADDR;

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------
static const uint8_t W = 128;
static const uint8_t H = 64;
static const uint8_t HEADER_H = 12;

//: How long each page stays put, and how the swap is animated. Six 40 ms
//: frames = a ~240 ms slide, which reads as motion rather than a jump without
//: costing any visible latency.
#define DISPLAY_FRAME_MS 40UL
#define DISPLAY_ANIM_FRAMES 6

typedef void (*PageFn)(int16_t x);
static void page_aircon(int16_t x);
static void page_room(int16_t x);
static void page_link(int16_t x);
static void page_fw(int16_t x);

static const uint8_t PAGE_COUNT = 4;
static PageFn s_pages[PAGE_COUNT] = {page_aircon, page_room, page_link, page_fw};

static DisplayInfo s_info;
static bool s_ok = false; // an OLED answered the probe
static UiNavigation s_nav;
static uint8_t s_from = 0;
static uint8_t s_anim = 0; // frames left in the current slide
static bool s_dirty = true;
static bool s_manual = true; // display_lines() owns the screen
static uint32_t s_fp = 0;
static uint32_t s_lastFrame = 0;

//: Bound on a single I2C transaction. The default is unbounded enough that a
//: half-connected module turns every frame into seconds of stalling.
static const uint16_t OLED_I2C_TIMEOUT_MS = 50;

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------
static const char* mode_name(uint8_t m) {
    static const char* t[] = {"AUTO", "COOL", "DRY", "FAN", "HEAT"};
    return (m <= AC_MODE_HEAT) ? t[m] : "?";
}

static const char* fan_name(uint8_t f) {
    static const char* t[] = {"AUTO", "LOW", "MED", "HIGH"};
    return (f <= AC_FAN_HIGH) ? t[f] : "?";
}

// A common chrome keeps status and navigation in the same place on every page.
// Protection takes priority over the connection badge, never over control logic.
static void header(int16_t x, const char* title, uint8_t page) {
    u8g2.setDrawColor(1);
    u8g2.drawBox(x, 0, W, HEADER_H);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(x + 4, 9, title);
    const char* badge = s_info.failsafe ? "SAFE OFF" : s_info.service_alive ? "SVC OK" : "NO HB";
    u8g2.drawStr(x + W - 4 - (int16_t)u8g2.getStrWidth(badge), 9, badge);
    u8g2.setDrawColor(1);
    u8g2.drawHLine(x + 4, 54, W - 8);
    u8g2.drawStr(x + 76, 63, s_nav.diagnostics ? "DIAG" : s_nav.automatic ? "AUTO" : "HOLD");
    if (s_nav.diagnostics) {
        char index[8];
        snprintf(index, sizeof(index), "%u/5", (unsigned)s_nav.detail + 1);
        u8g2.drawStr(x + 110, 63, index);
    } else {
        for (uint8_t dot = 0; dot < PAGE_COUNT; ++dot) {
            const int16_t dx = x + 102 + dot * 7;
            if (dot + 1 == page)
                u8g2.drawBox(dx, 59, 4, 3);
            else
                u8g2.drawFrame(dx, 59, 4, 3);
        }
    }
}

static void footer(int16_t x, const char* text) {
    // Reserve the right side for AUTO/HOLD/DIAG and page indication.
    char short_text[15];
    snprintf(short_text, sizeof(short_text), "%.14s", text);
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(x + 4, 63, short_text);
}

// A big number followed by a "deg C" suffix. Returns nothing; kept here so
// both the AC and the room page read identically.
static void big_celsius(int16_t x, int16_t bx, int16_t baseline, int value) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", value);

    u8g2.setFont(u8g2_font_helvB18_tn);
    u8g2.drawStr(x + bx, baseline, buf);
    int16_t w = (int16_t)u8g2.getStrWidth(buf);

    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(x + bx + w + 4, baseline, "C");
    u8g2.drawCircle(x + bx + w + 6, baseline - 13, 1, U8G2_DRAW_ALL);
}

// The sensor failed: say so in plain words instead of showing a stale number.
static void no_sensor(int16_t x) {
    u8g2.drawRFrame(x + 6, 19, 23, 27, 4);
    u8g2.setFont(u8g2_font_7x14B_tf);
    u8g2.drawStr(x + 14, 38, "!");
    u8g2.drawStr(x + 36, 33, "NO DATA");
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(x + 36, 46, s_info.sensor_seen ? "CHECK SENSOR" : "NOT READ");
    footer(x, s_info.sensor_seen ? "DHT11 FAILED" : "WAIT DHT11");
}

// ---------------------------------------------------------------------------
// Pages
// ---------------------------------------------------------------------------
static void page_aircon(int16_t x) {
    header(x, "AIRCON", 1);
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(x + 6, 22, "SETPOINT");
    big_celsius(x, 6, 49, (int)s_info.ac.temp);

    bool on = s_info.ac.power != AC_POWER_OFF;
    if (on) {
        u8g2.drawRBox(x + 84, 18, 38, 22, 4);
        u8g2.setDrawColor(0);
    } else {
        u8g2.drawRFrame(x + 84, 18, 38, 22, 4);
    }
    u8g2.setFont(u8g2_font_7x14B_tf);
    const char* pw = on ? "ON" : "OFF";
    u8g2.drawStr(x + 84 + (38 - (int16_t)u8g2.getStrWidth(pw)) / 2, 34, pw);
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_5x7_tf);
    const char* mode = mode_name(s_info.ac.mode);
    u8g2.drawStr(x + 103 - (int16_t)u8g2.getStrWidth(mode) / 2, 50, mode);
    char row[26];
    snprintf(row, sizeof(row), "FAN %s", fan_name(s_info.ac.fan));
    footer(x, row);
}

static void page_room(int16_t x) {
    header(x, "ROOM", 2);

    if (!s_info.env_ok) {
        no_sensor(x);
        return;
    }

    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(x + 6, 22, "ROOM TEMP");
    big_celsius(x, 6, 49, s_info.room_c);
    u8g2.drawVLine(x + 79, 18, 32);
    u8g2.drawStr(x + 87, 23, "RH");
    u8g2.setFont(u8g2_font_7x14B_tf);
    char rh[8];
    snprintf(rh, sizeof(rh), "%d%%", s_info.humidity);
    u8g2.drawStr(x + 85, 40, rh);
    // This is a humidity gauge, not an invented comfort classification.
    const int humidity = s_info.humidity < 0 ? 0 : s_info.humidity > 100 ? 100 : s_info.humidity;
    u8g2.drawFrame(x + 85, 46, 36, 4);
    u8g2.drawBox(x + 86, 47, (uint8_t)(34 * humidity / 100), 2);
    char row[28];
    if (s_info.ac.power != AC_POWER_OFF) {
        // The useful question is "how far from the setpoint am I".
        snprintf(row, sizeof(row), "SET %u  d%+d", (unsigned)s_info.ac.temp,
                 s_info.room_c - (int)s_info.ac.temp);
    } else {
        snprintf(row, sizeof(row), "SET %u   --", (unsigned)s_info.ac.temp);
    }
    footer(x, row);
}

static void page_link(int16_t x) {
    header(x, "LINK", 3);

    u8g2.setFont(u8g2_font_5x7_tf);
    char row[32];

    snprintf(row, sizeof(row), "NETWORK  %s", s_info.net_state ? s_info.net_state : "?");
    u8g2.drawStr(x + 4, 23, row);

    const char* ip =
        (s_info.ip && s_info.ip[0] && strcmp(s_info.ip, "0.0.0.0") != 0) ? s_info.ip : "no IP";
    snprintf(row, sizeof(row), "IP %s", ip);
    u8g2.drawStr(x + 4, 35, row);

    // The heartbeat is the interesting one: WiFi can be fine while the bot
    // service that makes the decisions is gone.
    snprintf(row, sizeof(row), "HEARTBEAT %s", s_info.service_alive ? "OK" : "LOST");
    u8g2.drawStr(x + 4, 48, row);

    uint32_t s = s_info.uptime_s;
    snprintf(row, sizeof(row), "up %lud %02lu:%02lu", (unsigned long)(s / 86400UL),
             (unsigned long)((s / 3600UL) % 24UL), (unsigned long)((s / 60UL) % 60UL));
    footer(x, row);
}

// Which build is this? The one question the unit otherwise cannot answer: the
// LINK page shows live state and NVS holds the remembered AC state, but neither
// names the firmware. Values come from version.h (git revision injected at build
// time), so a reflashed board is identifiable by looking at it -- no cable.
static void page_fw(int16_t x) {
    header(x, "FIRMWARE", 4);
    u8g2.setFont(u8g2_font_5x7_tf);

    char row[32];
    snprintf(row, sizeof(row), "%s / OLED UI3", FW_TAG);
    u8g2.drawStr(x + 4, 23, row);
    u8g2.drawStr(x + 4, 35, FW_GIT_HASH);

    snprintf(row, sizeof(row), "built %s", __DATE__);
    u8g2.drawStr(x + 4, 48, row);

    snprintf(row, sizeof(row), "%s", __TIME__);
    footer(x, row);
}

// ---------------------------------------------------------------------------
// Change detection: redraw on data change, not every loop().
// ---------------------------------------------------------------------------
static void diagnostic_row(uint8_t y, const char* text) {
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(4, y, text);
}

static void age_row(uint8_t y, const char* label, uint32_t then) {
    char row[32];
    snprintf(row, sizeof(row), "%s %lus", label,
             (unsigned long)((uint32_t)(s_info.snapshot_ms - then) / 1000));
    diagnostic_row(y, row);
}

static void render_diagnostic() {
    static const char* titles[] = {"NETWORK", "HEARTBEAT", "SENSOR", "FAILSAFE", "BOOT/FW"};
    header(0, titles[s_nav.detail], 0);
    char row[40];
    switch (s_nav.detail) {
    case 0:
        snprintf(row, sizeof(row), "WIFI %s  MQTT %s", s_info.wifi_connected ? "UP" : "DOWN",
                 s_info.mqtt_connected ? "UP" : "DOWN");
        diagnostic_row(23, row);
        snprintf(row, sizeof(row), "IP %s",
                 s_info.ip && s_info.ip[0] && strcmp(s_info.ip, "0.0.0.0") != 0 ? s_info.ip : "--");
        diagnostic_row(35, row);
        snprintf(row, sizeof(row), "UP %lus", (unsigned long)s_info.uptime_s);
        diagnostic_row(48, row);
        footer(0, "READ ONLY");
        break;
    case 1:
        diagnostic_row(23, !s_info.heartbeat_seen ? "NOT RECEIVED"
                           : s_info.service_alive ? "ALIVE"
                                                  : "EXPIRED");
        if (s_info.heartbeat_seen)
            age_row(35, "LAST", s_info.heartbeat_last_ms);
        else
            diagnostic_row(35, "LAST --");
        snprintf(row, sizeof(row), "TIMEOUT %lus", (unsigned long)(SERVICE_HB_TIMEOUT_MS / 1000));
        diagnostic_row(48, row);
        footer(0, "ARRIVAL AGE");
        break;
    case 2:
        diagnostic_row(23, !s_info.sensor_seen ? "NOT READ"
                           : s_info.env_ok     ? "READ OK"
                                               : "READ FAILED");
        if (s_info.sensor_seen)
            age_row(35, "ATTEMPT", s_info.sensor_last_ms);
        else
            diagnostic_row(35, "ATTEMPT --");
        if (s_info.sensor_good_seen)
            age_row(48, "LAST GOOD", s_info.sensor_good_ms);
        else
            diagnostic_row(48, "LAST GOOD --");
        footer(0, "DHT11");
        break;
    case 3:
        diagnostic_row(23, s_info.failsafe         ? "TRIPPED"
                           : s_info.failsafe_armed ? "ARMED"
                                                   : "UNARMED");
        diagnostic_row(35, !s_info.failsafe_armed                          ? "NO CONTACT"
                           : s_info.mqtt_connected && s_info.service_alive ? "CONTACT OK"
                                                                           : "CONTACT LOST");
        if (s_info.failsafe_armed)
            age_row(48, "LAST", s_info.last_contact_ms);
        else
            diagnostic_row(48, "LAST --");
        snprintf(row, sizeof(row), "OFF %lus", (unsigned long)(LINK_LOST_OFF_MS / 1000));
        footer(0, row);
        break;
    case 4:
        snprintf(row, sizeof(row), "%u %s", s_info.boot_count,
                 s_info.boot_reason ? s_info.boot_reason : "?");
        diagnostic_row(23, row);
        diagnostic_row(35, FW_GIT_HASH);
        diagnostic_row(48, __DATE__);
        footer(0, __TIME__);
        break;
    }
}

static void mix_hash(uint32_t& h, uint32_t v) {
    h = (h ^ v) * 16777619u;
}

static uint32_t fingerprint(const DisplayInfo& i) {
    uint32_t h = 2166136261u;
    mix_hash(h, i.ac.power);
    mix_hash(h, i.ac.mode);
    mix_hash(h, i.ac.temp);
    mix_hash(h, i.ac.fan);
    mix_hash(h, i.env_ok ? 1u : 0u);
    mix_hash(h, (uint32_t)i.room_c);
    mix_hash(h, (uint32_t)i.humidity);
    mix_hash(h, i.service_alive ? 1u : 0u);
    mix_hash(h, i.failsafe ? 1u : 0u);
    mix_hash(h, i.uptime_s / 60u); // minute resolution: no 1 Hz churn
    mix_hash(h, i.wifi_connected);
    mix_hash(h, i.mqtt_connected);
    mix_hash(h, i.heartbeat_seen);
    mix_hash(h, i.heartbeat_last_ms);
    mix_hash(h, i.sensor_seen);
    mix_hash(h, i.sensor_last_ms);
    mix_hash(h, i.sensor_good_seen);
    mix_hash(h, i.sensor_good_ms);
    mix_hash(h, i.failsafe_armed);
    mix_hash(h, i.last_contact_ms);
    mix_hash(h, i.boot_count);
    if (s_nav.diagnostics) mix_hash(h, i.snapshot_ms / 1000u);
    for (const char* p = i.boot_reason; p && *p; ++p)
        mix_hash(h, (uint8_t)*p);
    for (const char* p = i.net_state; p && *p; ++p)
        mix_hash(h, (uint8_t)*p);
    for (const char* p = i.ip; p && *p; ++p)
        mix_hash(h, (uint8_t)*p);
    return h;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
// I2C bring-up
// ---------------------------------------------------------------------------
//: A *soft* reset -- esptool's RTS pulse, the reset button, an OTA reboot --
//: can land in the middle of an OLED transfer and leave the slave pulling a
//: line low; every later transaction then times out and the board looks dead
//: after a reflash. Clock the bus out by hand *before* Wire owns the pins:
//: nine SCL pulses walk a slave through a stuck bit, then a STOP returns the
//: bus to idle. Costs ~100 us on a healthy bus.
static void i2c_bus_recover() {
    pinMode(PIN_OLED_SDA, INPUT_PULLUP);
    pinMode(PIN_OLED_SCL, INPUT_PULLUP);
    delayMicroseconds(20);
    if (digitalRead(PIN_OLED_SDA) == HIGH && digitalRead(PIN_OLED_SCL) == HIGH) {
        return; // idle, leave it be
    }

    pinMode(PIN_OLED_SCL, OUTPUT_OPEN_DRAIN);
    for (int i = 0; i < 9; ++i) {
        digitalWrite(PIN_OLED_SCL, LOW);
        delayMicroseconds(5);
        digitalWrite(PIN_OLED_SCL, HIGH);
        delayMicroseconds(5);
    }

    // STOP: SDA rises while SCL is high.
    pinMode(PIN_OLED_SDA, OUTPUT_OPEN_DRAIN);
    digitalWrite(PIN_OLED_SDA, LOW);
    delayMicroseconds(5);
    digitalWrite(PIN_OLED_SCL, HIGH);
    delayMicroseconds(5);
    digitalWrite(PIN_OLED_SDA, HIGH);
    delayMicroseconds(5);

    pinMode(PIN_OLED_SDA, INPUT_PULLUP);
    pinMode(PIN_OLED_SCL, INPUT_PULLUP);
    Serial.println("[oled] i2c bus was stuck -> clocked it out");
}

//: Is anything answering at the usual addresses? The display is optional: a
//: loose cable, an unpowered module or no module at all must not stop the
//: gateway, so a silent bus disables the screen rather than letting u8g2 burn
//: seconds of timeouts on every frame.
static bool oled_probe() {
    const uint8_t candidates[2] = {0x3C, 0x3D};
    for (uint8_t a : candidates) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            s_addr = a;
            return true;
        }
    }
    return false;
}

void display_begin() {
    i2c_bus_recover();
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    Wire.setTimeOut(OLED_I2C_TIMEOUT_MS);

    if (!oled_probe()) {
        Serial.println("[oled] no display answering -> running headless");
        return;
    }
    Serial.printf("[oled] using 0x%02X\n", s_addr);

    u8g2.setI2CAddress(s_addr << 1); // U8g2 wants the 8-bit address
    u8g2.begin();
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.clearBuffer();
    u8g2.drawStr(0, 12, "AC Gateway booting");
    u8g2.sendBuffer();

    s_ok = true;
    s_nav.reset(millis());
}

void display_lines(const char* l1, const char* l2, const char* l3, const char* l4) {
    if (!s_ok) return;

    // Manual mode: boot chatter and the self-test own the screen until the
    // pager is handed data again.
    s_manual = true;
    u8g2.clearBuffer();
    u8g2.drawBox(0, 0, W, HEADER_H);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(4, 9, l1);
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(4, 29, l2);
    u8g2.drawHLine(4, 35, W - 8);
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(4, 47, l3);
    u8g2.drawStr(4, 60, l4);
    u8g2.sendBuffer();
}

void display_show_ac(const AcState& s, const char* net_state, const char* ip) {
    DisplayInfo info;
    info.ac = s;
    info.net_state = net_state;
    info.ip = ip;
    display_show_info(info);
}

void display_show_info(const DisplayInfo& info) {
    s_info = info;
    if (s_manual) {
        s_dirty = true;
        s_anim = 0;
        s_nav.restart_timer(millis());
    }
    s_manual = false;

    uint32_t fp = fingerprint(info);
    if (fp != s_fp) {
        s_fp = fp;
        s_dirty = true;
    }
}

void display_navigate(UiEvent event) {
    if (!s_ok || s_manual) return;
    s_nav.handle(event, millis());
    s_anim = 0;
    s_dirty = true;
}

// Prevent outgoing footer text or decorations bleeding into the incoming page.
static void render_page(uint8_t page, int16_t x) {
    const int16_t left = x < 0 ? 0 : x;
    const int16_t right = x + W > W ? W : x + W;
    if (left >= right) return;
    u8g2.setClipWindow(left, 0, right, H);
    s_pages[page](x);
    u8g2.setMaxClipWindow();
}

void display_tick() {
    if (!s_ok || s_manual) return;

    uint32_t now = millis();

    // Time to move on?
    const uint8_t previous = s_nav.page;
    if (s_anim == 0 && s_nav.tick(now)) {
        s_from = previous;
        s_anim = DISPLAY_ANIM_FRAMES;
    }

    if (s_anim == 0 && !s_dirty) return;
    if ((uint32_t)(now - s_lastFrame) < DISPLAY_FRAME_MS) return;
    s_lastFrame = now;

    if (s_anim > 0) {
        s_anim--;
        uint16_t done = (uint16_t)(DISPLAY_ANIM_FRAMES - s_anim);
        int16_t off = (int16_t)((uint32_t)done * W / DISPLAY_ANIM_FRAMES);

        u8g2.clearBuffer();
        render_page(s_from, (int16_t)(-off));
        render_page(s_nav.page, (int16_t)(W - off));
        u8g2.sendBuffer();
        if (s_anim == 0) s_dirty = false;
        return;
    }

    s_dirty = false;
    u8g2.clearBuffer();
    if (s_nav.diagnostics)
        render_diagnostic();
    else
        render_page(s_nav.page, 0);
    u8g2.sendBuffer();
}

uint8_t display_address() {
    return s_addr;
}

void display_test_patterns() {
    if (!s_ok) return;
    s_manual = true;

    // 1) all pixels on
    u8g2.clearBuffer();
    u8g2.drawBox(0, 0, 128, 64);
    u8g2.sendBuffer();
    delay(700);

    // 2) all off
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    delay(400);

    // 3) border + diagonals + label
    u8g2.clearBuffer();
    u8g2.drawFrame(0, 0, 128, 64);
    u8g2.drawLine(0, 0, 127, 63);
    u8g2.drawLine(127, 0, 0, 63);
    u8g2.drawStr(2, 12, "OLED OK");
    u8g2.sendBuffer();
    delay(700);

    // 4) text sample (checks font rendering / no garbling)
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(0, 12, "SELFTEST");
    u8g2.drawStr(0, 26, "1234567890");
    u8g2.drawStr(0, 40, "PWR COOL 24C");
    u8g2.drawStr(0, 54, "ABCDEFGHIJ");
    u8g2.sendBuffer();
    delay(500);
}
