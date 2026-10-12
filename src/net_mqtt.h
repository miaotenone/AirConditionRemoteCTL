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
