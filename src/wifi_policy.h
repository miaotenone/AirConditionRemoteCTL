#pragma once
#include <stddef.h>

// Which WiFi credentials the gateway should remember after the config portal
// closed. Pure logic, no Arduino -- this is the regression guard for the
// "moved the device to another network and it kept trying the old one forever"
// bug: the portal connects to the NEW network, so the NEW network is what has
// to be written back to NVS. Keeping it here (instead of inline in
// web_config.cpp) is what makes that reachable from the native test suite.
//
// The portal reports what it actually connected to. When it reports nothing --
// the portal was opened to fix MQTT settings and timed out, or the user never
// picked a network -- the stored credentials must survive untouched.

// 802.11 limits: 32 bytes of SSID, 64 bytes of passphrase, both NUL-terminated.
struct WifiCred {
    char ssid[33];
    char pass[65];
};

// portal_ssid/portal_pass: what the config portal connected to ("" if none).
// stored_ssid/stored_pass: what is currently in our config.
WifiCred wifi_cred_after_portal(const char* portal_ssid, const char* portal_pass,
                                const char* stored_ssid, const char* stored_pass);

// Seconds still on the clock for the open config portal, as shown on the OLED.
// now_ms / start_ms are millis() readings; the subtraction is unsigned so it
// survives the ~49-day millis() wrap. Rounds up, so the screen shows "1" for
// the whole final second instead of "0" while the portal is still usable.
unsigned portal_seconds_left(unsigned long now_ms, unsigned long start_ms,
                             unsigned long timeout_s);
