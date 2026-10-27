#pragma once
struct WireStub {
    void begin(int, int) {}
    void setTimeOut(int) {}
    void beginTransmission(int) {}
    int endTransmission() {
        return 0;
    }
};
inline WireStub Wire;
