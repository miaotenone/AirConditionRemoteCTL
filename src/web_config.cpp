#include "web_config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include "config.h"
#include "pins.h"
#include "display.h"
#include "serial_console.h"
#include "wifi_policy.h"

// How long the BOOT button has to be held before we treat it as "I want to
// configure this device". Long enough not to fire on a stray bump.
static const uint32_t RESET_HOLD_MS = 3000;

bool reset_button_hold_detected() {
    static bool     down  = false;
    static uint32_t since = 0;
    static bool     fired = false;

    if (digitalRead(PIN_BOOT) != LOW) {
        down = false;
        fired = false;
        return false;
    }
    if (!down) {
        down  = true;
        since = millis();
        return false;
    }
    if (!fired && (uint32_t)(millis() - since) >= RESET_HOLD_MS) {
        fired = true;
        return true;
    }
    return false;
}

static void reboot_into_portal(const char* why) {
    Serial.printf("[wifi] %s -> WiFi + config cleared, rebooting\n", why);
    web_config_reset();
    display_lines("AirCon Gateway", "config cleared", "rebooting...", "");
    delay(800);
    ESP.restart();
}

// ---------------------------------------------------------------------------
// Phase 1: try the credentials WE hold (serial `set ssid`/`set pass`, or the
// last config portal run). WiFiManager only knows what it saved itself, so
// without this a provisioned board would still block on the portal even though
// its config is complete.
// ---------------------------------------------------------------------------
static bool try_stored(AppConfig& cfg) {
    if (cfg.wifi_ssid.isEmpty()) {
        Serial.println("[wifi] no stored WiFi credentials -> config portal");
        return false;
    }

    Serial.printf("[wifi] trying stored ssid '%s'", cfg.wifi_ssid.c_str());
    WiFi.mode(WIFI_STA);
    // We keep the credentials ourselves, and writing them to the driver's NVS
    // on every boot only burns the flash write budget.
    WiFi.persistent(false);
    WiFi.begin(cfg.wifi_ssid.c_str(), cfg.wifi_pass.c_str());

    char line[48];
    snprintf(line, sizeof(line), "trying: %.24s", cfg.wifi_ssid.c_str());
    display_lines("AirCon Gateway", line, "wait 20s", "hold BOOT to reset");

    unsigned long t0      = millis();
    unsigned long lastDot = 0;
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_STA_TIMEOUT_MS) {
        delay(50);
        // The console must stay reachable: this window is the only chance to
        // provision a board that has no credentials.
        serial_console_poll(cfg);
        if (reset_button_hold_detected()) reboot_into_portal("BOOT held");
        if (millis() - lastDot >= 1000) { lastDot = millis(); Serial.print('.'); }
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[wifi] connected, IP=%s\n", WiFi.localIP().toString().c_str());
        WiFi.setHostname(cfg.device_name.c_str());
        return true;
    }
    Serial.println("[wifi] stored credentials did not connect -> config portal");
    return false;
}

// ---------------------------------------------------------------------------
// Phase 2: the captive portal. Runs NON-BLOCKING so this loop can keep the
// serial console, the BOOT button and the OLED alive while we wait -- the old
// blocking autoConnect() froze all three, so a device that had moved house sat
// on a static "connecting WiFi..." screen and could not even be reset without
// a serial cable.
// ---------------------------------------------------------------------------
static bool wait_portal(AppConfig& cfg) {
    WiFiManager wm;
    wm.setDebugOutput(false);
    wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_S);
    wm.setConfigPortalBlocking(false);
    wm.setConnectTimeout(5);        // keep a bad pick from stalling for a minute
    wm.setAPClientCheck(true);      // a connected phone keeps the portal alive

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

    Serial.printf("[wifi] portal: join AP '%s' (pw '%s'), open http://192.168.4.1\n",
                  WIFI_PORTAL_AP_NAME, WIFI_PORTAL_AP_PASS);
    display_lines("AirCon Gateway", "portal open", "AP: AC-Gateway-Setup",
                  "pw: acgateway");

    // In non-blocking mode this raises the AP + web server and returns at once;
    // process() below is what then services requests. Calling process() without
    // this would leave the portal shut for good, and the linker silently drops
    // the whole captive portal as dead code when nothing calls it.
    wm.startConfigPortal(WIFI_PORTAL_AP_NAME, WIFI_PORTAL_AP_PASS);

    unsigned long t0 = millis();
    while (!wm.process()) {
        // Our own clock, not the library's: it bumps its portal-start time on
        // every web request, which would extend the timeout indefinitely. A
        // phone that is actually attached to the AP holds it open too -- nobody
        // gets rebooted halfway through typing in their MQTT settings.
        if (WiFi.softAPgetStationNum() > 0) {
            t0 = millis();
        } else if (millis() - t0 > WIFI_PORTAL_TIMEOUT_S * 1000UL) {
            Serial.println("[wifi] portal timed out, restarting");
            display_lines("AirCon Gateway", "portal timeout", "rebooting...", "");
            delay(1000);
            ESP.restart();
        }
        serial_console_poll(cfg);
        if (reset_button_hold_detected()) reboot_into_portal("BOOT held");
        delay(10);
    }

    // The portal connected to whatever the user picked, so THAT is the network
    // we persist. Recording only the SSID when nothing was stored (and never
    // the password) is what used to leave a moved device pointing at its old
    // network on every subsequent boot.
    String portal_ssid = WiFi.SSID();
    String portal_pass = WiFi.psk();
    if (portal_ssid.isEmpty()) {          // fall back to what WiFiManager holds
        portal_ssid = wm.getWiFiSSID();
        portal_pass = wm.getWiFiPass();
    }

    WifiCred cred = wifi_cred_after_portal(portal_ssid.c_str(), portal_pass.c_str(),
                                          cfg.wifi_ssid.c_str(), cfg.wifi_pass.c_str());
    cfg.wifi_ssid = cred.ssid;
    cfg.wifi_pass = cred.pass;

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

    config_store_save(cfg);
    WiFi.setHostname(cfg.device_name.c_str());
    Serial.printf("[wifi] connected, IP=%s (saved)\n", WiFi.localIP().toString().c_str());
    return true;
}

void web_config_run(AppConfig& cfg) {
    if (try_stored(cfg)) return;
    wait_portal(cfg);
}

void web_config_reset() {
    WiFiManager wm;
    wm.setDebugOutput(false);
    wm.resetSettings();      // forget the saved AP / password
    config_store_clear();    // forget MQTT + app settings
}
