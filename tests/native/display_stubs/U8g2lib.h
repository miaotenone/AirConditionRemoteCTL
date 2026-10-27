#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include <cstring>
constexpr int U8G2_R0 = 0, U8X8_PIN_NONE = 0, U8G2_DRAW_ALL = 0;
inline const uint8_t u8g2_font_6x12_tf[] = {6};
inline const uint8_t u8g2_font_5x7_tf[] = {5};
inline const uint8_t u8g2_font_7x14B_tf[] = {7};
inline const uint8_t u8g2_font_helvB18_tn[] = {14};
struct TextDraw {
    int x, y, width;
    std::string text;
};
struct U8g2Stub {
    std::vector<TextDraw> text;
    int char_width = 6, clips = 0;
    U8g2Stub(int, int) {}
    void setFont(const uint8_t* font) {
        char_width = *font;
    }
    int getStrWidth(const char* value) {
        return static_cast<int>(strlen(value)) * char_width;
    }
    void drawStr(int x, int y, const char* value) {
        text.push_back({x, y, getStrWidth(value), value});
    }
    void clearBuffer() {
        text.clear();
        clips = 0;
    }
    void sendBuffer() {}
    void setDrawColor(int) {}
    void setI2CAddress(int) {}
    void begin() {}
    void setClipWindow(int, int, int, int) {
        ++clips;
    }
    void setMaxClipWindow() {}
    template <typename... Args> void drawBox(Args...) {}
    template <typename... Args> void drawRBox(Args...) {}
    template <typename... Args> void drawRFrame(Args...) {}
    template <typename... Args> void drawFrame(Args...) {}
    template <typename... Args> void drawHLine(Args...) {}
    template <typename... Args> void drawVLine(Args...) {}
    template <typename... Args> void drawLine(Args...) {}
    template <typename... Args> void drawCircle(Args...) {}
    template <typename... Args> void drawDisc(Args...) {}
};
using U8G2_SH1106_128X64_NONAME_F_HW_I2C = U8g2Stub;
using U8G2_SSD1306_128X64_NONAME_F_HW_I2C = U8g2Stub;
