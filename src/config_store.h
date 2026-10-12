#pragma once
#include <Arduino.h>
#include "config.h"
#include "ac_state.h"

// Everything that is configurable at runtime and persisted to NVS.
struct AppConfig {
    String   wifi_ssid;
    String   wifi_pass;
    String   mqtt_host;
    uint16_t mqtt_port    = DEFAULT_MQTT_PORT;
    String   mqtt_user;
    String   mqtt_pass;
    String   topic_prefix = DEFAULT_TOPIC_PREFIX;
    String   device_name  = DEFAULT_DEVICE_NAME;
    uint8_t  brand        = BRAND_MIDEA1;
    AcState  initial;
};

// Returns false when nothing has been stored yet (first boot).
bool config_store_load(AppConfig& cfg);

// Persist the current configuration.
void config_store_save(const AppConfig& cfg);
