#include "web_config.h"
#include <Arduino.h>
#include <WiFiManager.h>
#include "config.h"

void web_config_run(AppConfig& cfg) {
    WiFiManager wm;
    wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_S);
    wm.setDebugOutput(false);

    // Custom fields shown on the config page.
    char host[64];   strncpy(host,   cfg.mqtt_host.c_str(),    sizeof(host) - 1);   host[sizeof(host) - 1] = '\0';
    char port[8];    snprintf(port, sizeof(port), "%u", (unsigned)cfg.mqtt_port);
    char user[48];   strncpy(user,   cfg.mqtt_user.c_str(),    sizeof(user) - 1);   user[sizeof(user) - 1] = '\0';
    char pass[48];   strncpy(pass,   cfg.mqtt_pass.c_str(),    sizeof(pass) - 1);   pass[sizeof(pass) - 1] = '\0';
    char prefix[32]; strncpy(prefix, cfg.topic_prefix.c_str(), sizeof(prefix) - 1); prefix[sizeof(prefix) - 1] = '\0';
    char dev[32];    strncpy(dev,    cfg.device_name.c_str(),  sizeof(dev) - 1);    dev[sizeof(dev) - 1] = '\0';
    char brand[8];   snprintf(brand, sizeof(brand), "0x%02X", cfg.brand);

    WiFiManagerParameter p_host("mqtt_host", "MQTT host", host, sizeof(host));
    WiFiManagerParameter p_port("mqtt_port", "MQTT port", port, sizeof(port));
    WiFiManagerParameter p_user("mqtt_user", "MQTT user", user, sizeof(user));
    WiFiManagerParameter p_pass("mqtt_pass", "MQTT pass", pass, sizeof(pass), "type=\"password\"");
    WiFiManagerParameter p_prefix("topic_prefix", "Topic prefix", prefix, sizeof(prefix));
    WiFiManagerParameter p_dev("device_name", "Device name", dev, sizeof(dev));
    WiFiManagerParameter p_brand("brand", "AC brand (hex)", brand, sizeof(brand));

    wm.addParameter(&p_host);
    wm.addParameter(&p_port);
    wm.addParameter(&p_user);
    wm.addParameter(&p_pass);
    wm.addParameter(&p_prefix);
    wm.addParameter(&p_dev);
    wm.addParameter(&p_brand);

    Serial.printf("[wifi] connecting (portal: %s)\n", WIFI_PORTAL_AP_NAME);
    Serial.printf("[wifi] if no saved network works, join AP '%s' (pw '%s') "
                  "and open http://192.168.4.1\n",
                  WIFI_PORTAL_AP_NAME, WIFI_PORTAL_AP_PASS);
    if (!wm.autoConnect(WIFI_PORTAL_AP_NAME, WIFI_PORTAL_AP_PASS)) {
        Serial.println("[wifi] portal timed out, restarting");
        delay(1000);
        ESP.restart();
    }
    Serial.printf("[wifi] connected, IP=%s\n", WiFi.localIP().toString().c_str());

    // Read the (possibly edited) values back.
    cfg.mqtt_host    = p_host.getValue();
    cfg.mqtt_port    = (uint16_t)atoi(p_port.getValue());
    cfg.mqtt_user    = p_user.getValue();
    cfg.mqtt_pass    = p_pass.getValue();
    cfg.topic_prefix = p_prefix.getValue();
    cfg.device_name  = p_dev.getValue();
    long b = strtol(p_brand.getValue(), nullptr, 16);
    if (b > 0) cfg.brand = (uint8_t)b;

    if (cfg.topic_prefix.isEmpty()) cfg.topic_prefix = DEFAULT_TOPIC_PREFIX;
    if (cfg.device_name.isEmpty())  cfg.device_name  = DEFAULT_DEVICE_NAME;

    WiFi.setHostname(cfg.device_name.c_str());
    config_store_save(cfg);
}

void web_config_reset() {
    WiFiManager wm;
    wm.setDebugOutput(false);
    wm.resetSettings();      // forget the saved AP / password
    config_store_clear();    // forget MQTT + app settings
}
