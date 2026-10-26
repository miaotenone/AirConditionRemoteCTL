#include "reset_info.h"

// esp_reset_reason_t, spelled out rather than included: esp_system.h is not
// available to the host build, and the numbers ARE the ABI of this function.
const char* reset_reason_name(int code) {
    switch (code) {
    case 0:  return "UNKNOWN";    // ESP_RST_UNKNOWN
    case 1:  return "POWERON";    // ESP_RST_POWERON: a real power-up
    case 2:  return "EXT";        // ESP_RST_EXT: the EN/RST pin, i.e. the button
    case 3:  return "SW";         // ESP_RST_SW: our own ESP.restart()
    case 4:  return "PANIC";      // ESP_RST_PANIC: an exception / abort
    case 5:  return "INT_WDT";    // ESP_RST_INT_WDT
    case 6:  return "TASK_WDT";   // ESP_RST_TASK_WDT
    case 7:  return "WDT";        // ESP_RST_WDT: any other watch-dog
    case 8:  return "DEEPSLEEP";  // ESP_RST_DEEPSLEEP
    case 9:  return "BROWNOUT";   // ESP_RST_BROWNOUT: the supply sagged
    case 10: return "SDIO";       // ESP_RST_SDIO
    default: return "?";
    }
}
