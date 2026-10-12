#include "net_mqtt.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

static WiFiClient   s_wifiClient;
static PubSubClient s_mqtt(s_wifiClient);
static AppConfig    s_cfg;
static AcCommandHandler s_handler = nullptr;
static uint32_t     s_lastAttempt = 0;
static String       s_topicCmd, s_topicState, s_topicStatus;

static void mqtt_callback(char* topic, byte* payload, unsigned int len) {
    if (s_handler) {
        s_handler(reinterpret_cast<const char*>(payload), (size_t)len);
    }
}

void net_set_command_handler(AcCommandHandler handler) {
    s_handler = handler;
}

static void build_topics(const AppConfig& cfg) {
    s_topicCmd    = cfg.topic_prefix + "/cmd";
    s_topicState  = cfg.topic_prefix + "/state";
    s_topicStatus = cfg.topic_prefix + "/status";
}

static bool mqtt_connect() {
    String clientId = s_cfg.device_name + "-" +
                      String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);
    const char* user = s_cfg.mqtt_user.length() ? s_cfg.mqtt_user.c_str() : nullptr;
    const char* pass = s_cfg.mqtt_pass.length() ? s_cfg.mqtt_pass.c_str() : nullptr;
    const char* willMsg = "{\"online\":false}";

    bool ok;
    if (user) {
        ok = s_mqtt.connect(clientId.c_str(), user, pass,
                            s_topicStatus.c_str(), 1, true, willMsg);
    } else {
        ok = s_mqtt.connect(clientId.c_str(),
                            s_topicStatus.c_str(), 1, true, willMsg);
    }
    if (ok) {
        Serial.println("[mqtt] connected");
        s_mqtt.subscribe(s_topicCmd.c_str());
        s_mqtt.publish(s_topicStatus.c_str(), "{\"online\":true}", true);
    } else {
        Serial.printf("[mqtt] connect failed rc=%d\n", s_mqtt.state());
    }
    return ok;
}

void net_begin(const AppConfig& cfg) {
    s_cfg = cfg;
    build_topics(cfg);

    if (WiFi.status() != WL_CONNECTED && cfg.wifi_ssid.length()) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(cfg.wifi_ssid.c_str(), cfg.wifi_pass.c_str());
    }

    s_mqtt.setServer(cfg.mqtt_host.c_str(), cfg.mqtt_port);
    s_mqtt.setBufferSize(512);
    s_mqtt.setCallback(mqtt_callback);

    if (WiFi.status() == WL_CONNECTED && cfg.mqtt_host.length()) {
        mqtt_connect();
    }
}

bool net_connected() {
    return WiFi.status() == WL_CONNECTED && s_mqtt.connected();
}

void net_loop() {
    if (WiFi.status() != WL_CONNECTED) return;

    if (!s_mqtt.connected()) {
        uint32_t now = millis();
        if (now - s_lastAttempt > 5000 || s_lastAttempt == 0) {
            s_lastAttempt = now;
            if (s_cfg.mqtt_host.length()) mqtt_connect();
        }
    }
    s_mqtt.loop();
}

void net_publish_state(const AppConfig& cfg, const AcState& state, bool online) {
    if (!s_mqtt.connected()) return;

    StaticJsonDocument<256> doc;
    doc["online"] = online;
    doc["power"]  = state.power;
    doc["mode"]   = state.mode;
    doc["temp"]   = state.temp;
    doc["fan"]    = state.fan;
    doc["brand"]  = cfg.brand;

    char buf[160];
    size_t n = serializeJson(doc, buf, sizeof(buf));
    s_mqtt.publish(s_topicState.c_str(), reinterpret_cast<const uint8_t*>(buf), n, true);
}
