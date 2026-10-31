#include <cstdio>
#include <cstdlib>
#include <climits>
#include "../../src/display.cpp"
#include "../../src/buttons.cpp"

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

static void require_layout() {
    for (const auto& item : u8g2.text) {
        require(item.x >= 0 && item.x + item.width <= 128, "diagnostic text fits width");
        require(item.y > 0 && item.y <= 64, "diagnostic baseline fits height");
    }
}

static void navigation_tests() {
    DisplayInfo info;
    info.ac.temp = 24;
    info.ip = "255.255.255.255";
    info.net_state = "OK";
    info.wifi_connected = true;
    info.mqtt_connected = true;
    info.boot_count = UINT_MAX;
    info.boot_reason = "POWERON";
    s_nav.reset(millis());
    s_anim = 0;
    display_show_info(info);
    test_now_ms += 40;
    display_tick();
    require(has_text("AUTO"), "auto mode visible");
    display_navigate(UiEvent::right);
    test_now_ms += 40;
    display_tick();
    require(has_text("HOLD") && has_text("ROOM"), "arrow fixes selected page");
    require(has_text("NOT READ") && !has_text("CHECK SENSOR"),
            "sensor not yet sampled is not reported broken");
    info.env_ok = true;
    info.room_c = 27;
    info.humidity = 40;
    display_show_info(info);
    test_now_ms += 6000;
    display_tick();
    require(has_text("ROOM") && has_text("27"), "held page still refreshes live data");
    display_navigate(UiEvent::ok_long);
    test_now_ms += 40;
    display_tick();
    require(has_text("DIAG") && has_text("NETWORK"), "long opens network detail");
    require(has_text("WIFI UP  MQTT UP"), "separate connection states");
    require_layout();
    display_navigate(UiEvent::right);
    test_now_ms += 40;
    display_tick();
    require(has_text("NOT RECEIVED"), "unseen heartbeat not zero age");
    display_navigate(UiEvent::right);
    test_now_ms += 40;
    display_tick();
    require(has_text("NOT READ") && has_text("LAST GOOD --"), "unread sensor unknown");
    display_navigate(UiEvent::right);
    test_now_ms += 40;
    display_tick();
    require(has_text("UNARMED") && has_text("NO CONTACT"), "unarmed protection unknown age");
    display_navigate(UiEvent::right);
    test_now_ms += 40;
    display_tick();
    require(has_text(FW_GIT_HASH) && has_text(__TIME__), "boot details identify firmware");
    require_layout();
    display_navigate(UiEvent::ok_short);
    test_now_ms += 40;
    display_tick();
    require(has_text("DIAG"), "short OK does not leave diagnostics");
    display_navigate(UiEvent::ok_long);
    test_now_ms += 40;
    display_tick();
    require(has_text("ROOM") && has_text("HOLD"), "exit restores held overview");
    display_navigate(UiEvent::ok_short);
    test_now_ms += 4999;
    display_tick();
    require(has_text("ROOM") && has_text("AUTO"), "resume waits whole interval");
    test_now_ms += 41;
    display_tick();
    require(s_anim > 0, "resume retains slide animation");
    display_navigate(UiEvent::left);
    test_now_ms += 40;
    display_tick();
    require(s_anim == 0 && has_text("ROOM") && has_text("HOLD"), "key cancels slide");
    info.failsafe = true;
    info.failsafe_armed = true;
    info.heartbeat_seen = true;
    info.heartbeat_last_ms = UINT32_MAX - 1999;
    info.sensor_seen = true;
    info.sensor_last_ms = UINT32_MAX - 2999;
    info.sensor_good_seen = true;
    info.sensor_good_ms = UINT32_MAX - 4999;
    info.env_ok = false;
    info.last_contact_ms = UINT32_MAX - 3999;
    info.snapshot_ms = 1000;
    display_show_info(info);
    display_navigate(UiEvent::ok_long);
    for (int detail = 0; detail < 5; ++detail) {
        s_nav.detail = detail;
        s_dirty = true;
        test_now_ms += 40;
        display_tick();
        require_layout();
        require(has_text("SAFE OFF"), "every diagnostic keeps protection badge");
        if (detail == 1) require(has_text("LAST 3s"), "heartbeat age across wrap");
        if (detail == 2) {
            require(has_text("READ FAILED") && has_text("ATTEMPT 4s") && has_text("LAST GOOD 6s"),
                    "failed sensor shows attempt and last good ages, not stale values");
            info.snapshot_ms = 2000;
            display_show_info(info);
            test_now_ms += 40;
            display_tick();
            require(has_text("ATTEMPT 5s"), "diagnostic age refreshes on held detail");
        }
        if (detail == 3)
            require(has_text("TRIPPED") && has_text("LAST 6s"), "protection age and state");
    }
    const uint8_t old_page = s_nav.page;
    display_lines("AirCon Gateway", "starting...", "boot #12", "rst SW");
    display_navigate(UiEvent::right);
    test_now_ms += 10000;
    display_tick();
    require(has_text("boot #12") && s_nav.page == old_page, "manual screen ignores navigation");
    display_show_info(info);
    test_now_ms += 40;
    display_tick();
    require(!has_text("boot #12") && has_text("DIAG"), "handoff redraws even unchanged snapshot");
    s_ok = false;
    const uint8_t old_detail = s_nav.detail;
    display_navigate(UiEvent::right);
    require(s_nav.detail == old_detail, "headless navigation is harmless");
    s_ok = true;
    s_nav.diagnostics = true;
    for (int healthy : {0, 1}) {
        info.service_alive = healthy;
        info.failsafe = false;
        info.env_ok = healthy;
        info.wifi_connected = healthy;
        info.mqtt_connected = healthy;
        info.snapshot_ms = UINT32_MAX;
        info.heartbeat_last_ms = info.sensor_last_ms = info.sensor_good_ms = 0;
        info.last_contact_ms = 0;
        info.uptime_s = UINT32_MAX / 1000;
        display_show_info(info);
        for (int detail = 0; detail < 5; ++detail) {
            s_nav.detail = detail;
            s_dirty = true;
            test_now_ms += 40;
            display_tick();
            require_layout();
            if (detail == 0)
                require(has_text(healthy ? "WIFI UP  MQTT UP" : "WIFI DOWN  MQTT DOWN"),
                        "network diagnostics reflects disconnection");
            if (detail == 1)
                require(has_text(healthy ? "ALIVE" : "EXPIRED"),
                        "heartbeat alive and expired states");
            if (detail == 2)
                require(has_text(healthy ? "READ OK" : "READ FAILED"),
                        "sensor success and failure states");
            if (detail == 3)
                require(has_text("ARMED") && has_text(healthy ? "CONTACT OK" : "CONTACT LOST"),
                        "armed contact state stays observational");
        }
    }
}

static void buttons_tests() {
    for (int& level : test_gpio_level)
        level = HIGH;
    s_ok = true;
    s_nav.reset(millis());
    DisplayInfo info;
    info.ac.temp = 24;
    display_show_info(info);
    buttons_begin();
    for (int pin : {PIN_BUTTON_LEFT, PIN_BUTTON_OK, PIN_BUTTON_RIGHT})
        require(test_gpio_mode[pin] == INPUT_PULLUP, "all navigation inputs pulled up");
    for (int i = 0; i < 100; ++i) {
        test_now_ms += 10;
        buttons_loop();
    }
    require(s_nav.automatic && s_nav.page == 0, "unconnected buttons emit no events");
    auto sample = [](int pin, int level, uint32_t delta) {
        test_gpio_level[pin] = level;
        test_now_ms += delta;
        buttons_loop();
    };
    auto click = [&](int pin) {
        sample(pin, LOW, 10);
        sample(pin, LOW, 30);
        sample(pin, HIGH, 10);
        sample(pin, HIGH, 30);
    };
    click(PIN_BUTTON_RIGHT);
    test_now_ms += 40;
    display_tick();
    require(has_text("ROOM") && has_text("HOLD"), "GPIO right displays next held page");
    click(PIN_BUTTON_LEFT);
    test_now_ms += 40;
    display_tick();
    require(has_text("AIRCON"), "GPIO left displays previous page");
    click(PIN_BUTTON_OK);
    require(s_nav.automatic, "GPIO OK short resumes auto");
    sample(PIN_BUTTON_OK, LOW, 10);
    sample(PIN_BUTTON_OK, LOW, 30);
    sample(PIN_BUTTON_OK, LOW, 800);
    test_now_ms += 40;
    display_tick();
    require(has_text("NETWORK") && has_text("DIAG"), "GPIO long opens diagnostics");
    sample(PIN_BUTTON_OK, HIGH, 10);
    sample(PIN_BUTTON_OK, HIGH, 30);
    require(s_nav.diagnostics && s_nav.automatic, "long release never toggles mode");
    click(PIN_BUTTON_RIGHT);
    test_now_ms += 40;
    display_tick();
    require(has_text("HEARTBEAT"), "GPIO arrows switch diagnostics");
    test_gpio_level[PIN_BUTTON_OK] = LOW;
    buttons_begin();
    sample(PIN_BUTTON_OK, LOW, 2000);
    sample(PIN_BUTTON_OK, HIGH, 10);
    sample(PIN_BUTTON_OK, HIGH, 30);
    require(s_nav.diagnostics, "held during initialization not a phantom event");
    const uint8_t detail = s_nav.detail;
    test_gpio_level[PIN_BUTTON_LEFT] = LOW;
    sample(PIN_BUTTON_RIGHT, LOW, 10);
    sample(PIN_BUTTON_RIGHT, LOW, 900);
    sample(PIN_BUTTON_LEFT, HIGH, 10);
    sample(PIN_BUTTON_RIGHT, HIGH, 10);
    test_now_ms += 30;
    buttons_loop();
    require(s_nav.detail == detail, "multi-key chord ignored until all released");
    test_now_ms += 30;
    buttons_loop();
    test_gpio_level[PIN_BUTTON_LEFT] = LOW;
    sample(PIN_BUTTON_RIGHT, LOW, 10);
    test_gpio_level[PIN_BUTTON_LEFT] = HIGH;
    sample(PIN_BUTTON_RIGHT, HIGH, 10);
    sample(PIN_BUTTON_LEFT, LOW, 2);
    sample(PIN_BUTTON_LEFT, LOW, 30);
    sample(PIN_BUTTON_LEFT, HIGH, 8);
    sample(PIN_BUTTON_LEFT, HIGH, 30);
    require(s_nav.detail == detail, "chord release bounce cannot rearm navigation");
    // Stable release interval can also cross the 32-bit clock wrap.
    test_now_ms = UINT32_MAX - 20;
    test_gpio_level[PIN_BUTTON_LEFT] = LOW;
    sample(PIN_BUTTON_RIGHT, LOW, 0);
    test_gpio_level[PIN_BUTTON_LEFT] = HIGH;
    sample(PIN_BUTTON_RIGHT, HIGH, 10);
    test_now_ms += 29;
    buttons_loop();
    require(s_nav.detail == detail, "chord stays inert across clock wrap");
    test_now_ms += 1;
    buttons_loop();
    click(PIN_BUTTON_RIGHT);
    require(s_nav.detail == 2, "normal input resumes after chord");
    display_lines("AirCon Gateway", "starting...", "boot #12", "rst SW");
    click(PIN_BUTTON_RIGHT);
    test_now_ms += 40;
    display_tick();
    require(has_text("boot #12"), "physical key cannot steal manual screen");
    require(s_info.ac.temp == 24 && s_info.ac.power == info.ac.power,
            "input changes no AC snapshot values");
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
    s_nav.reset(millis());
    s_anim = 3;
    s_lastFrame = 0;
    display_tick();
    require(u8g2.clips >= 2, "sliding pages have independent clipping");
    display_lines("AirCon Gateway", "starting...", "boot #12", "rst SW");
    require(has_text("boot #12") && has_text("rst SW"), "boot diagnostics preserved");
    require(s_manual, "manual screens keep pager ownership");
    navigation_tests();
    buttons_tests();
    std::puts("PASS: display layout, persistent safety badge, clipping and diagnostics");
}
