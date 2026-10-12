#pragma once
#include <stdint.h>
#include "ac_encoder.h"

// The complete AC state we track and mirror to the real unit.
struct AcState {
    uint8_t power = AC_POWER_OFF;
    uint8_t mode  = AC_MODE_COOL;
    uint8_t temp  = 24;
    uint8_t fan   = AC_FAN_AUTO;
};

// Clamp every field into its legal range.
void ac_state_clamp(AcState& s);

// Apply one named field ("power"/"mode"/"temp"/"fan").
// Returns false and leaves the state untouched for unknown fields.
bool ac_state_apply_field(AcState& s, const char* field, int value);
