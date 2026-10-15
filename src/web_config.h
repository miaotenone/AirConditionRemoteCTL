#pragma once
#include "config_store.h"

// Ensure WiFi is connected. If credentials are missing/wrong, start the
// WiFiManager captive portal (AP: WIFI_PORTAL_AP_NAME) so the user can enter
// WiFi + MQTT settings from a phone. Settings are persisted to NVS.
//
// The portal connects to whichever network the user picks, and that network is
// then written back to our own config -- otherwise the next boot would try the
// old one again and the portal would reappear forever.
void web_config_run(AppConfig& cfg);

// Wipe stored WiFi credentials and app config (used by the boot-time 'r'
// escape hatch and by a long press on the BOOT button) so the next boot opens
// a clean config portal.
void web_config_reset();

// True once, when the BOOT button (GPIO0) has been held down for
// RESET_HOLD_MS. Non-blocking: call it repeatedly from any loop that has to
// stay responsive (the WiFi connect window, the config portal window, loop()).
bool reset_button_hold_detected();
