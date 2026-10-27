#pragma once
#include <stdint.h>

// Decision state only. The main loop owns IR, NVS and event-delivery side effects.
struct FailsafeStatus {
    bool armed = false;
    bool tripped = false;
    uint32_t last_contact_ms = 0;
};

enum class FailsafeAction { none, arm, restore, turn_off };

// healthy means BOTH MQTT connectivity and a fresh service heartbeat.
// Uses 32-bit unsigned elapsed time, matching millis() on ESP32 and on hosts.
FailsafeAction failsafe_action(const FailsafeStatus& status, bool healthy, uint32_t now_ms,
                               uint32_t timeout_ms);
