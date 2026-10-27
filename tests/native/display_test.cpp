#include <cstdio>
#include <cstdlib>
#include "../../src/display.cpp"

static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static bool has_text(const char* text) {
    for (const auto& item : u8g2.text)
        if (item.text == text) return true;
    return false;
}

int main() {
    s_info.ip = "255.255.255.255";
    s_info.net_state = "MQTT?";
    s_info.ac.temp = 30;
    for (int failsafe : {0, 1}) {
        s_info.failsafe = failsafe;
        for (int sensor : {0, 1}) {
            s_info.env_ok = sensor;
            s_info.room_c = -40;
            s_info.humidity = 100;
            for (auto page : s_pages) {
                u8g2.clearBuffer();
                page(0);
                for (const auto& item : u8g2.text) {
                    require(item.x >= 0 && item.x + item.width <= 128, "text fits width");
                    require(item.y > 0 && item.y <= 64, "text baseline fits height");
                }
                if (failsafe) require(has_text("SAFE OFF"), "protection visible on every page");
            }
        }
    }
    s_ok = true;
    s_manual = false;
    s_lastSwitch = millis();
    s_anim = 3;
    s_lastFrame = 0;
    display_tick();
    require(u8g2.clips >= 2, "sliding pages have independent clipping");
    display_lines("AirCon Gateway", "starting...", "boot #12", "rst SW");
    require(has_text("boot #12") && has_text("rst SW"), "boot diagnostics preserved");
    require(s_manual, "manual screens keep pager ownership");
    std::puts("PASS: display layout, persistent safety badge, clipping and diagnostics");
}
