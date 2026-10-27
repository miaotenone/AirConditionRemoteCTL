#include "ac_state.h"
#include <string.h>

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void ac_state_clamp(AcState& s) {
    s.power = s.power ? AC_POWER_ON : AC_POWER_OFF;
    s.mode = (uint8_t)clamp_int(s.mode, 0, AC_MODE_HEAT);
    s.temp = (uint8_t)clamp_int(s.temp, AC_TEMP_MIN, AC_TEMP_MAX);
    s.fan = (uint8_t)clamp_int(s.fan, 0, AC_FAN_HIGH);
}

bool ac_state_apply_field(AcState& s, const char* field, int value) {
    if (strcmp(field, "power") == 0) {
        s.power = value ? AC_POWER_ON : AC_POWER_OFF;
        return true;
    }
    if (strcmp(field, "mode") == 0) {
        s.mode = (uint8_t)clamp_int(value, 0, AC_MODE_HEAT);
        return true;
    }
    if (strcmp(field, "temp") == 0) {
        s.temp = (uint8_t)clamp_int(value, AC_TEMP_MIN, AC_TEMP_MAX);
        return true;
    }
    if (strcmp(field, "fan") == 0) {
        s.fan = (uint8_t)clamp_int(value, 0, AC_FAN_HIGH);
        return true;
    }
    return false;
}
