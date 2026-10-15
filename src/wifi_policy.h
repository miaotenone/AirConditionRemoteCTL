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

// Absolute deadline for the config portal, measured from the first time it
// opened. The slider above restarts whenever a client is associated, so on its
// own it lets a phone someone walked away from pin the device in the portal
// forever -- no MQTT, no AC control. This ceiling never restarts.
bool portal_hard_timeout(unsigned long now_ms, unsigned long opened_ms,
                         unsigned long cap_s);

// Port number typed into the portal. "" / unparsable / out of range / trailing
// junk keep the previous value rather than persisting a broken-looking setting.
// NOTE: 0 is rejected too, which is stricter than the old (uint16_t)atoi() --
// a blank port used to be persisted as 0.
unsigned short portal_port_or(const char* text, unsigned short fallback);

// AC brand typed into the portal, as hex, 1..255. Anything else keeps the
// previous value (0 used to mean the same, and a wider value was silently
// truncated to a byte).
unsigned char portal_brand_or(const char* text, unsigned char fallback);

// Copy a WiFi credentials field out of the IDF's fixed-size array. `src_len` is
// the width of that array (32 for an SSID, 64 for a passphrase) because at full
// length it holds NO NUL terminator -- the core's own WiFi.SSID()/psk()
// accessors strlen() it and read past the end. This never reads src past
// src_len, stops at a NUL if one is there, and always terminates dst, so dst
// must be at least src_len + 1 bytes.
void wifi_field_copy(char* dst, size_t dst_size, const char* src, size_t src_len);
