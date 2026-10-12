#pragma once
// Pure frame encoder for the zjiot (智家物联) high-config IR module.
// No Arduino / hardware dependencies so it can be unit tested natively.
#include <stdint.h>
#include <stddef.h>

// Frame delimiters and header fields.
constexpr uint8_t IR_FRAME_START = 0x68;
constexpr uint8_t IR_FRAME_END   = 0x16;
constexpr uint8_t IR_MODULE_ADDR = 0xFF;

// Function codes (AFN).
constexpr uint8_t AFN_LEARN        = 0x10;  // enter internal-storage learn mode
constexpr uint8_t AFN_SEND_LEARNED = 0x12;  // replay internal-storage slot
constexpr uint8_t AFN_MULTI_PARAM  = 0x30;  // full-state AC control
constexpr uint8_t AFN_SINGLE_PARAM = 0x31;  // single-parameter AC control

// Frame lengths (value of the 2-byte length field; also total byte count).
constexpr uint8_t MULTI_PARAM_LEN  = 0x0C;
constexpr uint8_t SINGLE_PARAM_LEN = 0x09;
constexpr uint8_t LEARN_LEN        = 0x08;

// The multi/single parameter frames use a fixed checksum.
constexpr uint8_t IR_CHECKSUM_FIXED = 0x55;

// AC brands (subset; see docs/protocol.md for the full table).
constexpr uint8_t BRAND_GREE1   = 0x00;
constexpr uint8_t BRAND_MIDEA1  = 0x10;
constexpr uint8_t BRAND_MIDEA2  = 0x11;
constexpr uint8_t BRAND_MIDEA3  = 0x12;
constexpr uint8_t BRAND_HAIER1  = 0x20;
constexpr uint8_t BRAND_TCL     = 0x30;
constexpr uint8_t BRAND_AUX     = 0x40;

// Power.
constexpr uint8_t AC_POWER_OFF = 0x00;
constexpr uint8_t AC_POWER_ON  = 0x01;

// Mode.
constexpr uint8_t AC_MODE_AUTO = 0x00;
constexpr uint8_t AC_MODE_COOL = 0x01;
constexpr uint8_t AC_MODE_DRY  = 0x02;
constexpr uint8_t AC_MODE_FAN  = 0x03;
constexpr uint8_t AC_MODE_HEAT = 0x04;

// Fan.
constexpr uint8_t AC_FAN_AUTO = 0x00;
constexpr uint8_t AC_FAN_LOW  = 0x01;
constexpr uint8_t AC_FAN_MED  = 0x02;
constexpr uint8_t AC_FAN_HIGH = 0x03;

// Temperature bounds (celsius; the byte value equals the temperature).
constexpr uint8_t AC_TEMP_MIN = 16;
constexpr uint8_t AC_TEMP_MAX = 30;

// Resulting frame sizes.
constexpr size_t MULTI_PARAM_FRAME_SIZE  = 12;
constexpr size_t SINGLE_PARAM_FRAME_SIZE = 9;
constexpr size_t LEARN_FRAME_SIZE        = 8;

// Single-parameter indices.
constexpr uint8_t PARAM_BRAND = 0x00;
constexpr uint8_t PARAM_POWER = 0x01;
constexpr uint8_t PARAM_MODE  = 0x02;
constexpr uint8_t PARAM_TEMP  = 0x03;
constexpr uint8_t PARAM_FAN   = 0x04;

// Sum of bytes, modulo 256.
uint8_t frame_checksum(const uint8_t* data, size_t len);

// 68 0C 00 FF 30 brand power mode temp fan 55 16  -> returns 12.
size_t build_multi_param_frame(uint8_t* out, uint8_t brand, uint8_t power,
                               uint8_t mode, uint8_t temp, uint8_t fan);

// 68 09 00 FF 31 index value 55 16 -> returns 9.
size_t build_single_param_frame(uint8_t* out, uint8_t param_index, uint8_t value);

// 68 08 00 FF 10 slot checksum 16 -> returns 8.
size_t build_learn_frame(uint8_t* out, uint8_t slot);

// 68 08 00 FF 12 slot checksum 16 -> returns 8.
size_t build_send_learned_frame(uint8_t* out, uint8_t slot);
