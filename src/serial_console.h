#pragma once
#include "config_store.h"

// Bench provisioning over the USB serial port -- lets us set WiFi + MQTT
// config without the phone / captive portal, which makes repeatable bring-up
// and integration testing practical.
//
//   scan                list visible WiFi networks
//   show                print the current config (passwords masked)
//   set <key> <value>   set one field and persist it immediately
//                       keys: ssid pass host port user mqttpass prefix name brand
//   reset               wipe WiFi + config and restart (same as boot-time 'r')
//   restart             reboot (applies a changed SSID / MQTT host)
//
// Polled from loop(), AND from the WiFi-connect window inside web_config_run():
// once the captive portal takes over it blocks, so this must be reachable
// before that happens or a board with no credentials could never be provisioned.
void serial_console_poll(AppConfig& cfg);
