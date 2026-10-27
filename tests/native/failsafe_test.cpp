// Pure decision tests: firmware side effects stay in main.cpp.
#include <cstdio>
#include <cstdint>
#include "failsafe.h"

static int failures = 0;

static void expect(const char* name, FailsafeAction got, FailsafeAction want) {
    if (got == want) {
        std::printf("PASS %s\n", name);
    } else {
        std::printf("FAIL %s\n", name);
        ++failures;
    }
}

int main() {
    constexpr uint32_t timeout = 300000;
    FailsafeStatus status;
    expect("boot without contact never turns off AC",
           failsafe_action(status, false, 900000, timeout), FailsafeAction::none);
    expect("first whole-path contact arms protection", failsafe_action(status, true, 1000, timeout),
           FailsafeAction::arm);

    status.armed = true;
    status.last_contact_ms = 1000;
    expect("healthy contact does not repeat arming", failsafe_action(status, true, 2000, timeout),
           FailsafeAction::none);
    expect("one ms before deadline remains idle", failsafe_action(status, false, 300999, timeout),
           FailsafeAction::none);
    expect("exact five minute deadline turns off", failsafe_action(status, false, 301000, timeout),
           FailsafeAction::turn_off);

    status.tripped = true;
    expect("continued outage never repeats IR off", failsafe_action(status, false, 999999, timeout),
           FailsafeAction::none);
    expect("whole-path recovery publishes restored state",
           failsafe_action(status, true, 999999, timeout), FailsafeAction::restore);

    status.tripped = false;
    status.last_contact_ms = UINT32_MAX - 999;
    expect("millis wrap one ms before deadline", failsafe_action(status, false, 298999, timeout),
           FailsafeAction::none);
    expect("millis wrap exact deadline", failsafe_action(status, false, 299000, timeout),
           FailsafeAction::turn_off);

    std::printf("%d failsafe failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
