#include "wifi_policy.h"
#include <string.h>
#include <stdlib.h>   // strtol

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

bool portal_hard_timeout(unsigned long now_ms, unsigned long opened_ms,
                         unsigned long cap_s) {
    return (now_ms - opened_ms) >= cap_s * 1000UL;
}

unsigned short portal_port_or(const char* text, unsigned short fallback) {
    if (!text || !text[0]) return fallback;
    char* end = NULL;
    long v = strtol(text, &end, 10);
    // Any trailing rubbish ("1883x") is a typo, not a number.
    if (end == text || (end && *end != '\0')) return fallback;
    if (v < 1 || v > 65535) return fallback;
    return (unsigned short)v;
}

unsigned char portal_brand_or(const char* text, unsigned char fallback) {
    if (!text || !text[0]) return fallback;
    char* end = NULL;
    long v = strtol(text, &end, 16);
    if (end == text || (end && *end != '\0')) return fallback;
    if (v < 1 || v > 255) return fallback;
    return (unsigned char)v;
}

void wifi_field_copy(char* dst, size_t dst_size, const char* src, size_t src_len) {
    if (!dst || dst_size == 0) return;
    if (!src) { dst[0] = '\0'; return; }

    size_t n = 0;
    // Bounded by src_len, never by a terminator that may not be there.
    while (n < src_len && n < dst_size - 1 && src[n] != '\0') ++n;
    memcpy(dst, src, n);
    dst[n] = '\0';
}
