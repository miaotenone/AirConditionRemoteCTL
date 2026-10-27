// Native unit tests for wifi_policy — what credentials to persist after the
// config portal closed. This is the regression guard for the "moved the device
// and it kept reconnecting to the old network forever" bug: the portal
// connects to the NEW network, so the NEW network is what must be written back.
//
//   g++ -std=c++17 -I src tests/native/wifi_policy_test.cpp src/wifi_policy.cpp -o
//   build/wifi_policy_test
#include <cstdio>
#include <cstring>
#include "wifi_policy.h"

static int g_failures = 0;

static void expect(const char* name, bool cond) {
    if (cond) {
        printf("PASS %s\n", name);
        return;
    }
    printf("FAIL %s\n", name);
    ++g_failures;
}

static bool same(const char* a, const char* b) {
    return strcmp(a, b) == 0;
}

int main() {
    // The portal connected to a different network than the one stored: the new
    // one wins. This is the exact case that used to be dropped on the floor.
    {
        WifiCred c = wifi_cred_after_portal("NewNet", "newpass123", "OldNet", "oldpass123");
        expect("moved: new ssid is kept", same(c.ssid, "NewNet"));
        expect("moved: new password is kept", same(c.pass, "newpass123"));
    }

    // First boot: nothing was stored, the portal did the whole job.
    {
        WifiCred c = wifi_cred_after_portal("HomeNet", "hunter2xx", "", "");
        expect("first boot: ssid from the portal", same(c.ssid, "HomeNet"));
        expect("first boot: password from the portal", same(c.pass, "hunter2xx"));
    }

    // The portal was opened only to fix MQTT settings and timed out without a
    // connection: it must NOT erase the network that was working.
    {
        WifiCred c = wifi_cred_after_portal("", "", "HomeNet", "hunter2xx");
        expect("no connection: stored ssid survives", same(c.ssid, "HomeNet"));
        expect("no connection: stored password survives", same(c.pass, "hunter2xx"));
    }

    // An open network has no passphrase. The ssid still has to be persisted,
    // otherwise the next boot goes through the portal again.
    {
        WifiCred c = wifi_cred_after_portal("CafeFreeWiFi", "", "OldNet", "oldpass123");
        expect("open network: ssid kept", same(c.ssid, "CafeFreeWiFi"));
        expect("open network: empty password kept", same(c.pass, ""));
    }

    // A password-only change on the same ssid must be persisted too.
    {
        WifiCred c = wifi_cred_after_portal("HomeNet", "brandnewpw", "HomeNet", "hunter2xx");
        expect("same ssid: new password kept", same(c.pass, "brandnewpw"));
    }

    // Nothing stored and no connection: stay empty rather than inventing one.
    {
        WifiCred c = wifi_cred_after_portal("", "", "", "");
        expect("nothing anywhere: ssid empty", same(c.ssid, ""));
        expect("nothing anywhere: password empty", same(c.pass, ""));
    }

    // Over-long input is truncated to the field sizes, never overflowing:
    // 32 bytes of ssid (the 802.11 maximum) and 64 bytes of passphrase.
    {
        char long_ssid[80];
        memset(long_ssid, 'S', sizeof(long_ssid) - 1);
        long_ssid[79] = '\0';
        char long_pass[100];
        memset(long_pass, 'P', sizeof(long_pass) - 1);
        long_pass[99] = '\0';
        WifiCred c = wifi_cred_after_portal(long_ssid, long_pass, "", "");
        expect("long ssid truncated to 32", strlen(c.ssid) == 32);
        expect("long password truncated to 64", strlen(c.pass) == 64);
    }

    // --- portal countdown shown on the OLED ---
    // Ceiling, so the last second reads "1" rather than sitting on "0" while
    // the portal is still usable.
    expect("countdown starts at the full timeout", portal_seconds_left(0, 0, 180) == 180);
    expect("countdown ticks down", portal_seconds_left(1000, 0, 180) == 179);
    expect("countdown rounds up mid-second", portal_seconds_left(500, 0, 180) == 180);
    expect("countdown last second", portal_seconds_left(179500, 0, 180) == 1);
    expect("countdown reaches zero at the timeout", portal_seconds_left(180000, 0, 180) == 0);
    expect("countdown never goes negative", portal_seconds_left(500000, 0, 180) == 0);
    // A phone attached resets t0, so the portal gets its full window back.
    // Without the bump, the portal would already be past its deadline here.
    expect("countdown after a client attaches", portal_seconds_left(180000, 179000, 180) == 179);
    // millis() wraps every ~49 days; the unsigned subtraction must survive it.
    // `now` is computed with the same arithmetic as the code, so the elapsed
    // time is exactly 20 s whether unsigned long is 32- or 64-bit wide.
    {
        unsigned long start = 0xFFFFFF00UL;
        unsigned long now = start + 20000UL;
        expect("countdown survives the millis() wrap", portal_seconds_left(now, start, 180) == 160);
    }

    // --- an attached client must not hold the portal open forever ---
    // The sliding clock above restarts whenever a station is associated, so a
    // phone someone walked away from would pin the device in the portal. This
    // absolute ceiling is measured from the first time the portal opened.
    expect("hard ceiling: still open early", portal_hard_timeout(100000, 0, 600) == false);
    expect("hard ceiling: fires at the cap", portal_hard_timeout(600000, 0, 600) == true);
    expect("hard ceiling: fires past the cap", portal_hard_timeout(900000, 0, 600) == true);
    expect("hard ceiling: survives the millis() wrap",
           portal_hard_timeout(0xFFFFFF00UL + 700000UL, 0xFFFFFF00UL, 600) == true);

    // --- values typed into the portal must not corrupt the config ---
    // atoi()/strtol() accept "" as 0 and let 99999 wrap a uint16_t, so a typo
    // on the phone would be persisted as a working-looking but wrong setting.
    expect("port: plain value", portal_port_or("1883", 1883) == 1883);
    expect("port: blank keeps the old one", portal_port_or("", 1883) == 1883);
    expect("port: garbage keeps the old one", portal_port_or("abc", 1883) == 1883);
    expect("port: zero keeps the old one", portal_port_or("0", 1883) == 1883);
    expect("port: out of range keeps the old one", portal_port_or("99999", 1883) == 1883);
    expect("port: highest legal value", portal_port_or("65535", 1883) == 65535);
    // Trailing rubbish is a typo, not the number it starts with. strtol would
    // happily return 1883 here, so this is the case the end-pointer guard is
    // for -- and the fallback differs from 1883, so a dropped guard is caught.
    expect("port: trailing junk keeps the old one", portal_port_or("1883x", 1884) == 1884);
    // One past the top of the range, with a fallback that differs from the
    // parsed value so a mutant returning the fallback blindly is caught.
    expect("port: 65536 keeps the old one", portal_port_or("65536", 1884) == 1884);
    // A legal value that differs from the fallback, so a mutant that ignores its
    // argument cannot pass this one.
    expect("port: a different legal value wins", portal_port_or("8883", 1883) == 8883);

    // Brand is hex, 1..255. 0 already meant "keep the old one" before, and
    // anything wider than a byte used to be silently truncated.
    expect("brand: hex value", (int)portal_brand_or("10", 16) == 0x10);
    expect("brand: hex is not decimal", (int)portal_brand_or("11", 16) == 0x11);
    expect("brand: blank keeps the old one", (int)portal_brand_or("", 16) == 16);
    expect("brand: zero keeps the old one", (int)portal_brand_or("0", 16) == 16);
    expect("brand: past a byte keeps the old one", (int)portal_brand_or("1FF", 16) == 16);
    expect("brand: garbage keeps the old one", (int)portal_brand_or("zz", 16) == 16);
    // Same end-pointer guard as the port, on the hex parse. 0x10 is 16, so the
    // fallback of 17 differs from what a dropped guard would return.
    expect("brand: trailing junk keeps the old one", (int)portal_brand_or("10z", 17) == 17);
    // A hex value that differs from the fallback, so a mutant that ignores its
    // argument cannot pass.
    expect("brand: a different legal value wins", (int)portal_brand_or("12", 16) == 0x12);

    // --- copying a credentials field that has no terminator ---
    // conf.sta.ssid[32] / .password[64] hold no NUL at full length, and the
    // core's own accessors strlen() them. So the copy has to be told how many
    // bytes are legal rather than looking for a terminator that is not there.
    {
        char raw[64];
        memset(raw, 'P', sizeof(raw)); // legal max, NOT terminated
        char dst[65];
        wifi_field_copy(dst, sizeof(dst), raw, sizeof(raw));
        expect("full-length field: kept whole", strlen(dst) == 64);
        expect("full-length field: terminated", dst[64] == '\0');

        char raw2[33];
        memset(raw2, 'S', sizeof(raw2));
        char dst2[33];
        wifi_field_copy(dst2, sizeof(dst2), raw2, sizeof(raw2));
        expect("full-length ssid: kept whole", strlen(dst2) == 32);

        // A short field is NUL-padded by the driver; stop at the NUL instead of
        // copying the padding. The bytes AFTER the terminator are deliberately
        // non-zero, so an implementation that ignores the terminator and copies
        // the full width fails this rather than passing by accident.
        char raw3[64];
        memset(raw3, 'X', sizeof(raw3));
        memcpy(raw3, "hunter2xx", 9);
        raw3[9] = '\0';
        char dst3[65];
        memset(dst3, 'Y', sizeof(dst3));
        wifi_field_copy(dst3, sizeof(dst3), raw3, sizeof(raw3));
        expect("short field: stops at the terminator", strlen(dst3) == 9);
        // The sentinel past the terminator is untouched, proving the copy
        // stopped at the NUL rather than running the full width.
        expect("short field: nothing written past the terminator", dst3[10] == 'Y');

        // An open network: the field is entirely zeroes.
        char raw4[64];
        memset(raw4, '\0', sizeof(raw4));
        char dst4[65];
        wifi_field_copy(dst4, sizeof(dst4), raw4, sizeof(raw4));
        expect("empty field: empty copy", dst4[0] == '\0');

        // A destination narrower than the source must still terminate and must
        // never write past dst_size.
        char raw5[64];
        memset(raw5, 'P', sizeof(raw5));
        char dst5[8];
        memset(dst5, 'Y', sizeof(dst5));
        wifi_field_copy(dst5, sizeof(dst5), raw5, sizeof(raw5));
        expect("narrow dst: truncated to dst_size - 1", strlen(dst5) == 7);
        expect("narrow dst: terminated inside the buffer", dst5[7] == '\0');
    }

    if (g_failures) {
        printf("\n%d failure(s)\n", g_failures);
        return 1;
    }
    printf("\nall wifi_policy tests passed\n");
    return 0;
}
