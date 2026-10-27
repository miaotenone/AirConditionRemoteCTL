#include "ac_encoder.h"

uint8_t frame_checksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i) {
        sum += data[i];
    }
    return sum;
}

size_t build_multi_param_frame(uint8_t* out, uint8_t brand, uint8_t power, uint8_t mode,
                               uint8_t temp, uint8_t fan) {
    // Normalise / clamp inputs so we never emit an out-of-range frame.
    power = power ? AC_POWER_ON : AC_POWER_OFF;
    if (mode > AC_MODE_HEAT) mode = AC_MODE_HEAT;
    if (temp < AC_TEMP_MIN) temp = AC_TEMP_MIN;
    if (temp > AC_TEMP_MAX) temp = AC_TEMP_MAX;
    if (fan > AC_FAN_HIGH) fan = AC_FAN_HIGH;

    out[0] = IR_FRAME_START;
    out[1] = MULTI_PARAM_LEN;
    out[2] = 0x00;
    out[3] = IR_MODULE_ADDR;
    out[4] = AFN_MULTI_PARAM;
    out[5] = brand;
    out[6] = power;
    out[7] = mode;
    out[8] = temp; // 16..30 -> 0x10..0x1E
    out[9] = fan;
    out[10] = IR_CHECKSUM_FIXED;
    out[11] = IR_FRAME_END;
    return MULTI_PARAM_FRAME_SIZE;
}

size_t build_single_param_frame(uint8_t* out, uint8_t param_index, uint8_t value) {
    out[0] = IR_FRAME_START;
    out[1] = SINGLE_PARAM_LEN;
    out[2] = 0x00;
    out[3] = IR_MODULE_ADDR;
    out[4] = AFN_SINGLE_PARAM;
    out[5] = param_index;
    out[6] = value;
    out[7] = IR_CHECKSUM_FIXED;
    out[8] = IR_FRAME_END;
    return SINGLE_PARAM_FRAME_SIZE;
}

size_t build_learn_frame(uint8_t* out, uint8_t slot) {
    out[0] = IR_FRAME_START;
    out[1] = LEARN_LEN;
    out[2] = 0x00;
    out[3] = IR_MODULE_ADDR;
    out[4] = AFN_LEARN;
    out[5] = slot;
    out[6] = frame_checksum(&out[3], 3);
    out[7] = IR_FRAME_END;
    return LEARN_FRAME_SIZE;
}

size_t build_send_learned_frame(uint8_t* out, uint8_t slot) {
    out[0] = IR_FRAME_START;
    out[1] = LEARN_LEN;
    out[2] = 0x00;
    out[3] = IR_MODULE_ADDR;
    out[4] = AFN_SEND_LEARNED;
    out[5] = slot;
    out[6] = frame_checksum(&out[3], 3);
    out[7] = IR_FRAME_END;
    return LEARN_FRAME_SIZE;
}
