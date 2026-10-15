#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>

#include "config.h"
#include "pins.h"
#include "ac_encoder.h"
#include "ac_state.h"
#include "dht_sensor.h"
#include "ir_module.h"
#include "display.h"
#include "config_store.h"
#include "state_store.h"
#include "web_config.h"
#include "serial_console.h"
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

// Last DHT11 reading (design 6.1). Published to <prefix>/env and shown on the
// room page; the closed-loop decision itself lives on the Raspberry Pi.
static bool g_env_ok  = false;
static int  g_room_c  = 0;
static int  g_humidity = 0;

// Fail-safe bookkeeping (design 8). The AC gets turned off when the device
// loses contact with whoever is making the decisions, so we have to remember
// whether we ever had contact, whether we already tripped, and what the state
// was before we forced it off.
static bool     g_failsafe_armed   = false;
static bool     g_failsafe_tripped = false;
static bool     g_failsafe_report  = false;  // the event still needs sending
static uint32_t g_last_contact     = 0;
static AcState  g_prev_state;

// Kept alive so DisplayInfo.ip stays valid (a temporary String would not).
static String   g_ip_str;

static const char* net_state_name() {
    if (net_connected()) return "OK";
    if (WiFi.status() == WL_CONNECTED) return "MQTT?";
    return "WiFi?";
}

static void refresh_display() {
    DisplayInfo info;
    info.ac            = g_state;
    info.net_state     = net_state_name();
    g_ip_str           = WiFi.localIP().toString();
    info.ip            = g_ip_str.c_str();
    info.env_ok        = g_env_ok;
    info.room_c        = g_room_c;
    info.humidity      = g_humidity;
    info.service_alive = net_service_alive();
    info.failsafe      = g_failsafe_tripped;
    info.uptime_s      = millis() / 1000;
    display_show_info(info);
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
    if (!changed) {
        // The command asked for exactly what we already have. There is nothing
        // for the IR module to do -- but the service's confirmation window is
        // 5 s wide, and our 30 s state heartbeat does not fit inside it, so a
        // silent "no change" is what the service reports as "the device never
        // answered". Echo the state now: no change is still an answer (design
        // 18.12 #1).
        net_publish_state(g_cfg, g_state, true);
        return;
    }

    state_store_save(g_state);      // remember it before anything else can fail

    uint8_t frame[MULTI_PARAM_FRAME_SIZE];
    size_t n = build_multi_param_frame(frame, g_cfg.brand, g_state.power,
                                       g_state.mode, g_state.temp, g_state.fan);
    ir_module_send(frame, n);
    net_publish_state(g_cfg, g_state, true);

    snprintf(g_last_action, sizeof(g_last_action), "cmd @%lus", (unsigned long)(millis() / 1000));
    refresh_display();
}

// --- DHT11 (design 6.1) ----------------------------------------------------
//
// We publish `ok=false` on a failed read rather than staying quiet: the service
// must be able to tell "no data" from "sensor broken", and it drops ok=false
// either way.
static void dht_loop() {
    static uint32_t last = 0;
    if (last == 0) {
        // DHT11 wants >= 1s of settling after power-up; the first read lands a
        // few seconds into the run rather than in the middle of boot.
        last = millis() - DHT_READ_MS + 5000UL;
    }
    if ((uint32_t)(millis() - last) < DHT_READ_MS) return;
    last = millis();

    int t = g_room_c, h = g_humidity;
    bool ok = dht_read(t, h);
    g_env_ok   = ok;
    g_room_c   = t;
    g_humidity = h;

    net_publish_env(g_cfg, t, h, ok);
    if (ok) Serial.printf("[dht] room=%dC humidity=%d%%\n", t, h);
    else    Serial.println("[dht] read failed");
}

// --- fail-safe (design 8) --------------------------------------------------
//
// "Contact" means BOTH halves are healthy:
//   * the WiFi link and the MQTT session to the broker, and
//   * a recent <prefix>/hb from the service that makes the decisions.
//
// Mosquitto is its own systemd unit on the Pi, so a live broker with a dead bot
// process still looks connected -- the heartbeat is what catches that case.
// After five minutes of no contact we send one IR "off" and say so as soon as
// we can reach the service again.
static void failsafe_loop() {
    uint32_t now = millis();
    bool healthy = net_connected() && net_service_alive();

    if (healthy) {
        g_last_contact = now;

        // Boot guard: never trip before we have proven we can reach the
        // service at least once. Otherwise a device that boots without WiFi
        // would "fail safe" against an AC it never controlled.
        if (!g_failsafe_armed) {
            g_failsafe_armed = true;
            Serial.println("[failsafe] armed (first contact)");
        }

        if (g_failsafe_tripped) {
            g_failsafe_tripped = false;
            net_publish_state(g_cfg, g_state, true);
            Serial.println("[failsafe] contact restored");
            refresh_display();
        }

        if (g_failsafe_report && net_publish_event(g_cfg, "failsafe_off", g_prev_state)) {
            g_failsafe_report = false;
            Serial.println("[failsafe] reported to the service");
        }
        return;
    }

    if (!g_failsafe_armed || g_failsafe_tripped) return;
    if ((uint32_t)(now - g_last_contact) < LINK_LOST_OFF_MS) return;

    Serial.println("[failsafe] no contact for 5 min -> AC off");
    g_prev_state = g_state;
    g_state.power = AC_POWER_OFF;
    state_store_save(g_state);

    uint8_t frame[MULTI_PARAM_FRAME_SIZE];
    size_t n = build_multi_param_frame(frame, g_cfg.brand, g_state.power,
                                       g_state.mode, g_state.temp, g_state.fan);
    ir_module_send(frame, n);
    net_publish_state(g_cfg, g_state, false);   // a no-op while offline

    g_failsafe_tripped = true;
    g_failsafe_report  = true;
    snprintf(g_last_action, sizeof(g_last_action), "failsafe @%lus",
             (unsigned long)(millis() / 1000));
    refresh_display();
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[ac-gateway] boot");

    // The OLED is our only user-facing feedback, so bring it up FIRST --
    // web_config_run() below can block for a long time on the config portal.
    display_begin();
    display_lines("AirCon Gateway", "starting...", "", "");

    config_store_load(g_cfg);
    // The AC is still wherever we left it, so a reboot must not rewrite that.
    // The configured "initial" values are what a factory-fresh device starts
    // at, and only what an empty NVS falls back to (design 18.12 #2).
    g_state = g_cfg.initial;
    if (state_store_load(g_state)) {
        Serial.println("[state] restoring the remembered AC state");
    } else {
        Serial.println("[state] nothing remembered; using the configured initial state");
    }
    ac_state_clamp(g_state);

    // Escape hatch: send 'r' within 2s of boot -- or hold the BOOT button --
    // to wipe the stored WiFi credentials + app settings. Use when the saved
    // network is gone (or the password changed) and the config portal would
    // otherwise never open.
    pinMode(PIN_BOOT, INPUT_PULLUP);
    Serial.print("[ac-gateway] send 'r' (or hold BOOT) within 2s to reset WiFi+config ... ");
    bool want_reset = false;
    unsigned long t0 = millis();
    while (millis() - t0 < 2000) {
        int c = Serial.read();
        if (c == 'r' || c == 'R') { want_reset = true; break; }
        if (reset_button_hold_detected()) { want_reset = true; break; }
        delay(5);
    }
    Serial.println(want_reset ? "RESET" : "ok");
    if (want_reset) {
        web_config_reset();
        display_lines("AirCon Gateway", "config cleared", "rebooting...", "");
        Serial.println("[ac-gateway] WiFi + config cleared, rebooting");
        delay(800);
        ESP.restart();
    }

    // web_config_run() puts up its own per-phase screens ("trying: <ssid>",
    // "portal open", "portal timeout"), so setup() does not pre-paint one here.
    web_config_run(g_cfg);

    display_lines("AirCon Gateway", "WiFi OK",
                  WiFi.localIP().toString().c_str(), "starting MQTT");

    ir_module_begin();
    dht_begin();

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

    serial_console_poll(g_cfg);
    net_loop();

    // Re-provisioning without a cable: hold BOOT for 3 s at any point while
    // running to wipe the WiFi + app config and reboot into the portal. The
    // same press works during the connect window and while the portal is open
    // (see web_config.cpp), so a device that moved house is recoverable
    // whatever stage it is stuck at.
    if (reset_button_hold_detected()) {
        web_config_reset();
        display_lines("AirCon Gateway", "config cleared", "rebooting...", "");
        Serial.println("[ac-gateway] BOOT held -> WiFi + config cleared, rebooting");
        delay(800);
        ESP.restart();
    }

    if (g_cmd_pending) {
        g_cmd_pending = false;
        apply_pending_command();
    }

    // Safety first, then the sensor, then housekeeping.
    failsafe_loop();
    dht_loop();

    static uint32_t lastHeartbeat = 0;
    if (millis() - lastHeartbeat > STATE_HEARTBEAT_MS) {
        lastHeartbeat = millis();
        net_publish_state(g_cfg, g_state, net_connected());
    }

    // Hand the pager fresh data once a second; display_tick() does the actual
    // animating at its own frame cadence.
    static uint32_t lastDisplay = 0;
    if (millis() - lastDisplay > 1000) {
        lastDisplay = millis();
        refresh_display();
    }
    display_tick();

    delay(2);
}
