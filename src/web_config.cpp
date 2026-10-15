#include "web_config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include "config.h"
#include "pins.h"
#include "display.h"
#include "serial_console.h"
#include "wifi_policy.h"
#include <stdio.h>   // snprintf for the on-screen countdown
#include <string.h>
#include <esp_wifi.h>  // esp_wifi_get_config: bounded read of the saved creds

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
    // Must precede the first mode()/begin(): the core latches the persistence
    // flag when it initialises the driver, which the mode() call below does.
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    // We keep the credentials ourselves, and writing them to the driver's NVS
    // on every boot only burns the flash write budget.
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
// The portal's parameters, read back into the config. Shared by the save
// callback and the post-connect path, because WiFiManager calls them from
// different places and we need at least one of them to have run.
// ---------------------------------------------------------------------------
struct PortalFields {
    WiFiManagerParameter* host;
    WiFiManagerParameter* port;
    WiFiManagerParameter* user;
    WiFiManagerParameter* pass;
    WiFiManagerParameter* prefix;
    WiFiManagerParameter* device;
    WiFiManagerParameter* brand;
};

static void portal_fields_to_cfg(AppConfig& cfg, PortalFields& f) {
    cfg.mqtt_host    = f.host->getValue();
    cfg.mqtt_port    = portal_port_or(f.port->getValue(), cfg.mqtt_port);
    cfg.mqtt_user    = f.user->getValue();
    cfg.mqtt_pass    = f.pass->getValue();
    cfg.topic_prefix = f.prefix->getValue();
    cfg.device_name  = f.device->getValue();
    cfg.brand        = portal_brand_or(f.brand->getValue(), cfg.brand);

    if (cfg.topic_prefix.isEmpty()) cfg.topic_prefix = DEFAULT_TOPIC_PREFIX;
    if (cfg.device_name.isEmpty())  cfg.device_name  = DEFAULT_DEVICE_NAME;
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

    PortalFields fields = { &p_host, &p_port, &p_user, &p_pass, &p_prefix, &p_dev, &p_brand };

    // Persist the moment the user presses Save, not only when a connection
    // succeeds. The SSID box on the wifi page is a *placeholder*, not a value,
    // so the natural way to fix just the MQTT host -- save with the SSID left
    // blank -- never reaches our write-back at all: WiFiManager skips the
    // connect entirely and the portal just stays open. Without this callback
    // those settings were silently dropped when our timeout rebooted the board,
    // and a device whose network was really gone could never be given an MQTT
    // host through the portal at all.
    wm.setSaveParamsCallback([&cfg, &fields]() {
        portal_fields_to_cfg(cfg, fields);
        config_store_save(cfg);
        Serial.printf("[wifi] settings saved from the portal (host=%s)\n",
                      cfg.mqtt_host.c_str());
    });

    Serial.printf("[wifi] portal: join AP '%s' (pw '%s'), open http://192.168.4.1\n",
                  WIFI_PORTAL_AP_NAME, WIFI_PORTAL_AP_PASS);
    display_lines("AirCon Gateway", "portal open", "AP: AC-Gateway-Setup",
                  "pw: acgateway");

    // In non-blocking mode this raises the AP + web server and returns at once;
    // process() below is what then services requests. Calling process() without
    // this would leave the portal shut for good, and the linker silently drops
    // the whole captive portal as dead code when nothing calls it.
    wm.startConfigPortal(WIFI_PORTAL_AP_NAME, WIFI_PORTAL_AP_PASS);

    unsigned long opened = millis();
    unsigned long t0     = millis();   // restarts while a client is attached
    unsigned long last_shown = millis() - 1000;   // paint on the first pass
    while (!wm.process()) {
        // Our own clock, not the library's: it bumps its portal-start time on
        // every web request, which would extend the timeout indefinitely. A
        // phone that is actually attached to the AP holds it open too -- nobody
        // gets rebooted halfway through typing in their MQTT settings.
        bool client = WiFi.softAPgetStationNum() > 0;
        if (client) {
            t0 = millis();
        } else if (millis() - t0 > WIFI_PORTAL_TIMEOUT_S * 1000UL) {
            Serial.println("[wifi] portal timed out, restarting");
            display_lines("AirCon Gateway", "portal timeout", "rebooting...", "");
            delay(1000);
            ESP.restart();
        }

        // But a phone someone walked away from is still associated, and the
        // clock above would never expire: cap the whole visit so the gateway
        // always gets back to controlling the AC. BOOT still resets it early.
        if (portal_hard_timeout(millis(), opened, PORTAL_HARD_TIMEOUT_S)) {
            Serial.println("[wifi] portal open too long, restarting");
            portal_fields_to_cfg(cfg, fields);
            config_store_save(cfg);   // keep whatever they did type
            display_lines("AirCon Gateway", "portal timeout", "rebooting...", "");
            delay(1000);
            ESP.restart();
        }

        // Once a second: how long the portal has left. Between refreshes the
        // screen keeps whatever it has, and display_lines() is not cheap.
        if (millis() - last_shown >= 1000) {
            last_shown = millis();
            char countdown[24];
            snprintf(countdown, sizeof(countdown), "portal %us",
                     (unsigned)portal_seconds_left(millis(), t0, WIFI_PORTAL_TIMEOUT_S));
            Serial.printf("[wifi] %s%s\n", countdown, client ? " (client attached)" : "");
            display_lines("AirCon Gateway", "portal open", countdown,
                          "192.168.4.1");
        }

        serial_console_poll(cfg);
        if (reset_button_hold_detected()) reboot_into_portal("BOOT held");
        delay(10);
    }

    // The portal connected to whatever the user picked, so THAT is the network
    // we persist. Recording only the SSID when nothing was stored (and never
    // the password) is what used to leave a moved device pointing at its old
    // network on every subsequent boot.
    //
    // Bounded by hand, straight off the IDF's own struct: conf.sta.ssid[32] and
    // .password[64] carry NO room for a NUL at full length, so WiFi.SSID() /
    // WiFi.psk() -- which build a String with strlen() on those arrays -- read
    // past the end for a legal 32-char SSID or 64-char passphrase and hand us
    // the garbage. At WiFiDriverNVS 0xff / 0x00 padding that used to be
    // persisted as the network name.
    wifi_config_t wconf;
    memset(&wconf, 0, sizeof(wconf));
    esp_err_t got = esp_wifi_get_config(WIFI_IF_STA, &wconf);

    char ssid_buf[33];
    char pass_buf[65];
    if (got == ESP_OK) {
        wifi_field_copy(ssid_buf, sizeof(ssid_buf), (const char*)wconf.sta.ssid,     sizeof(wconf.sta.ssid));
        wifi_field_copy(pass_buf, sizeof(pass_buf), (const char*)wconf.sta.password, sizeof(wconf.sta.password));
    } else {
        ssid_buf[0] = '\0';
        pass_buf[0] = '\0';
    }

    if (ssid_buf[0] == '\0') {          // fall back to what WiFiManager holds
        wifi_field_copy(ssid_buf, sizeof(ssid_buf), wm.getWiFiSSID().c_str(), strlen(wm.getWiFiSSID().c_str()));
        wifi_field_copy(pass_buf, sizeof(pass_buf), wm.getWiFiPass().c_str(), strlen(wm.getWiFiPass().c_str()));
    }

    WifiCred cred = wifi_cred_after_portal(ssid_buf, pass_buf,
                                           cfg.wifi_ssid.c_str(), cfg.wifi_pass.c_str());
    cfg.wifi_ssid = cred.ssid;
    cfg.wifi_pass = cred.pass;

    portal_fields_to_cfg(cfg, fields);
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
