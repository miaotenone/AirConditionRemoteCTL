#include "config_store.h"
#include "state_store.h"
#include <Preferences.h>
#include <ArduinoJson.h>

static Preferences s_prefs;

bool config_store_load(AppConfig& cfg) {
    // Open read-write: on first boot the namespace does not exist yet, and a
    // read-only open would log a scary "nvs_open failed: NOT_FOUND".
    s_prefs.begin("acctl", false);
    bool has = s_prefs.isKey("cfg");
    String json = has ? s_prefs.getString("cfg", "") : String();
    s_prefs.end();
    if (!has || json.isEmpty()) {
        Serial.println("[cfg] no stored config yet (first boot)");
        return false;
    }

    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, json)) return false;

    cfg.wifi_ssid    = doc["wifi_ssid"]    | cfg.wifi_ssid;
    cfg.wifi_pass    = doc["wifi_pass"]    | cfg.wifi_pass;
    cfg.mqtt_host    = doc["mqtt_host"]    | cfg.mqtt_host;
    cfg.mqtt_port    = doc["mqtt_port"]    | cfg.mqtt_port;
    cfg.mqtt_user    = doc["mqtt_user"]    | cfg.mqtt_user;
    cfg.mqtt_pass    = doc["mqtt_pass"]    | cfg.mqtt_pass;
    cfg.topic_prefix = doc["topic_prefix"] | cfg.topic_prefix;
    cfg.device_name  = doc["device_name"]  | cfg.device_name;
    cfg.brand        = doc["brand"]        | cfg.brand;
    cfg.initial.power = doc["init_power"]  | cfg.initial.power;
    cfg.initial.mode  = doc["init_mode"]   | cfg.initial.mode;
    cfg.initial.temp  = doc["init_temp"]   | cfg.initial.temp;
    cfg.initial.fan   = doc["init_fan"]    | cfg.initial.fan;
    ac_state_clamp(cfg.initial);
    return true;
}

void config_store_save(const AppConfig& cfg) {
    DynamicJsonDocument doc(1024);
    doc["wifi_ssid"]    = cfg.wifi_ssid;
    doc["wifi_pass"]    = cfg.wifi_pass;
    doc["mqtt_host"]    = cfg.mqtt_host;
    doc["mqtt_port"]    = cfg.mqtt_port;
    doc["mqtt_user"]    = cfg.mqtt_user;
    doc["mqtt_pass"]    = cfg.mqtt_pass;
    doc["topic_prefix"] = cfg.topic_prefix;
    doc["device_name"]  = cfg.device_name;
    doc["brand"]        = cfg.brand;
    doc["init_power"]   = cfg.initial.power;
    doc["init_mode"]    = cfg.initial.mode;
    doc["init_temp"]    = cfg.initial.temp;
    doc["init_fan"]     = cfg.initial.fan;

    String json;
    serializeJson(doc, json);
    s_prefs.begin("acctl", false);
    s_prefs.putString("cfg", json);
    s_prefs.end();
}

void config_store_clear() {
    s_prefs.begin("acctl", false);
    if (s_prefs.isKey("cfg")) s_prefs.remove("cfg");
    s_prefs.end();
    state_store_clear();   // the remembered AC state is part of "forget it all"
}
