#include "net_mqtt.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>

#include "link_guard.h"

static WiFiClient   s_wifiClient;
static PubSubClient s_mqtt(s_wifiClient);
static AppConfig    s_cfg;
static AcCommandHandler s_handler = nullptr;
static uint32_t     s_lastAttempt = 0;
static String       s_topicCmd, s_topicState, s_topicStatus;
static String       s_topicEnv, s_topicHb, s_topicEvent;

// Network watch-dog (config.h has the thresholds and the reasoning).
// `s_lastHealthy` is the last time the WHOLE path was up -- WiFi associated AND
// an MQTT session established. Both failure shapes move it: a station that lost
// its association, and one the driver still reports as connected while nothing
// gets through. Reading WiFi.status() alone would miss the second.
static uint32_t s_lastHealthy   = 0;
static uint32_t s_lastRedial    = 0;
static uint32_t s_linkDownSince = 0;
static bool     s_linkWasUp     = false;

// Last <prefix>/hb, and whether we have ever seen one. Both are written from
// the MQTT callback and read from loop(), hence volatile.
static volatile uint32_t s_lastHb   = 0;
static volatile bool     s_hbSeen   = false;

// Which topic a message arrived on decides what it is. The old code only ever
// subscribed to /cmd, so it could hand every payload to the command handler;
// now that /hb shares the subscription that shortcut would be a bug.
static void mqtt_callback(char* topic, byte* payload, unsigned int len) {
    if (s_topicHb.length() && strcmp(topic, s_topicHb.c_str()) == 0) {
        s_lastHb = millis();
        s_hbSeen = true;
        return;
    }
    if (s_handler && s_topicCmd.length() && strcmp(topic, s_topicCmd.c_str()) == 0) {
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
    s_topicEnv    = cfg.topic_prefix + "/env";
    s_topicHb     = cfg.topic_prefix + "/hb";
    s_topicEvent  = cfg.topic_prefix + "/event";
}

// --- SNTP -----------------------------------------------------------------
//
// `ac/event` carries a wall-clock `ts` and the service only acts on an event
// younger than 10 minutes, so the clock has to be real. Started once, on the
// first time we have WiFi; lwIP keeps re-syncing afterwards.
static bool s_ntpStarted = false;

static void maybe_start_ntp() {
    if (s_ntpStarted || WiFi.status() != WL_CONNECTED) return;
    configTime(NTP_TZ_OFFSET_S, 0, NTP_SERVER1, NTP_SERVER2);
    s_ntpStarted = true;
    Serial.printf("[ntp] syncing via %s / %s\n", NTP_SERVER1, NTP_SERVER2);
}

bool net_time_synced() {
    return time(nullptr) > 1600000000;   // sanity floor: ~Sep 2020
}

bool net_service_alive() {
    // Unsigned subtraction, so this survives the ~49-day millis() wrap.
    return s_hbSeen && (uint32_t)(millis() - s_lastHb) < SERVICE_HB_TIMEOUT_MS;
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
        s_mqtt.subscribe(s_topicHb.c_str());
        s_mqtt.publish(s_topicStatus.c_str(), "{\"online\":true}", true);
    } else {
        Serial.printf("[mqtt] connect failed rc=%d\n", s_mqtt.state());
    }
    return ok;
}

// Force a fresh association. The core's own reconnect is exactly what silently
// gives up on a dropped station, so we do not leave the recovery to it:
// disconnect + begin() starts a new join attempt right now, and
// setAutoReconnect(true) re-arms the core for the next time. The core's
// persistence flag is already off (web_config latches WiFi.persistent(false)
// before the first mode()), so none of this burns flash writes.
static void wifi_redial() {
    Serial.printf("[net] link down %lus -> re-dialling WiFi\n",
                  (unsigned long)((millis() - s_lastHealthy) / 1000));
    WiFi.disconnect();
    WiFi.setAutoReconnect(true);
    if (s_cfg.wifi_ssid.length()) {
        WiFi.begin(s_cfg.wifi_ssid.c_str(), s_cfg.wifi_pass.c_str());
    }
    s_lastAttempt = 0;   // let the MQTT dial that follows happen immediately
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
        maybe_start_ntp();
        mqtt_connect();
    }

    // The watch-dog counts from here, not from boot: everything up to this call
    // was startup, which web_config already policed.
    s_lastHealthy   = millis();
    s_lastRedial    = 0;
    s_linkDownSince = s_lastHealthy;
    s_linkWasUp     = net_connected();
}

bool net_connected() {
    return WiFi.status() == WL_CONNECTED && s_mqtt.connected();
}

void net_loop() {
    if (WiFi.status() == WL_CONNECTED) {
        maybe_start_ntp();

        if (!s_mqtt.connected()) {
            uint32_t now = millis();
            if (now - s_lastAttempt > 5000 || s_lastAttempt == 0) {
                s_lastAttempt = now;
                if (s_cfg.mqtt_host.length()) mqtt_connect();
            }
        }
        s_mqtt.loop();
    }

    // Health, then escalation. This runs even when the station is down -- the
    // case the old code returned early on, which is how a link could stay dead
    // for hours with nobody trying to fix it.
    uint32_t now = millis();
    if (net_connected()) {
        if (!s_linkWasUp) {
            Serial.printf("[net] link back after %lus\n",
                          (unsigned long)((now - s_linkDownSince) / 1000));
            s_linkWasUp = true;
        }
        s_lastHealthy = now;
        s_lastRedial  = 0;   // a live session resets the escalation
        return;
    }

    if (s_linkWasUp) {
        s_linkWasUp     = false;
        s_linkDownSince = now;
        Serial.printf("[net] link lost (wifi=%d mqtt=%d)\n",
                      (int)WiFi.status(), (int)s_mqtt.connected());
    }

    // Nothing provisioned to reach: the portal or the serial console owns that,
    // and re-dialling a network we have no credentials for helps nobody.
    if (!s_cfg.wifi_ssid.length() || !s_cfg.mqtt_host.length()) return;

    LinkGuardInput in = { now, s_lastHealthy, s_lastRedial,
                          NET_REDIAL_AFTER_MS, NET_REDIAL_EVERY_MS, NET_REBOOT_AFTER_MS };
    switch (link_guard_action(in)) {
        case LINK_GUARD_REDIAL:
            s_lastRedial = now;
            wifi_redial();
            break;
        case LINK_GUARD_REBOOT:
            // Last resort: state and credentials are in NVS, so a reboot costs
            // a re-boot and buys a clean driver + a fresh join attempt.
            Serial.printf("[net] link down %lus -> rebooting\n",
                          (unsigned long)((now - s_lastHealthy) / 1000));
            Serial.flush();
            delay(50);
            ESP.restart();
            break;
        default:
            break;
    }
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

void net_publish_env(const AppConfig& cfg, int temp_c, int humidity, bool ok) {
    (void)cfg;
    if (!s_mqtt.connected()) return;

    StaticJsonDocument<128> doc;
    doc["temp"]     = temp_c;
    doc["humidity"] = humidity;
    doc["ok"]       = ok;

    char buf[96];
    size_t n = serializeJson(doc, buf, sizeof(buf));
    // Retained: a subscriber that starts later still learns the last reading.
    // The service knows to ignore the *replay* of it (design 18.6).
    s_mqtt.publish(s_topicEnv.c_str(), reinterpret_cast<const uint8_t*>(buf), n, true);
}

bool net_publish_event(const AppConfig& cfg, const char* type, const AcState& prev) {
    (void)cfg;
    if (!s_mqtt.connected()) return false;

    time_t now = time(nullptr);
    if (!net_time_synced()) {
        // Without a clock the service cannot tell a fresh event from a retained
        // replay, so it drops it -- better to say so than to send a lie.
        Serial.println("[mqtt] event withheld: clock not synced");
        return false;
    }

    StaticJsonDocument<256> doc;
    doc["type"] = type;
    doc["ts"]   = (long)now;
    JsonObject p = doc.createNestedObject("prev");
    p["power"] = prev.power;
    p["mode"]  = prev.mode;
    p["temp"]  = prev.temp;
    p["fan"]   = prev.fan;

    char buf[192];
    size_t n = serializeJson(doc, buf, sizeof(buf));
    // NOT retained (design 9.1): an event means "this just happened". A retained
    // copy outlives the moment and is replayed to every later subscriber as if
    // it were fresh -- and the service drops retained replays anyway, so keeping
    // one buys nothing and only plants a stale "fail-safe tripped" landmine.
    bool sent = s_mqtt.publish(s_topicEvent.c_str(),
                               reinterpret_cast<const uint8_t*>(buf), n, false);
    Serial.printf("[mqtt] event %s %s\n", type, sent ? "published" : "FAILED");
    return sent;
}
