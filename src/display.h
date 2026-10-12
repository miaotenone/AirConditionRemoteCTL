#pragma once
#include "ac_state.h"

void display_begin();

// Four-line status screen.
void display_lines(const char* l1, const char* l2, const char* l3, const char* l4);

// Convenience screen for the current AC + connection state.
void display_show_ac(const AcState& s, const char* net_state, const char* ip);
