#pragma once
#include "config_store.h"

// Ensure WiFi is connected. If credentials are missing/wrong, start the
// WiFiManager captive portal (AP: WIFI_PORTAL_AP_NAME) so the user can enter
// WiFi + MQTT settings from a phone. Settings are persisted to NVS.
void web_config_run(AppConfig& cfg);
