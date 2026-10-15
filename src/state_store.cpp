#include "state_store.h"
#include <Arduino.h>
#include <Preferences.h>

namespace {
constexpr const char* NVS_NAMESPACE = "acctl";
// "st_power" doubles as the "the set is complete" marker: all four keys are
// written together, so finding any one of them means finding all of them.
constexpr const char* KEY_MARKER = "st_power";
}  // namespace

bool state_store_load(AcState& state) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    bool remembered = prefs.isKey(KEY_MARKER);
    if (remembered) {
        state.power = (uint8_t)prefs.getUChar("st_power", state.power);
        state.mode  = (uint8_t)prefs.getUChar("st_mode",  state.mode);
        state.temp  = (uint8_t)prefs.getUChar("st_temp",  state.temp);
        state.fan   = (uint8_t)prefs.getUChar("st_fan",   state.fan);
    }
    prefs.end();
    return remembered;
}

void state_store_save(const AcState& state) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putUChar("st_power", state.power);
    prefs.putUChar("st_mode",  state.mode);
    prefs.putUChar("st_temp",  state.temp);
    prefs.putUChar("st_fan",   state.fan);
    prefs.end();
    Serial.printf("[state] remembered power=%d mode=%d temp=%d fan=%d\n",
                  state.power, state.mode, state.temp, state.fan);
}

void state_store_clear() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.remove("st_power");
    prefs.remove("st_mode");
    prefs.remove("st_temp");
    prefs.remove("st_fan");
    prefs.end();
    Serial.println("[state] forgot the remembered AC state");
}
