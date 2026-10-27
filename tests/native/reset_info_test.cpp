// Native unit tests for reset_info — the reset-reason decoder.
//
// Not a regression guard for a bug: this is the tool that tells a brown-out
// apart from a watch-dog reset apart from a plain power-up, on a device whose
// only surviving output is a 128x64 screen nobody has a cable attached to at
// the time it matters. A wrong or too-long name there is worse than no name, so
// both the mapping and the on-screen width are pinned here.
//
//   g++ -std=c++17 -I src tests/native/reset_info_test.cpp src/reset_info.cpp -o
//   build/reset_info_test
#include <cstdio>
#include <cstring>

#include "reset_info.h"

static int g_failures = 0;

static void expect(const char* name, bool cond) {
    if (cond) {
        printf("PASS %s\n", name);
        return;
    }
    printf("FAIL %s\n", name);
    ++g_failures;
}

int main() {
    // The codes that actually turn up on this gateway. Each one means something
    // different to whoever is standing in front of the OLED: EXT is "someone
    // pressed the button" (or the auto-reset circuit did), BROWNOUT is "the
    // supply cannot take the load", PANIC is "our code faulted".
    expect("power-on", strcmp(reset_reason_name(1), "POWERON") == 0);
    expect("reset button / EN pin", strcmp(reset_reason_name(2), "EXT") == 0);
    expect("our own restart", strcmp(reset_reason_name(3), "SW") == 0);
    expect("panic", strcmp(reset_reason_name(4), "PANIC") == 0);
    expect("interrupt watchdog", strcmp(reset_reason_name(5), "INT_WDT") == 0);
    expect("task watchdog", strcmp(reset_reason_name(6), "TASK_WDT") == 0);
    expect("brown-out", strcmp(reset_reason_name(9), "BROWNOUT") == 0);
    expect("deep sleep wake", strcmp(reset_reason_name(8), "DEEPSLEEP") == 0);
    expect("unknown (0)", strcmp(reset_reason_name(0), "UNKNOWN") == 0);

    // Anything the core grows later, and any garbage RTC read, must stay short
    // and printable rather than indexing past the table.
    expect("out of range -> ?", strcmp(reset_reason_name(11), "?") == 0);
    expect("negative -> ?", strcmp(reset_reason_name(-1), "?") == 0);

    // The screen prints "rst <name>" on one 6x12 line: 128 px / 6 px per char is
    // 21 characters, minus "rst " leaves 17. Enforced here because the failure
    // mode (a name silently running off the screen) is invisible on the host.
    bool all_short = true;
    for (int code = -2; code <= 12; ++code) {
        const char* n = reset_reason_name(code);
        if (!n || strlen(n) > 17) all_short = false;
    }
    expect("every name fits 'rst <name>'", all_short);

    if (g_failures) {
        printf("%d test(s) FAILED\n", g_failures);
        return 1;
    }
    printf("all reset_info tests passed\n");
    return 0;
}
