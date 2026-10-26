#include "boot_diag.h"

#include <Arduino.h>
#include <esp_attr.h>     // RTC_NOINIT_ATTR
#include <esp_system.h>   // esp_reset_reason()
#include <stdio.h>

#include "display.h"
#include "reset_info.h"

// Only there to tell "never written" from "written": RTC_NOINIT memory holds
// whatever was in it across a reset, and after a power-up that is garbage.
static const uint32_t BOOT_MAGIC = 0xACB0075u;   // "AC BOOT"

RTC_NOINIT_ATTR static uint32_t s_magic;
RTC_NOINIT_ATTR static uint32_t s_boots;

static unsigned    s_boot_no      = 1;
static const char* s_reason       = "UNKNOWN";
static bool        s_power_cycled = false;

void boot_diag_begin() {
    s_power_cycled = (s_magic != BOOT_MAGIC);
    if (s_power_cycled) {
        s_magic = BOOT_MAGIC;
        s_boots = 0;
    }
    ++s_boots;
    s_boot_no = (unsigned)s_boots;
    s_reason  = reset_reason_name((int)esp_reset_reason());
}

unsigned    boot_diag_count()        { return s_boot_no; }
const char* boot_diag_reason()       { return s_reason; }
bool        boot_diag_power_cycled() { return s_power_cycled; }

void boot_diag_screen(const char* phase) {
    char count_line[16];
    char reason_line[24];
    snprintf(count_line, sizeof(count_line), "boot #%u", s_boot_no);
    snprintf(reason_line, sizeof(reason_line), "rst %s", s_reason);
    // Same title/phase lines as before this existed, so the screen someone
    // describes over the phone still reads the same -- only the two bottom
    // lines are new.
    display_lines("AirCon Gateway", phase, count_line, reason_line);
}
