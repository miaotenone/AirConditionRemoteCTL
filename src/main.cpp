#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>

#include "config.h"
#include "pins.h"
#include "ac_encoder.h"
#include "ac_state.h"
#include "ir_module.h"
#include "display.h"
#include "config_store.h"
#include "web_config.h"
#include "net_mqtt.h"

#if OTA_ENABLED
#include <ArduinoOTA.h>
#endif

static AppConfig g_cfg;
static AcState   g_state;

static volatile bool g_cmd_pending = false;
static char    g_cmd_buf[256];
static size_t  g_cmd_len = 0;
static char    g_last_action[24] = "boot";

static const char* net_state_name() {
    if (net_connected()) return "OK";
    if (WiFi.status() == WL_CONNECTED) return "MQTT?";
    return "WiFi?";
}

static void refresh_display() {
    display_show_ac(g_state, net_state_name(), WiFi.localIP().toString().c_str());
}

// Copy the incoming command out of the MQTT callback (which is not
// null-terminated and runs inside the client loop) for later processing.
static void on_command(const char* payload, size_t len) {
    if (len >= sizeof(g_cmd_buf)) len = sizeof(g_cmd_buf) - 1;
    memcpy(g_cmd_buf, payload, len);
    g_cmd_buf[len] = '\0';
    g_cmd_len = len;
    g_cmd_pending = true;
}

static void apply_pending_command() {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, g_cmd_buf, g_cmd_len)) {
        Serial.println("[cmd] bad json, ignored");
        return;
    }
    JsonObject obj = doc.as<JsonObject>();
    if (obj.isNull()) return;

    bool changed = false;
    for (JsonPair kv : obj) {
        int v = kv.value().as<int>();
        if (ac_state_apply_field(g_state, kv.key().c_str(), v)) {
            changed = true;
        } else {
            Serial.printf("[cmd] unknown field '%s' ignored\n", kv.key().c_str());
        }
    }
    if (!changed) return;

    uint8_t frame[MULTI_PARAM_FRAME_SIZE];
    size_t n = build_multi_param_frame(frame, g_cfg.brand, g_state.power,
                                       g_state.mode, g_state.temp, g_state.fan);
    ir_module_send(frame, n);
    net_publish_state(g_cfg, g_state, true);

    snprintf(g_last_action, sizeof(g_last_action), "cmd @%lus", (unsigned long)(millis() / 1000));
    refresh_display();
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[ac-gateway] boot");

    config_store_load(g_cfg);
    web_config_run(g_cfg);

    g_state = g_cfg.initial;
    ac_state_clamp(g_state);

    display_begin();
    ir_module_begin();

    net_set_command_handler(on_command);
    net_begin(g_cfg);

#if OTA_ENABLED
    ArduinoOTA.setHostname(g_cfg.device_name.c_str());
    ArduinoOTA.begin();
#endif

    net_publish_state(g_cfg, g_state, true);
    refresh_display();
    Serial.printf("[ac-gateway] ready, brand=0x%02X topic=%s\n",
                  g_cfg.brand, g_cfg.topic_prefix.c_str());
}

void loop() {
#if OTA_ENABLED
    ArduinoOTA.handle();
#endif

    net_loop();

    if (g_cmd_pending) {
        g_cmd_pending = false;
        apply_pending_command();
    }

    static uint32_t lastHeartbeat = 0;
    if (millis() - lastHeartbeat > STATE_HEARTBEAT_MS) {
        lastHeartbeat = millis();
        net_publish_state(g_cfg, g_state, net_connected());
    }

    static uint32_t lastDisplay = 0;
    if (millis() - lastDisplay > 1000) {
        lastDisplay = millis();
        refresh_display();
    }

    delay(2);
}
