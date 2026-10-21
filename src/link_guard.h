#pragma once

// The network watch-dog's decision table. Pure logic, no Arduino, so the
// "when do we stop relying on the core's own reconnect" questions are reachable
// from the native test suite.
//
// Why this exists: the gateway's WiFi link is dropped by the access point a few
// times a day (the 4G router re-keys its clients). The ESP32's implicit station
// auto-reconnect does not always bring the association back, and the firmware
// had no runtime recovery of its own -- `net_loop()` just returns while
// `WiFi.status() != WL_CONNECTED`. The result was a gateway that the broker
// dropped for keepalive silence and that then stayed away for minutes to hours
// (7h40m observed), with `ac/cmd` unable to reach the AC for that whole window.
//
// Escalation, measured from the last time the WHOLE path was up (WiFi
// associated AND an MQTT session established -- both failure shapes break that):
//
//   1. idle          before redial_after_ms        the link may heal itself
//   2. redial        then, at most every redial_every_ms
//   3. reboot        at reboot_after_ms            nothing else is left

enum LinkGuardAction {
    LINK_GUARD_IDLE = 0,
    LINK_GUARD_REDIAL,
    LINK_GUARD_REBOOT
};

// All times are millis() readings. Every field is `unsigned long` on purpose:
// each subtraction is unsigned, so this survives the ~49-day millis() wrap.
struct LinkGuardInput {
    unsigned long now_ms;           // millis() right now
    unsigned long last_healthy_ms;  // last time WiFi + MQTT were both up
    unsigned long last_redial_ms;   // last forced dial (0 = never)
    unsigned long redial_after_ms;  // silence before the first forced dial
    unsigned long redial_every_ms;  // and the minimum spacing between dials
    unsigned long reboot_after_ms;  // silence before giving up on dialling
};

enum LinkGuardAction link_guard_action(const LinkGuardInput& in);
