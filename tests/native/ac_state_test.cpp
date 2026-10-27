// Native unit tests for ac_state.
//   g++ -std=c++17 -I src tests/native/ac_state_test.cpp src/ac_state.cpp src/ac_encoder.cpp -o
//   build/ac_state_test
#include <cstdio>
#include "ac_state.h"

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
    // Unknown field is rejected and state is untouched.
    {
        AcState s;
        bool ok = ac_state_apply_field(s, "bogus", 5);
        expect("unknown field rejected", ok == false);
        expect("unknown field leaves every state value unchanged",
               s.power == 0 && s.mode == 1 && s.temp == 24 && s.fan == 0);
    }
    // main.cpp relies on recognition, not value equality, to decide whether
    // to persist and send a full frame for an accepted command.
    {
        AcState s;
        expect("same temperature is still recognized", ac_state_apply_field(s, "temp", 24));
        expect("same power is still recognized", ac_state_apply_field(s, "power", 0));
        expect("same mode is still recognized", ac_state_apply_field(s, "mode", 1));
        expect("same fan is still recognized", ac_state_apply_field(s, "fan", 0));
        expect("same fields preserve values",
               s.power == 0 && s.mode == 1 && s.temp == 24 && s.fan == 0);
    }
    {
        AcState s;
        ac_state_apply_field(s, "temp", -1);
        ac_state_apply_field(s, "mode", -1);
        ac_state_apply_field(s, "fan", -1);
        ac_state_apply_field(s, "power", -1);
        expect("negative values clamp before narrowing to bytes",
               s.temp == 16 && s.mode == 0 && s.fan == 0 && s.power == 1);
    }
    // Temperature is clamped to [16, 30].
    {
        AcState s;
        ac_state_apply_field(s, "temp", 99);
        expect("temp 99 -> 30", s.temp == AC_TEMP_MAX);
        ac_state_apply_field(s, "temp", 1);
        expect("temp 1 -> 16", s.temp == AC_TEMP_MIN);
        ac_state_apply_field(s, "temp", 24);
        expect("temp 24 stays", s.temp == 24);
    }
    // Mode clamps to [0, 4].
    {
        AcState s;
        ac_state_apply_field(s, "mode", 7);
        expect("mode 7 -> 4", s.mode == AC_MODE_HEAT);
    }
    // Fan clamps to [0, 3].
    {
        AcState s;
        ac_state_apply_field(s, "fan", 9);
        expect("fan 9 -> 3", s.fan == AC_FAN_HIGH);
    }
    // Power is normalised to 0/1.
    {
        AcState s;
        ac_state_apply_field(s, "power", 2);
        expect("power 2 -> ON", s.power == AC_POWER_ON);
        ac_state_apply_field(s, "power", 0);
        expect("power 0 -> OFF", s.power == AC_POWER_OFF);
    }
    // A sequence of partial updates yields the last value (Review Focus).
    {
        AcState s;
        ac_state_apply_field(s, "temp", 20);
        ac_state_apply_field(s, "temp", 26);
        ac_state_apply_field(s, "temp", 22);
        expect("last temp wins", s.temp == 22);
    }

    if (g_failures == 0) {
        printf("\nALL PASS\n");
        return 0;
    }
    printf("\n%d FAILURE(S)\n", g_failures);
    return 1;
}
