// Native unit tests for link_guard — the network watch-dog's decision table.
//
// This is the regression guard for the "gateway goes silent and never comes
// back" bug. The access point re-keys its clients a few times a day; the ESP32's
// implicit station auto-reconnect does not always follow, and the firmware had
// no runtime recovery at all, so the broker saw a gateway that was silent for
// minutes to hours (7h40m observed) and the AC was unreachable for that whole
// window. The decision of *when* to force a fresh WiFi dial and *when* to give
// up and reboot used to not exist; keeping it pure is what makes it testable.
//
//   g++ -std=c++17 -I src tests/native/link_guard_test.cpp src/link_guard.cpp -o
//   build/link_guard_test
#include <cstdio>

#include "link_guard.h"

static int g_failures = 0;

static void expect(const char* name, bool cond) {
    if (cond) {
        printf("PASS %s\n", name);
        return;
    }
    printf("FAIL %s\n", name);
    ++g_failures;
}

// One place for the thresholds so the cases read as "15s of silence" instead of
// "15000" -- the values must match config.h, they are not the thing under test.
static const unsigned long AFTER = 15000UL;   // NET_REDIAL_AFTER_MS
static const unsigned long EVERY = 15000UL;   // NET_REDIAL_EVERY_MS
static const unsigned long REBOOT = 600000UL; // NET_REBOOT_AFTER_MS

static LinkGuardAction action(unsigned long now, unsigned long healthy, unsigned long redial,
                              unsigned long after = AFTER, unsigned long every = EVERY,
                              unsigned long reboot = REBOOT) {
    LinkGuardInput in = {now, healthy, redial, after, every, reboot};
    return link_guard_action(in);
}

int main() {
    // A link that just went quiet is left alone: an AP hiccup that heals itself
    // must not turn into a re-association storm.
    {
        expect("fresh link -> idle", action(1000, 0, 0) == LINK_GUARD_IDLE);
        expect("14.9s of silence -> still idle", action(14999, 0, 0) == LINK_GUARD_IDLE);
    }

    // 15s is the point where we take over from the core's own reconnect. It is
    // deliberately shorter than the broker's 1.5 x keepalive (15s -> 22.5s)
    // window, so a successful dial usually beats the broker's "exceeded
    // timeout" and the card never even flashes offline.
    {
        expect("exactly 15s of silence -> redial", action(15000, 0, 0) == LINK_GUARD_REDIAL);
    }

    // ... but not more often than NET_REDIAL_EVERY_MS. Hammering WiFi.begin()
    // while the AP is genuinely away does not help and delays the association
    // that is already in flight.
    {
        expect("redialled 5s ago -> wait", action(20000, 0, 15000) == LINK_GUARD_IDLE);
        expect("redialled 15s ago -> redial again", action(30000, 0, 15000) == LINK_GUARD_REDIAL);
    }

    // One second short of the reboot deadline we are still trying to recover
    // without a restart.
    {
        expect("10min - 1ms, never redialled -> redial",
               action(REBOOT - 1, 0, 0) == LINK_GUARD_REDIAL);
    }

    // Past the deadline the escalation wins even if a dial just went out: the
    // redials have had their chance.
    {
        expect("exactly 10min -> reboot", action(REBOOT, 0, REBOOT - 1000) == LINK_GUARD_REBOOT);
        expect("30min of silence, dialling throughout -> reboot",
               action(1800000, 0, 1799000) == LINK_GUARD_REBOOT);
    }

    // millis() wraps after ~49 days and the firmware is meant to run for
    // months, so every subtraction here is unsigned and must survive the wrap.
    // base sits just below the wrap; base + 20000 lands past 0.
    {
        unsigned long base = 0xFFFFF000UL;
        unsigned long now = base + 20000UL;
        expect("wrap: healthy 20s ago -> redial", action(now, base, 0) == LINK_GUARD_REDIAL);

        now = base + 8000UL;
        expect("wrap: healthy 8s ago -> idle", action(now, base, 0) == LINK_GUARD_IDLE);

        now = base + 30000UL;
        expect("wrap: last redial 10s ago -> wait",
               action(now, base, base + 20000UL) == LINK_GUARD_IDLE);
        expect("wrap: last redial 20s ago -> redial",
               action(now, base, base + 10000UL) == LINK_GUARD_REDIAL);
    }

    // A link that was healthy right now is idle no matter where the last redial
    // sits — the reboot deadline is measured from the last HEALTHY moment, not
    // from the last action.
    {
        expect("healthy now -> idle", action(999999, 999999, 500000) == LINK_GUARD_IDLE);
    }

    if (g_failures) {
        printf("%d failure(s)\n", g_failures);
        return 1;
    }
    printf("all link_guard tests passed\n");
    return 0;
}
