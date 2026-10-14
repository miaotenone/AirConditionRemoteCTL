#include "display.h"
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <stdio.h>
#include <string.h>
#include "pins.h"
#include "config.h"

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
static const uint8_t HEADER_H = 11;

//: How long each page stays put, and how the swap is animated. Six 40 ms
//: frames = a ~240 ms slide, which reads as motion rather than a jump without
//: costing any visible latency.
#define DISPLAY_PAGE_MS    5000UL
#define DISPLAY_FRAME_MS   40UL
#define DISPLAY_ANIM_FRAMES 6

typedef void (*PageFn)(int16_t x);
static void page_aircon(int16_t x);
static void page_room(int16_t x);
static void page_link(int16_t x);

static const uint8_t PAGE_COUNT = 3;
static PageFn s_pages[PAGE_COUNT] = { page_aircon, page_room, page_link };

static DisplayInfo s_info;
static uint8_t  s_page        = 0;
static uint8_t  s_from        = 0;
static uint8_t  s_anim        = 0;   // frames left in the current slide
static bool     s_dirty       = true;
static bool     s_manual      = true;  // display_lines() owns the screen
static uint32_t s_fp          = 0;
static uint32_t s_lastSwitch  = 0;
static uint32_t s_lastFrame   = 0;

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

// An inverted title bar: a filled box with knocked-out text. Drawn at an x
// offset so the same page can slide in/out.
static void header(int16_t x, const char* title, uint8_t page) {
    u8g2.setDrawColor(1);
    u8g2.drawBox(x, 0, W, HEADER_H);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(x + 3, HEADER_H - 2, title);

    char right[8];
    snprintf(right, sizeof(right), "%u/%u", (unsigned)page, (unsigned)PAGE_COUNT);
    u8g2.drawStr(x + W - 3 - (int16_t)u8g2.getStrWidth(right), HEADER_H - 2, right);
    u8g2.setDrawColor(1);
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
    u8g2.setFont(u8g2_font_7x14B_tf);
    u8g2.drawStr(x + 8, 40, "no signal");

    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(x + 8, 58, "DHT11 read failed");
}

// ---------------------------------------------------------------------------
// Pages
// ---------------------------------------------------------------------------
static void page_aircon(int16_t x) {
    header(x, "AIRCON", 1);
    big_celsius(x, 8, 44, (int)s_info.ac.temp);

    bool on = s_info.ac.power != AC_POWER_OFF;
    u8g2.drawRFrame(x + 84, 20, 38, 26, 6);
    u8g2.setFont(u8g2_font_7x14B_tf);
    const char* pw = on ? "ON" : "OFF";
    u8g2.drawStr(x + 84 + (38 - (int16_t)u8g2.getStrWidth(pw)) / 2, 39, pw);

    u8g2.drawHLine(x + 4, 48, W - 8);

    u8g2.setFont(u8g2_font_6x12_tf);
    char row[26];
    snprintf(row, sizeof(row), "MODE %s", mode_name(s_info.ac.mode));
    u8g2.drawStr(x + 4, 60, row);

    snprintf(row, sizeof(row), "FAN %s", fan_name(s_info.ac.fan));
    u8g2.drawStr(x + W - 4 - (int16_t)u8g2.getStrWidth(row), 60, row);
}

static void page_room(int16_t x) {
    header(x, "ROOM", 2);

    if (!s_info.env_ok) {
        no_sensor(x);
        return;
    }

    big_celsius(x, 8, 44, s_info.room_c);

    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(x + 96, 26, "RH");
    u8g2.setFont(u8g2_font_7x14B_tf);
    char rh[8];
    snprintf(rh, sizeof(rh), "%d%%", s_info.humidity);
    u8g2.drawStr(x + 86, 42, rh);

    u8g2.drawHLine(x + 4, 48, W - 8);

    u8g2.setFont(u8g2_font_6x12_tf);
    char row[28];
    if (s_info.ac.power != AC_POWER_OFF) {
        // The useful question is "how far from the setpoint am I".
        snprintf(row, sizeof(row), "SET %u  d%+d",
                 (unsigned)s_info.ac.temp, s_info.room_c - (int)s_info.ac.temp);
    } else {
        snprintf(row, sizeof(row), "SET %u   --", (unsigned)s_info.ac.temp);
    }
    u8g2.drawStr(x + 4, 60, row);
}

static void page_link(int16_t x) {
    header(x, "LINK", 3);

    u8g2.setFont(u8g2_font_6x12_tf);
    char row[32];

    snprintf(row, sizeof(row), "WiFi/MQTT %s", s_info.net_state);
    u8g2.drawStr(x + 4, 22, row);

    const char* ip = (s_info.ip && s_info.ip[0] && strcmp(s_info.ip, "0.0.0.0") != 0)
                     ? s_info.ip : "no IP";
    snprintf(row, sizeof(row), "IP %s", ip);
    u8g2.drawStr(x + 4, 35, row);

    // The heartbeat is the interesting one: WiFi can be fine while the bot
    // service that makes the decisions is gone.
    snprintf(row, sizeof(row), "SVC HB %s", s_info.service_alive ? "OK" : "LOST");
    u8g2.drawStr(x + 4, 48, row);

    uint32_t s = s_info.uptime_s;
    snprintf(row, sizeof(row), "up %lud %02lu:%02lu",
             (unsigned long)(s / 86400UL),
             (unsigned long)((s / 3600UL) % 24UL),
             (unsigned long)((s / 60UL) % 60UL));
    u8g2.drawStr(x + 4, 60, row);

    if (s_info.failsafe) {
        u8g2.setFont(u8g2_font_5x7_tf);
        const char* m = "FAILSAFE";
        u8g2.drawStr(x + W - 4 - (int16_t)u8g2.getStrWidth(m), 60, m);
    }
}

// ---------------------------------------------------------------------------
// Change detection: redraw on data change, not every loop().
// ---------------------------------------------------------------------------
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
    mix_hash(h, i.uptime_s / 60u);   // minute resolution: no 1 Hz churn
    for (const char* p = i.net_state; p && *p; ++p) mix_hash(h, (uint8_t)*p);
    for (const char* p = i.ip; p && *p; ++p) mix_hash(h, (uint8_t)*p);
    return h;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void display_begin() {
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);

    // Probe the two common addresses.
    const uint8_t candidates[2] = {0x3C, 0x3D};
    for (uint8_t a : candidates) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) { s_addr = a; break; }
    }
    Serial.printf("[oled] using 0x%02X\n", s_addr);

    u8g2.setI2CAddress(s_addr << 1);  // U8g2 wants the 8-bit address
    u8g2.begin();
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.clearBuffer();
    u8g2.drawStr(0, 12, "AC Gateway booting");
    u8g2.sendBuffer();

    s_lastSwitch = millis();
}

void display_lines(const char* l1, const char* l2, const char* l3, const char* l4) {
    // Manual mode: boot chatter and the self-test own the screen until the
    // pager is handed data again.
    s_manual = true;
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.clearBuffer();
    u8g2.drawStr(0, 12, l1);
    u8g2.drawStr(0, 28, l2);
    u8g2.drawStr(0, 44, l3);
    u8g2.drawStr(0, 60, l4);
    u8g2.sendBuffer();
}

void display_show_ac(const AcState& s, const char* net_state, const char* ip) {
    DisplayInfo info;
    info.ac        = s;
    info.net_state = net_state;
    info.ip        = ip;
    display_show_info(info);
}

void display_show_info(const DisplayInfo& info) {
    s_info   = info;
    s_manual = false;

    uint32_t fp = fingerprint(info);
    if (fp != s_fp) {
        s_fp    = fp;
        s_dirty = true;
    }
}

void display_tick() {
    if (s_manual) return;

    uint32_t now = millis();

    // Time to move on?
    if (s_anim == 0 && (uint32_t)(now - s_lastSwitch) >= DISPLAY_PAGE_MS) {
        s_from       = s_page;
        s_page       = (uint8_t)((s_page + 1) % PAGE_COUNT);
        s_lastSwitch = now;
        s_anim       = DISPLAY_ANIM_FRAMES;
    }

    if (s_anim == 0 && !s_dirty) return;
    if ((uint32_t)(now - s_lastFrame) < DISPLAY_FRAME_MS) return;
    s_lastFrame = now;

    if (s_anim > 0) {
        s_anim--;
        uint16_t done = (uint16_t)(DISPLAY_ANIM_FRAMES - s_anim);
        int16_t  off  = (int16_t)((uint32_t)done * W / DISPLAY_ANIM_FRAMES);

        u8g2.clearBuffer();
        s_pages[s_from]((int16_t)(-off));          // slides out to the left
        s_pages[s_page]((int16_t)(W - off));       // and the new one follows
        u8g2.sendBuffer();
        if (s_anim == 0) s_dirty = false;
        return;
    }

    s_dirty = false;
    u8g2.clearBuffer();
    s_pages[s_page](0);
    u8g2.sendBuffer();
}

uint8_t display_address() {
    return s_addr;
}

void display_test_patterns() {
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
