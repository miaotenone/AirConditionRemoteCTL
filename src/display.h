#pragma once
#include "ac_state.h"

void display_begin();

// Four-line status screen (manual mode: paging is suppressed until a
// display_show_info() call hands the screen back to the pager).
void display_lines(const char* l1, const char* l2, const char* l3, const char* l4);

// Everything the OLED can show. Kept as a struct so the pages stay ignorant of
// where each field comes from (WiFi, MQTT, the DHT11, the fail-safe).
struct DisplayInfo {
    AcState ac;
    const char* net_state = "?";   // "OK" / "MQTT?" / "WiFi?"
    const char* ip        = "";
    bool   env_ok        = false;  // last DHT11 read succeeded
    int    room_c        = 0;
    int    humidity      = 0;
    bool   service_alive = false;  // a recent <prefix>/hb arrived
    bool   failsafe      = false;  // we turned the AC off because we lost them
    uint32_t uptime_s    = 0;
};

// Hand the pager the current data. Cheap: it only stores and marks dirty.
void display_show_info(const DisplayInfo& info);

// Drive the animation/paging. Call from loop() -- it self-throttles.
void display_tick();

// Convenience screen for the current AC + connection state (no sensor data).
void display_show_ac(const AcState& s, const char* net_state, const char* ip);

// --- bring-up helpers (used by the hardware self-test firmware) ---
uint8_t display_address();
void display_test_patterns();
