#include "ir_module.h"
#include <Arduino.h>
#include "pins.h"
#include "config.h"

void ir_module_begin() {
    Serial2.begin(IR_BAUD, SERIAL_8N1, PIN_IR_RX, PIN_IR_TX);
}

void ir_module_send(const uint8_t* frame, size_t len) {
    Serial2.write(frame, len);
    Serial2.flush();

    Serial.print("[ir] ->");
    for (size_t i = 0; i < len; ++i) {
        Serial.printf(" %02X", frame[i]);
    }
    Serial.println();
}
