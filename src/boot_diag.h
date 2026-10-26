#pragma once

// Boot self-report: which boot is this, and why did the last one end?
//
// Written for one specific failure (2026-10-26): with the USB cable unplugged
// the gateway stopped during bring-up -- OLED parked on "starting...", no WiFi,
// no MQTT, `ac/cmd` unanswered -- and the board only came back when the cable
// was plugged in again. Nothing in the firmware could say whether it had been
// resetting in a loop (supply sagging under the WiFi start-up current) or had
// simply stopped, and pressing RST changed nothing, so the two were
// indistinguishable from outside.
//
// The counter lives in RTC memory: it survives a reset -- including a panic and
// a brown-out -- but not a real power loss. That is exactly the distinction
// that matters: a number that keeps climbing means a reset loop, a frozen one
// means whatever ran last never finished.
//
// All of it is mirrored on the boot screen, because the serial port is not
// attached when this matters: the screen is the one output that survives.

// Call once, first thing after Serial.begin(): decode the reset reason, bump
// the boot counter, print the "[ac-gateway] boot #N rst=..." banner line.
void boot_diag_begin();

// Paint the boot screen for one phase, e.g. "starting...", "wifi up". The last
// phase painted is the phase the gateway died in -- so the phases have to be
// painted *before* the risky call, not after it.
void boot_diag_screen(const char* phase);

unsigned    boot_diag_count();         // boots since the RTC domain last lost power
const char* boot_diag_reason();        // reset_reason_name() of the last reset
bool        boot_diag_power_cycled();  // true = first boot since a real power-up
