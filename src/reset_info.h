#pragma once

// Decoding the ESP32's reset reason, kept pure (no Arduino, no esp_system.h) so
// every code the chip can report is reachable from the native test suite.
//
// Why this exists: chasing "OLED stuck on AirCon Gateway / starting... while the
// USB cable is unplugged" (2026-10-26) came down to one fact the firmware never
// told anyone -- had the board been resetting in a loop, or had it simply
// stopped? The ROM prints the reset reason exactly once, at 115200 baud, to a
// serial port that by definition is not attached when the fault happens. The
// firmware now carries that reason forward to the OLED (see boot_diag.h), where
// a glance is enough.
//
// `code` is an esp_reset_reason() value, taken as an int so this header stays
// host-compilable.

const char* reset_reason_name(int code);
