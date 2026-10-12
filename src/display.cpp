#include "display.h"
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "pins.h"
#include "config.h"

#if OLED_SH1106
static U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#else
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#endif

static uint8_t s_addr = OLED_I2C_ADDR;

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
}

void display_lines(const char* l1, const char* l2, const char* l3, const char* l4) {
    u8g2.clearBuffer();
    u8g2.drawStr(0, 12, l1);
    u8g2.drawStr(0, 28, l2);
    u8g2.drawStr(0, 44, l3);
    u8g2.drawStr(0, 60, l4);
    u8g2.sendBuffer();
}

void display_show_ac(const AcState& s, const char* net_state, const char* ip) {
    static const char* modes[] = {"AUTO", "COOL", "DRY", "FAN", "HEAT"};
    static const char* fans[]  = {"AUTO", "LOW", "MED", "HIGH"};
    const char* mode = (s.mode <= AC_MODE_HEAT) ? modes[s.mode] : "?";
    const char* fan  = (s.fan  <= AC_FAN_HIGH) ? fans[s.fan]  : "?";

    char l2[24], l3[24];
    snprintf(l2, sizeof(l2), "AC %s  %s  %uC",
             s.power ? "ON" : "OFF", mode, (unsigned)s.temp);
    snprintf(l3, sizeof(l3), "Fan %s   Net %s", fan, net_state);

    // Show the real IP so the user can find the device without a serial log.
    const char* ipstr = (ip && ip[0] && strcmp(ip, "0.0.0.0") != 0) ? ip : "no IP";

    display_lines("AirCon Gateway", l2, l3, ipstr);
}

uint8_t display_address() {
    return s_addr;
}

void display_test_patterns() {
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
