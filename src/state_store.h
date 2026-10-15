#pragma once
#include "ac_state.h"

// The AC state this gateway believes it has set (design 18.12 #2).
//
// An air conditioner cannot be read: the IR link is one-way, so this state is a
// *memory*, and a memory that dies with the power means a reboot leaves the
// gateway believing the AC is off while it is still humming away. The card then
// lies, and the next "temperature" press sends a full frame with power=0 --
// which really does switch the unit off. Persisting the state in NVS is what
// makes a reboot stop being a lie.
//
// Shares the "acctl" NVS namespace with the stored config, so the boot-time 'r'
// escape hatch (web_config_reset -> config_store_clear) wipes both together.

// Returns false when nothing has been remembered yet (first boot, or after a
// reset); `state` is then left untouched.
bool state_store_load(AcState& state);

// Remember `state` as what the AC is set to. Only called when it changed --
// NVS has a finite write budget.
void state_store_save(const AcState& state);

// Forget the remembered state (factory reset of the app settings).
void state_store_clear();
