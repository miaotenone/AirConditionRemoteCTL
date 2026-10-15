// Native unit tests for wifi_policy — what credentials to persist after the
// config portal closed. This is the regression guard for the "moved the device
// and it kept reconnecting to the old network forever" bug: the portal
// connects to the NEW network, so the NEW network is what must be written back.
//
//   g++ -std=c++17 -I src tests/native/wifi_policy_test.cpp src/wifi_policy.cpp -o build/wifi_policy_test
#include <cstdio>
#include <cstring>
#include "wifi_policy.h"

static int g_failures = 0;

static void expect(const char* name, bool cond) {
    if (cond) { printf("PASS %s\n", name); return; }
    printf("FAIL %s\n", name);
    ++g_failures;
}

static bool same(const char* a, const char* b) { return strcmp(a, b) == 0; }

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
        char long_ssid[80]; memset(long_ssid, 'S', sizeof(long_ssid) - 1); long_ssid[79] = '\0';
        char long_pass[100]; memset(long_pass, 'P', sizeof(long_pass) - 1); long_pass[99] = '\0';
        WifiCred c = wifi_cred_after_portal(long_ssid, long_pass, "", "");
        expect("long ssid truncated to 32", strlen(c.ssid) == 32);
        expect("long password truncated to 64", strlen(c.pass) == 64);
    }

    // --- portal countdown shown on the OLED ---
    // Ceiling, so the last second reads "1" rather than sitting on "0" while
    // the portal is still usable.
    expect("countdown starts at the full timeout",
           portal_seconds_left(0, 0, 180) == 180);
    expect("countdown ticks down",
           portal_seconds_left(1000, 0, 180) == 179);
    expect("countdown rounds up mid-second",
           portal_seconds_left(500, 0, 180) == 180);
    expect("countdown last second",
           portal_seconds_left(179500, 0, 180) == 1);
    expect("countdown reaches zero at the timeout",
           portal_seconds_left(180000, 0, 180) == 0);
    expect("countdown never goes negative",
           portal_seconds_left(500000, 0, 180) == 0);
    // A phone attached resets t0, so the portal gets its full window back.
    // Without the bump, the portal would already be past its deadline here.
    expect("countdown after a client attaches",
           portal_seconds_left(180000, 179000, 180) == 179);
    // millis() wraps every ~49 days; the unsigned subtraction must survive it.
    // `now` is computed with the same arithmetic as the code, so the elapsed
    // time is exactly 20 s whether unsigned long is 32- or 64-bit wide.
    {
        unsigned long start = 0xFFFFFF00UL;
        unsigned long now   = start + 20000UL;
        expect("countdown survives the millis() wrap",
               portal_seconds_left(now, start, 180) == 160);
    }

    if (g_failures) { printf("\n%d failure(s)\n", g_failures); return 1; }
    printf("\nall wifi_policy tests passed\n");
    return 0;
}
