#include "wifi_policy.h"
#include <string.h>

static void copy_trunc(char* dst, size_t dst_size, const char* src) {
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

WifiCred wifi_cred_after_portal(const char* portal_ssid, const char* portal_pass,
                                const char* stored_ssid, const char* stored_pass) {
    WifiCred out;
    // Whatever the portal connected to is the truth. Only when it reports no
    // connection at all do we keep what we had.
    bool got = portal_ssid && portal_ssid[0];
    copy_trunc(out.ssid, sizeof(out.ssid), got ? portal_ssid : stored_ssid);
    copy_trunc(out.pass, sizeof(out.pass), got ? portal_pass : stored_pass);
    return out;
}

unsigned portal_seconds_left(unsigned long now_ms, unsigned long start_ms,
                             unsigned long timeout_s) {
    unsigned long total_ms = timeout_s * 1000UL;
    unsigned long spent    = now_ms - start_ms;   // unsigned: wraps like millis()
    if (spent >= total_ms) return 0;
    // Ceiling division: the screen reads "1" throughout the final second.
    return (unsigned)((total_ms - spent + 999UL) / 1000UL);
}
