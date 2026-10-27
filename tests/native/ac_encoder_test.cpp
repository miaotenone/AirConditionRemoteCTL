// Native unit tests for ac_encoder (no Arduino required).
// Build & run:
//   g++ -std=c++17 -I src tests/native/ac_encoder_test.cpp src/ac_encoder.cpp -o
//   build/ac_encoder_test
//   ./build/ac_encoder_test
#include <cstdio>
#include <cstring>
#include "ac_encoder.h"

static int g_failures = 0;

static void check_bytes(const char* name, const uint8_t* got, const uint8_t* want, size_t n) {
    if (memcmp(got, want, n) == 0) {
        printf("PASS %s\n", name);
        return;
    }
    printf("FAIL %s\n  got :", name);
    for (size_t i = 0; i < n; ++i)
        printf(" %02X", got[i]);
    printf("\n  want:");
    for (size_t i = 0; i < n; ++i)
        printf(" %02X", want[i]);
    printf("\n");
    ++g_failures;
}

static void check_u8(const char* name, uint8_t got, uint8_t want) {
    if (got == want) {
        printf("PASS %s\n", name);
        return;
    }
    printf("FAIL %s: got %02X want %02X\n", name, got, want);
    ++g_failures;
}

static void test_multi_param_midea_cool_24_auto() {
    uint8_t out[MULTI_PARAM_FRAME_SIZE] = {0};
    size_t n =
        build_multi_param_frame(out, BRAND_MIDEA1, AC_POWER_ON, AC_MODE_COOL, 24, AC_FAN_AUTO);
    const uint8_t want[] = {0x68, 0x0C, 0x00, 0xFF, 0x30, 0x10, 0x01, 0x01, 0x18, 0x00, 0x55, 0x16};
    if (n != MULTI_PARAM_FRAME_SIZE) {
        printf("FAIL multi size=%zu\n", n);
        ++g_failures;
    }
    check_bytes("multi midea cool 24 auto", out, want, sizeof(want));
}

static void test_multi_param_clamps_temp_and_mode() {
    uint8_t out[MULTI_PARAM_FRAME_SIZE] = {0};
    build_multi_param_frame(out, BRAND_MIDEA1, AC_POWER_ON, 7 /*>HEAT*/, 99 /*>30*/, 9 /*>HIGH*/);
    const uint8_t want[] = {0x68, 0x0C, 0x00, 0xFF, 0x30, 0x10, 0x01, 0x04, 0x1E, 0x03, 0x55, 0x16};
    check_bytes("multi clamps out-of-range", out, want, sizeof(want));
}

static void test_single_param_temp() {
    uint8_t out[SINGLE_PARAM_FRAME_SIZE] = {0};
    size_t n = build_single_param_frame(out, PARAM_TEMP, 24);
    const uint8_t want[] = {0x68, 0x09, 0x00, 0xFF, 0x31, 0x03, 0x18, 0x55, 0x16};
    if (n != SINGLE_PARAM_FRAME_SIZE) {
        printf("FAIL single size=%zu\n", n);
        ++g_failures;
    }
    check_bytes("single temp 24", out, want, sizeof(want));
}

static void test_learn_and_send_frames() {
    uint8_t learn[LEARN_FRAME_SIZE] = {0};
    size_t n1 = build_learn_frame(learn, 0);
    const uint8_t wantL[] = {0x68, 0x08, 0x00, 0xFF, 0x10, 0x00, 0x0F, 0x16};
    if (n1 != LEARN_FRAME_SIZE) {
        printf("FAIL learn size=%zu\n", n1);
        ++g_failures;
    }
    check_bytes("learn slot 0", learn, wantL, sizeof(wantL));

    uint8_t send[LEARN_FRAME_SIZE] = {0};
    size_t n2 = build_send_learned_frame(send, 0);
    const uint8_t wantS[] = {0x68, 0x08, 0x00, 0xFF, 0x12, 0x00, 0x11, 0x16};
    if (n2 != LEARN_FRAME_SIZE) {
        printf("FAIL send size=%zu\n", n2);
        ++g_failures;
    }
    check_bytes("send learned slot 0", send, wantS, sizeof(wantS));

    uint8_t send6[LEARN_FRAME_SIZE] = {0};
    build_send_learned_frame(send6, 6);
    const uint8_t wantS6[] = {0x68, 0x08, 0x00, 0xFF, 0x12, 0x06, 0x17, 0x16};
    check_bytes("send learned slot 6", send6, wantS6, sizeof(wantS6));
}

static void test_checksum() {
    const uint8_t data[] = {0xFF, 0x10, 0x00};
    check_u8("checksum FF 10 00", frame_checksum(data, 3), 0x0F);
}

int main() {
    test_multi_param_midea_cool_24_auto();
    test_multi_param_clamps_temp_and_mode();
    test_single_param_temp();
    test_learn_and_send_frames();
    test_checksum();
    if (g_failures == 0) {
        printf("\nALL PASS\n");
        return 0;
    }
    printf("\n%d FAILURE(S)\n", g_failures);
    return 1;
}
