#pragma once
#include <Arduino.h>
#include "config_store.h"
#include "ac_state.h"

// Called when a message arrives on <prefix>/cmd. payload is NOT null-terminated.
typedef void (*AcCommandHandler)(const char* payload, size_t len);

void net_set_command_handler(AcCommandHandler handler);
void net_begin(const AppConfig& cfg);
void net_loop();
bool net_connected();
void net_publish_state(const AppConfig& cfg, const AcState& state, bool online);

// --- v1.2 topics ----------------------------------------------------------
//
//   <prefix>/hb     server -> device, QoS0, NOT retained   (service liveness)
//   <prefix>/env    device -> server, QoS1, retained       (DHT11 reading)
//   <prefix>/event  device -> server, QoS1, NOT retained   (fail-safe trip)
//
// `hb` is the only proof the device has that the process making the decisions
// is still running: Mosquitto lives on the same Pi but as its own systemd unit,
// so "MQTT is connected" says nothing about the bot service (design 18.8).
bool net_service_alive();

struct HeartbeatSnapshot {
    bool seen;
    uint32_t last_received_ms;
};
// Observation only; liveness still comes from net_service_alive().
HeartbeatSnapshot net_heartbeat_snapshot();

// Publish one DHT11 reading. `ok=false` means "this read failed" -- the service
// must not turn that into data.
void net_publish_env(const AppConfig& cfg, int temp_c, int humidity, bool ok);

// Publish a device event, not retained and stamped with the wall clock. Returns
// false when it could not be sent -- including when the clock has not synced
// yet, because the service drops events without a usable `ts`.
bool net_publish_event(const AppConfig& cfg, const char* type, const AcState& prev);

// True once SNTP has produced a believable wall clock (needed for `ts`).
bool net_time_synced();
