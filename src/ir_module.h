#pragma once
#include <stdint.h>
#include <stddef.h>

// Bring up the UART that talks to the high-config IR module.
void ir_module_begin();

// Send a complete protocol frame.
void ir_module_send(const uint8_t* frame, size_t len);
