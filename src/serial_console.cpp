#include "serial_console.h"
#include <Arduino.h>
#include <WiFi.h>
#include "web_config.h"

void serial_console_poll(AppConfig& cfg) {
    static String line;

    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c != '\r' && c != '\n') {
            if (line.length() < 220) line += c;
            continue;
        }

        line.trim();
        if (line.isEmpty()) continue;

        String cmd = line, arg;
        int sp = line.indexOf(' ');
        if (sp >= 0) {
            cmd = line.substring(0, sp);
            arg = line.substring(sp + 1);
            arg.trim();
        }
        line = "";

        if (cmd == "scan") {
            int n = WiFi.scanNetworks();
            Serial.printf("[scan] %d network(s) visible\n", n);
            for (int i = 0; i < n; i++) {
                Serial.printf("  %4d dBm  %s%s\n", (int)WiFi.RSSI(i), WiFi.SSID(i).c_str(),
                              WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "  (open)" : "");
            }
            WiFi.scanDelete();
        } else if (cmd == "show") {
            Serial.printf("[cfg] ssid=%s pass=%s host=%s port=%u\n", cfg.wifi_ssid.c_str(),
                          cfg.wifi_pass.length() ? "***" : "(empty)", cfg.mqtt_host.c_str(),
                          (unsigned)cfg.mqtt_port);
            Serial.printf("[cfg] user=%s mqttpass=%s prefix=%s name=%s brand=0x%02X\n",
                          cfg.mqtt_user.c_str(), cfg.mqtt_pass.length() ? "***" : "(empty)",
                          cfg.topic_prefix.c_str(), cfg.device_name.c_str(), cfg.brand);
        } else if (cmd == "set") {
            String k = arg, v;
            int sp2 = arg.indexOf(' ');
            if (sp2 >= 0) {
                k = arg.substring(0, sp2);
                v = arg.substring(sp2 + 1);
                v.trim();
            }
            bool ok = true;
            if (k == "ssid")
                cfg.wifi_ssid = v;
            else if (k == "pass")
                cfg.wifi_pass = v;
            else if (k == "host")
                cfg.mqtt_host = v;
            else if (k == "port")
                cfg.mqtt_port = (uint16_t)v.toInt();
            else if (k == "user")
                cfg.mqtt_user = v;
            else if (k == "mqttpass")
                cfg.mqtt_pass = v;
            else if (k == "prefix")
                cfg.topic_prefix = v;
            else if (k == "name")
                cfg.device_name = v;
            else if (k == "brand")
                cfg.brand = (uint8_t)strtol(v.c_str(), nullptr, 16);
            else
                ok = false;

            if (ok) {
                config_store_save(cfg);
                if (k == "ssid" || k == "pass") {
                    Serial.printf("[cfg] %s saved -- send 'restart' to connect\n", k.c_str());
                } else {
                    Serial.printf("[cfg] %s saved\n", k.c_str());
                }
            } else {
                Serial.printf("[cfg] unknown key '%s'\n", k.c_str());
                Serial.println("[cfg] keys: ssid pass host port user mqttpass prefix name brand");
            }
        } else if (cmd == "reset") {
            web_config_reset();
            Serial.println("[cfg] WiFi + config cleared, restarting");
            delay(300);
            ESP.restart();
        } else if (cmd == "restart") {
            Serial.println("[cfg] restarting");
            delay(200);
            ESP.restart();
        } else {
            Serial.printf("[cfg] unknown command '%s'\n", cmd.c_str());
            Serial.println("[cfg] try: scan | show | set <key> <value> | reset | restart");
        }
    }
}
