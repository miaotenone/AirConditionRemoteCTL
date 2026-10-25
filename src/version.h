// Firmware identity -- printed on the boot banner so a plugged-in unit can say
// which build it is. Nothing else on the device does: the OLED shows live state,
// MQTT carries live state, and NVS carries the remembered AC state, but none of
// them can answer "which commit am I?" (found 2026-10-25 when checking whether
// the flashed build was current).
#pragma once

//: The git revision this build came from, injected by the build:
//: scripts/git_version.py (wired in platformio.ini) appends
//: ``-DFW_GIT_HASH="<short hash>"``, with a ``-dirty`` suffix when the tree had
//: uncommitted changes at build time. The fallback keeps host (native) test
//: builds compiling -- they never print the banner.
#ifndef FW_GIT_HASH
#define FW_GIT_HASH "dev"
#endif

//: Human tag for the design generation this build implements (the design docs
//: speak in v1.1 / v1.2). Bump it when the behaviour changes, so a banner read
//: off the serial port is enough to place the build.
#define FW_TAG "v1.2"
