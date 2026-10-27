#pragma once
#include <stdint.h>
constexpr int INPUT_PULLUP = 0, OUTPUT_OPEN_DRAIN = 1, HIGH = 1, LOW = 0;
inline uint32_t millis() {
    return 10000;
}
inline void pinMode(int, int) {}
inline int digitalRead(int) {
    return HIGH;
}
inline void digitalWrite(int, int) {}
inline void delayMicroseconds(int) {}
inline void delay(int) {}
struct SerialStub {
    void println(const char*) {}
    template <typename... Args> void printf(const char*, Args...) {}
};
inline SerialStub Serial;
