#pragma once
#include <stdint.h>
constexpr int INPUT_PULLUP = 0, OUTPUT_OPEN_DRAIN = 1, HIGH = 1, LOW = 0;
inline uint32_t test_now_ms = 10000;
inline int test_gpio_level[40] = {};
inline int test_gpio_mode[40] = {};
inline uint32_t millis() {
    return test_now_ms;
}
inline void pinMode(int pin, int mode) {
    test_gpio_mode[pin] = mode;
}
inline int digitalRead(int pin) {
    return test_gpio_level[pin];
}
inline void digitalWrite(int, int) {}
inline void delayMicroseconds(int) {}
inline void delay(int) {}
struct SerialStub {
    void println(const char*) {}
    template <typename... Args> void printf(const char*, Args...) {}
};
inline SerialStub Serial;
